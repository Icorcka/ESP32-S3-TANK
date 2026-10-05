# ESP32-S3-TANK — ESP32-S3 toy tank on ESP-IDF + FreeRTOS

Firmware for a toy tank: a tracked drivetrain on a **TB6612FNG** driver and a cannon whose
button is "pressed" by an **ST2222A** transistor. It grows a simple Arduino sketch into a
multitasking system on the **ESP-IDF** framework, with classes, FreeRTOS tasks, interrupts
and control from a **PS5 DualSense** gamepad.

## Features

- Three modes: **MANUAL** (operator in control), **DEMO** (the original sketch's behavior:
  drives forward and fires every 5 s), **E-STOP** (emergency stop).
- **DualSense** control through a phone or laptop browser over Wi-Fi — no wires to the tank.
  The on-board BOOT button toggles the emergency stop and demo mode.
- The browser remote shows link status, mode and gear, and has an on-screen E-STOP button.
- Smooth acceleration and braking, "gears" (speed limits) on the gamepad.
- **Failsafe**: if commands stop arriving (lost link, stuck task), the tank stops on its own.
- Cannon with a cooldown and a safety lock: it cannot fire in E-STOP.
- The tank logic is covered by 24 unit tests that run on a computer, no board needed.

## How a DualSense controls an ESP32-S3

The DualSense speaks **only Bluetooth Classic**, while the **ESP32-S3 has only BLE**, so they
can't pair directly. A phone or laptop sits in between:

```
DualSense ──Bluetooth──► phone / Mac: browser with the remote page
          ──Wi-Fi, WebSocket──► ESP32-S3 (WebRemote) ──► tank
```

The ESP32-S3 runs its own Wi-Fi network and serves the remote page. The browser reads the
gamepad through the standard Gamepad API and sends its state to the tank about 30 times per
second. Nothing needs to be installed.

### Step by step

