#include "input/WebRemote.h"

#include <RemoteProtocol.h>
#include <esp_log.h>
#include <string.h>
#include <unistd.h>

#include "util/Clock.h"

#if !CONFIG_HTTPD_WS_SUPPORT
#error "Set CONFIG_HTTPD_WS_SUPPORT=y in sdkconfig.defaults — the remote needs WebSocket"
#endif

extern const char kRemotePage[] asm("_binary_remote_html_start");

namespace {

constexpr const char* kTag = "WEB";
constexpr size_t kMaxFrameLength = 64;
constexpr size_t kMaxSockets = 7;

httpd_ws_frame_t textFrame(const char* text) {
  httpd_ws_frame_t frame{};
  frame.type = HTTPD_WS_TYPE_TEXT;
  frame.payload = reinterpret_cast<uint8_t*>(const_cast<char*>(text));
  frame.len = strlen(text);
  return frame;
}

}  // namespace

WebRemote::WebRemote(tank::ICommandSink& sink, const Settings& settings,
                     const tank::GamepadMapper::Settings& mapping)
    : sink_(sink), settings_(settings), mapper_(sink, tank::CommandSource::Web, mapping) {}

bool WebRemote::begin() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = settings_.port;
  config.max_open_sockets = kMaxSockets;
  config.lru_purge_enable = true;
  config.core_id = 0;

  config.keep_alive_enable = true;
  config.keep_alive_idle = 2;
  config.keep_alive_interval = 1;
  config.keep_alive_count = 3;
  config.global_user_ctx = this;
  config.global_user_ctx_free_fn = [](void*) {};
  config.close_fn = &WebRemote::onSocketClosed;

  httpd_handle_t server = nullptr;
  if (httpd_start(&server, &config) != ESP_OK) {
    ESP_LOGE(kTag, "Failed to start the HTTP server");
    return false;
  }

  httpd_uri_t page{};
  page.uri = "/";
  page.method = HTTP_GET;
  page.handler = &WebRemote::onPage;
  page.user_ctx = this;

  httpd_uri_t socket{};
  socket.uri = "/ws";
  socket.method = HTTP_GET;
  socket.handler = &WebRemote::onSocket;
  socket.user_ctx = this;
  socket.is_websocket = true;

  if (httpd_register_uri_handler(server, &page) != ESP_OK ||
      httpd_register_uri_handler(server, &socket) != ESP_OK) {
    ESP_LOGE(kTag, "Failed to register the remote's handlers");
    return false;
  }
  server_ = server;
  return true;
}

void WebRemote::onModeChanged(tank::TankMode mode) {
  mode_ = mode;
  if (httpd_handle_t server = server_.load()) {
    httpd_queue_work(server, &WebRemote::broadcastStatusWork, this);
  }
}

esp_err_t WebRemote::onPage(httpd_req_t* request) {
  httpd_resp_set_type(request, "text/html; charset=utf-8");
  return httpd_resp_send(request, kRemotePage, HTTPD_RESP_USE_STRLEN);
}

esp_err_t WebRemote::onSocket(httpd_req_t* request) {
  auto* self = static_cast<WebRemote*>(request->user_ctx);
  const int client = httpd_req_to_sockfd(request);

  if (request->method == HTTP_GET) {
    ESP_LOGI(kTag, "Remote #%d connected", client);
    char status[kStatusFrameSize];
    if (self->formatStatus(status)) {
      httpd_ws_frame_t frame = textFrame(status);
      httpd_ws_send_frame(request, &frame);
    }
    return ESP_OK;
  }

  httpd_ws_frame_t frame{};
  if (httpd_ws_recv_frame(request, &frame, 0) != ESP_OK) {
    return ESP_FAIL;
  }
  char text[kMaxFrameLength];
  if (frame.type != HTTPD_WS_TYPE_TEXT || frame.len >= sizeof(text)) {
    return ESP_FAIL;
  }
  frame.payload = reinterpret_cast<uint8_t*>(text);
  if (frame.len > 0 && httpd_ws_recv_frame(request, &frame, frame.len) != ESP_OK) {
    return ESP_FAIL;
  }
  text[frame.len] = '\0';
  self->handleFrame(client, text);
  return ESP_OK;
}

void WebRemote::onSocketClosed(httpd_handle_t server, int socket) {
  auto* self = static_cast<WebRemote*>(httpd_get_global_user_ctx(server));
  if (socket == self->pilot_) {
    ESP_LOGI(kTag, "Remote #%d disconnected", socket);
    self->releasePilot();
  }
  close(socket);
}

void WebRemote::broadcastStatusWork(void* self) {
  static_cast<WebRemote*>(self)->broadcastStatus();
}

void WebRemote::handleFrame(int client, const char* frame) {
  if (strcmp(frame, tank::remote::kEmergencyStopFrame) == 0) {
    sink_.post(tank::Command::toggleEmergencyStop(tank::CommandSource::Web));
    return;
  }

  tank::GamepadState state;
  if (!tank::remote::parseGamepadFrame(frame, state)) {
    return;
  }
  const uint32_t now = uptimeMs();
  if (client != pilot_) {
    const bool pilotSilent =
        pilot_ == kNoPilot || now - lastPilotFrameMs_ > settings_.pilotTimeoutMs;
    if (!pilotSilent) {
      return;
    }
    pilot_ = client;
    mapper_.reset(state);
    ESP_LOGI(kTag, "Remote #%d is driving", client);
  }
  lastPilotFrameMs_ = now;

  const uint8_t gearBefore = mapper_.gear();
  mapper_.update(state);
  if (mapper_.gear() != gearBefore) {
    ESP_LOGI(kTag, "Gear %u: up to %d%% speed", mapper_.gear() + 1u,
             static_cast<int>(mapper_.speedLimit() * 100.0f + 0.5f));
    broadcastStatus();
  }
}

void WebRemote::releasePilot() {
  pilot_ = kNoPilot;
  mapper_.stop();
}

bool WebRemote::formatStatus(char (&frame)[kStatusFrameSize]) const {
  return tank::remote::formatStatusFrame(frame, sizeof(frame), mode_.load(),
                                         mapper_.gear() + 1u) > 0;
}

void WebRemote::broadcastStatus() {
  httpd_handle_t server = server_.load();
  char status[kStatusFrameSize];
  if (server == nullptr || !formatStatus(status)) {
    return;
  }
  size_t count = kMaxSockets;
  int clients[kMaxSockets];
  if (httpd_get_client_list(server, &count, clients) != ESP_OK) {
    return;
  }
  httpd_ws_frame_t frame = textFrame(status);
  for (size_t i = 0; i < count; ++i) {
    if (httpd_ws_get_fd_info(server, clients[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
      httpd_ws_send_frame_async(server, clients[i], &frame);
    }
  }
}
