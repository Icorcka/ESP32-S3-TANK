#pragma once

#include <GamepadMapper.h>
#include <TankPorts.h>
#include <esp_http_server.h>
#include <stddef.h>
#include <stdint.h>

#include <atomic>

class WebRemote final : public tank::ITankObserver {
 public:
  struct Settings {
    uint16_t port;
    uint32_t pilotTimeoutMs;
  };

  WebRemote(tank::ICommandSink& sink, const Settings& settings,
            const tank::GamepadMapper::Settings& mapping);

  bool begin();

  void onModeChanged(tank::TankMode mode) override;

 private:
  static constexpr int kNoPilot = -1;
  static constexpr size_t kStatusFrameSize = 24;

  static esp_err_t onPage(httpd_req_t* request);
  static esp_err_t onSocket(httpd_req_t* request);
  static void onSocketClosed(httpd_handle_t server, int socket);
  static void broadcastStatusWork(void* self);

  void handleFrame(int client, const char* frame);
  void releasePilot();
  bool formatStatus(char (&frame)[kStatusFrameSize]) const;
  void broadcastStatus();

  tank::ICommandSink& sink_;
  Settings settings_;
  tank::GamepadMapper mapper_;
  int pilot_ = kNoPilot;
  uint32_t lastPilotFrameMs_ = 0;
  std::atomic<httpd_handle_t> server_{nullptr};
  std::atomic<tank::TankMode> mode_{tank::TankMode::Manual};
};