1. Flash the board (see [Build, flash, test](#build-flash-test)) and power the tank on.
   The log shows `Network "Tank-S3" is up, remote: http://192.168.4.1/`.
2. Pair the DualSense with a phone or Mac over Bluetooth: hold **Create + PS** until the light
   bar blinks rapidly, then pick the gamepad in the Bluetooth settings.
3. Connect the same device to the **Tank-S3** Wi-Fi (password `tank12345`). This network has no
   internet — if the phone offers to disconnect, stay on it.
4. Open **http://192.168.4.1** in the browser and press any button on the gamepad:
   browsers expose a gamepad to a page only after a button press.
5. If the gamepad still doesn't show up, open `web/remote.html` on the Mac with a double-click.
   The browser treats a page from disk as secure, and it connects to the tank on its own.

While the remote tab is hidden or the screen is off, the page sends nothing and the tank
stops (failsafe). This is intentional.

The network name and password are `kWifi` in `src/config/TankConfig.h`.

## Wiring

| Signal | GPIO |
|---|---|
| TB6612 PWMA (motor A, left) | 1 |
| TB6612 AIN1 | 2 |
| TB6612 AIN2 | 21 |
| TB6612 PWMB (motor B, right) | 14 |
| TB6612 BIN1 | 12 |
| TB6612 BIN2 | 13 |
| ST2222A base (via ~1 kΩ) | 4 |
| Mode button | 0 (on-board BOOT) |
| Status LED (optional) | not connected |

Hardware tips:

- The TB6612 **STBY** pin must be pulled up to 3.3 V, otherwise the motors won't turn.
- The motors have their own supply (VM), but **GND is shared** with the ESP32.
- A **10 kΩ resistor between the ST2222A base and GND** keeps the cannon from firing while
  the ESP32 boots and the pin is still floating.
- A track spins the wrong way? Flip `kInvertLeftMotor` / `kInvertRightMotor` in
  `src/config/TankConfig.h` — no rewiring needed. Each motor is set independently.
- To add a status LED: an LED through a 220–330 Ω resistor on a free pin (e.g. GPIO 5)
  and `kStatusLedPin = GPIO_NUM_5` in `src/config/BoardPins.h`.

## Controls

**DualSense gamepad** (via the remote page):

| Input | Action |
|---|---|
| Left stick ↕ | throttle forward / reverse |
| Right stick ↔ | turn (spins in place when standing) |
| R2 (half pull) or ✕ | fire |
| ○ | E-STOP / release E-STOP |
| OPTIONS | toggle DEMO |
| L1 / R1 | gear down / up: 40 % → 70 % → 100 % speed |

**The remote page** shows the link to the tank, the gamepad, mode and gear. The on-screen
**E-STOP** button works from any connected device. One remote drives at a time; another takes
over only if the first goes silent for 1 s.

**BOOT button**: short press — E-STOP / release, long press (≥ 0.8 s) — DEMO.

**Status LED** (if connected): steady — MANUAL, slow blink — DEMO, fast blink — E-STOP.

## Build, flash, test

In VS Code open the repository folder (PlatformIO → **Open Project**). Or from a terminal:

```bash
pio run -t upload        # build and flash the ESP32-S3
pio device monitor       # debug log while the board is on the cable (115200)
pio test -e native       # logic unit tests on the computer
pio run -t menuconfig    # (optional) all ESP-IDF settings
```

The first build is slow (a few minutes): PlatformIO compiles the whole ESP-IDF — Wi-Fi,
TCP/IP, the HTTP server.

## Arduino → ESP-IDF

| Arduino | ESP-IDF | Where |
|---|---|---|
| `setup()` / `loop()` | `app_main()`, then only FreeRTOS tasks run | `src/main.cpp` |
| `pinMode`, `digitalWrite`, `digitalRead` | `gpio_config`, `gpio_set_level`, `gpio_get_level` | `Motor`, `Cannon`, `PushButton` |
| `analogWrite` / `ledcWrite` | LEDC: `ledc_timer_config`, `ledc_channel_config`, `ledc_set_duty` + `ledc_update_duty` | `Motor` |
| `attachInterrupt` | `gpio_install_isr_service` + `gpio_isr_handler_add` | `PushButton` |
| `millis()` | `esp_timer_get_time() / 1000` | `util/Clock.h` |
| `Serial.print` | `ESP_LOGI` (thread-safe logging) | everywhere |
| `WiFi.softAP` | `esp_netif` + `esp_wifi` in `WIFI_MODE_AP` | `WifiAccessPoint` |
| `WebServer` + a WebSockets library | built-in `esp_http_server` with WebSocket — one port, no external libraries | `WebRemote` |

## Architecture

```
 ┌──────────────── Command sources (Adapter pattern) ──────────────┐
 │  WebRemote (Wi-Fi, httpd task)                 PushButton (BOOT) │
 │  └─ GamepadMapper: gamepad state → commands                      │
 └────────┬───────────────────────────────────────────────┬────────┘
          │                                               │ ISR → software timer
          └─────────────────── tank::Command ─────────────┘
                                        ▼
                 ┌─────────────── FreeRTOS queue ─────────────┐
                 │ TankController (control task)              │
                 │   └─ TankStateMachine                      │──► observers: ModeLogger,
                 │      MANUAL / DEMO / E-STOP                │    WebRemote, StatusLed
                 └─────────┬──────────────────────┬───────────┘
                     IDrive│                      │ICannon
                           ▼                      ▼
            DriveSystem (drive task)          Cannon (cannon task)
            mailbox, failsafe,                task notification,
            smooth ramp, 100 Hz               150 ms pulse, cooldown
                           │                      │
                 Motor × 2 (LEDC, TB6612FNG)   GPIO → ST2222A → module button
```

The code has two layers:

- **`lib/TankCore`** — the tank logic: state machine, commands, drive math, gamepad mapping and
  the remote protocol. It depends on neither ESP-IDF nor FreeRTOS and talks to the world through
  "port" interfaces (`IDrive`, `ICannon`, `ICommandSink`, `ITankObserver`), so it is tested on
  a computer.
- **`src/`** — "adapters" to the real world: ESP-IDF drivers, FreeRTOS tasks, Wi-Fi and the remote.

### State machine

| Command ↓ / Mode → | MANUAL | DEMO | E-STOP |
|---|---|---|---|
| Drive | drives | ignored — the autopilot drives | ignored |
| Fire | fires | fires | ignored, cannon locked |
| ToggleDemo | → DEMO | → MANUAL | ignored |
| ToggleEmergencyStop | → E-STOP | → E-STOP | → MANUAL |

After E-STOP the tank always returns to MANUAL, never to DEMO — it's safer.

### Patterns

| Pattern | Where | Why |
|---|---|---|
| **State** | `ManualState`, `DemoState`, `EmergencyStopState` | each mode's behavior is its own class; a new mode is a new class, not another `if` |
| **Command** | `tank::Command` | input devices only create commands and don't know who executes them |
| **Active Object** | `DriveSystem`, `Cannon`, `TankController` (base `RtosTask`) | each service owns a task; other tasks only send it requests |
| **Observer** | `ITankObserver` → `ModeLogger`, `WebRemote`, `StatusLed` | react to mode changes without coupling to the state machine |
| **Adapter** | `WebRemote`, `PushButton`, `Motor` | translate a device's "language" into tank commands; the gamepad layout lives in `GamepadMapper` |
| **Ports & Adapters** | `lib/TankCore` ↔ `src/` | the logic doesn't depend on hardware or the framework, so it's unit-tested |
| **Dependency Injection** | `src/main.cpp` (Composition Root) | all dependencies are passed through constructors in one place |

### FreeRTOS

| Task | Priority | Core | Runs | Does |
|---|---|---|---|---|
| `drive` | 5 | 1 | every 10 ms (`vTaskDelayUntil`) | failsafe, smooth ramp, motor PWM |
| `control` | 4 | 1 | on each command + every 50 ms | command queue → state machine |
| `cannon` | 3 | 1 | on a fire request | 150 ms pulse, cooldown |
| `httpd` (ESP-IDF) | 5 | 0 | on browser requests | remote page, WebSocket, gamepad state → commands |
| `Tmr Svc` (system) | 1 | — | on timers | button debounce, LED blinking |

Core 1 runs tank control, core 0 runs Wi-Fi and the HTTP server. The FreeRTOS tick is 1 ms
(`sdkconfig.defaults`). Synchronization:

| Mechanism | Where | Why |
|---|---|---|
| Queue | `TankController` | commands from different tasks and timers land in one task, so the state machine needs no locks |
| `xQueueSendToFront` | `TankController::post` | E-STOP overtakes drive commands in the queue |
| Mailbox (1-item queue + `xQueueOverwrite`) | `DriveSystem` | the mailbox always holds only the latest drive target |
| Task notification (`xTaskNotifyGive`) | `Cannon` | the lightest "semaphore": several requests in a row give one shot |
| Work in another task (`httpd_queue_work`) | `WebRemote` | remote state lives only in the HTTP server task; the controller hands it the mode broadcast instead of locking |
| Critical section (`portMUX`) | `Cannon` | "check the lock + press" is atomic across tasks and cores |
| `std::atomic` | `StatusLed`, `WebRemote` | the observer only stores the mode; its own task or timer displays it |
| Software timers | `PushButton`, `StatusLed` | debounce and blinking without dedicated tasks |

### Interrupts

The BOOT button runs on a hardware interrupt (both edges, `GPIO_INTR_ANYEDGE`):

```
edge on GPIO0 ──► ISR (IRAM_ATTR) ──► xTimerResetFromISR ──► portYIELD_FROM_ISR
                                              │
                     level stable for 30 ms   ▼
               timer callback: reads the pin, measures the press
                                              │
                          short / long ──► command to the controller queue
```

The handler is registered through the ESP-IDF GPIO interrupt service (`gpio_install_isr_service`
+ `gpio_isr_handler_add`). The ISR does the bare minimum and calls only `FromISR` functions:
no logging, delays or allocation. The rest happens outside the interrupt (deferred interrupt
processing). Every contact bounce restarts the timer, so the event fires only once.

## Safety

- **Drive failsafe**: a drive target is valid for 0.5 s. With no new commands the tank stops.
- The browser remote disconnected (tab closed, left the Wi-Fi) — the tank stops immediately.
  The page is hidden or frozen — it stops after 0.5 s.
- Malformed remote frames are dropped, and invalid numbers (NaN) can't spin up the motors.
- **E-STOP** stops the motors instantly (no smooth braking) and locks the cannon.
- The lock check and the cannon "press" run in a critical section, so an E-STOP can't slip
  in between.
- The cannon pin becomes an output already driven low — no pulse at boot.
- After power-on the tank stands still in MANUAL. To have it drive off like the original sketch,
  set `kInitialMode = tank::TankMode::Demo` in `TankConfig.h`.

## Compared with the original sketch

| Original Arduino sketch | ESP32-S3-TANK |
|---|---|
| `analogWrite(pinPWMA, 220)` in `setup()` | `Motor` + `DriveSystem`: 20 kHz PWM (LEDC), smooth ramp, reverse, brake |
| a shot every 5 s in `loop()` via `millis()` | DEMO mode (`DemoState`), interval set in `TankConfig.h` |
| `firePulseDuration = 150` | `Cannon`: 150 ms pulse in its own task + cooldown |
| `digitalWrite(pinFire, LOW)` at startup | `cannon.begin()` — the very first line of `app_main()` |
| global variables and constants | classes with settings in `src/config/` |

## Configuration

- `src/config/BoardPins.h` — pins.
- `src/config/TankConfig.h` — everything else: motor PWM frequency and minimum duty, motor
  inversion, acceleration, cannon and DEMO timings, gamepad mapping (`kGamepadMapping`:
  deadzone, R2 threshold, gears), Wi-Fi (`kWifi`), remote (`kWebRemote`), task priorities
  and stacks.
- `sdkconfig.defaults` — ESP-IDF settings: 1 ms FreeRTOS tick, WebSocket in the HTTP server,
  app partition size. After changing it, delete the generated `sdkconfig.esp32s3`.

If the tank hums but doesn't move at low speed, increase `kMotorPwm.minDuty`.

## Project layout

```
ESP32-S3-TANK/
├── platformio.ini              esp32s3 (ESP-IDF) / native (tests) environments
├── CMakeLists.txt              ESP-IDF project
├── sdkconfig.defaults          ESP-IDF settings
├── web/remote.html             browser remote (embedded into the firmware)
├── lib/TankCore/src/           hardware-free logic (tested on a PC)
│   ├── TankTypes.h             modes, commands, track speeds
│   ├── TankPorts.h             IDrive, ICannon, ICommandSink, ITankObserver interfaces
│   ├── DriveMath.h             deadzone, throttle + turn mixing, smooth ramp
│   ├── GamepadMapper.*         gamepad state → commands
│   ├── RemoteProtocol.*        remote text frames (parsing and validation)
│   └── TankStateMachine.*      state machine (State pattern)
├── src/
│   ├── CMakeLists.txt          main ESP-IDF component
│   ├── main.cpp                app_main — Composition Root: creates and wires objects
│   ├── config/                 BoardPins.h, TankConfig.h
│   ├── drivers/Motor.*         TB6612FNG channel (LEDC + GPIO)
│   ├── services/               DriveSystem, Cannon, TankController — FreeRTOS tasks
│   ├── input/                  WebRemote (Wi-Fi remote), PushButton (ISR)
│   ├── net/WifiAccessPoint.*   the tank's Wi-Fi network
│   ├── feedback/               ModeLogger, StatusLed — observers
│   └── util/                   RtosTask (active object base), Clock
└── test/test_tank_core/        unit tests (Unity)
```
