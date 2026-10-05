#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TankPorts.h"
#include "TankTypes.h"

namespace tank {

struct DemoSettings {
  float speed;
  uint32_t fireIntervalMs;
};

class TankContext {
 public:
  virtual IDrive& drive() = 0;
  virtual ICannon& cannon() = 0;
  virtual void transitionTo(TankMode mode, uint32_t nowMs) = 0;

 protected:
  ~TankContext() = default;
};

class TankState {
 public:
  virtual ~TankState() = default;
  virtual TankMode mode() const = 0;
  virtual void onEnter(TankContext& /*tank*/, uint32_t /*nowMs*/) {}
  virtual void onExit(TankContext& /*tank*/) {}
  virtual void onCommand(TankContext& tank, const Command& command, uint32_t nowMs) = 0;
  virtual void onTick(TankContext& /*tank*/, uint32_t /*nowMs*/) {}
};

class ManualState final : public TankState {
 public:
  TankMode mode() const override { return TankMode::Manual; }
  void onEnter(TankContext& tank, uint32_t nowMs) override;
  void onCommand(TankContext& tank, const Command& command, uint32_t nowMs) override;
};

class DemoState final : public TankState {
 public:
  explicit DemoState(const DemoSettings& settings) : settings_(settings) {}

  TankMode mode() const override { return TankMode::Demo; }
  void onEnter(TankContext& tank, uint32_t nowMs) override;
  void onExit(TankContext& tank) override;
  void onCommand(TankContext& tank, const Command& command, uint32_t nowMs) override;
  void onTick(TankContext& tank, uint32_t nowMs) override;

 private:
  void driveForward(TankContext& tank) const;

  DemoSettings settings_;
  uint32_t lastShotMs_ = 0;
};

class EmergencyStopState final : public TankState {
 public:
  TankMode mode() const override { return TankMode::EmergencyStop; }
  void onEnter(TankContext& tank, uint32_t nowMs) override;
  void onExit(TankContext& tank) override;
  void onCommand(TankContext& tank, const Command& command, uint32_t nowMs) override;
};

class TankStateMachine final : private TankContext {
 public:
  static constexpr size_t kMaxObservers = 4;

  TankStateMachine(IDrive& drive, ICannon& cannon, const DemoSettings& demo);
  TankStateMachine(const TankStateMachine&) = delete;
  TankStateMachine& operator=(const TankStateMachine&) = delete;

  bool addObserver(ITankObserver& observer);

  void start(TankMode initialMode, uint32_t nowMs);
  void handle(const Command& command, uint32_t nowMs);
  void tick(uint32_t nowMs);
  TankMode mode() const { return state_->mode(); }

 private:
  IDrive& drive() override { return drive_; }
  ICannon& cannon() override { return cannon_; }
  void transitionTo(TankMode mode, uint32_t nowMs) override;

  TankState& stateFor(TankMode mode);
  void notifyObservers();

  IDrive& drive_;
  ICannon& cannon_;

  ManualState manual_;
  DemoState demo_;
  EmergencyStopState emergencyStop_;
  TankState* state_ = &manual_;

  ITankObserver* observers_[kMaxObservers] = {};
  size_t observerCount_ = 0;
};

}  // namespace tank
