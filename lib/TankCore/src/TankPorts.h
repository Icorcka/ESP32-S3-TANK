#pragma once

#include "TankTypes.h"

namespace tank {

class IDrive {
 public:
  virtual ~IDrive() = default;

  virtual void setTarget(const TrackSpeeds& target) = 0;

  virtual void emergencyStop() = 0;
};

class ICannon {
 public:
  virtual ~ICannon() = default;

  virtual void fire() = 0;
  virtual void setLocked(bool locked) = 0;
};

class ITankObserver {
 public:
  virtual ~ITankObserver() = default;
  virtual void onModeChanged(TankMode mode) = 0;
};

class ICommandSink {
 public:
  virtual ~ICommandSink() = default;

  virtual bool post(const Command& command) = 0;
};

}  // namespace tank
