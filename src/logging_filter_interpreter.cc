// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/logging_filter_interpreter.h"

#include "gestures/include/logging.h"

namespace gestures {

LoggingFilterInterpreter::LoggingFilterInterpreter(PropRegistry* prop_reg,
                                                   Interpreter* next)
  : logging_notify_(prop_reg, "Logging Notify", 0, this) {
  next_.reset(next);
}

LoggingFilterInterpreter::~LoggingFilterInterpreter() {}

Gesture* LoggingFilterInterpreter::SyncInterpret(HardwareState* hwstate,
                                                 stime_t* timeout) {
  log_.LogHardwareState(*hwstate);
  Gesture* result = next_->SyncInterpret(hwstate, timeout);
  LogOutputs(result, timeout);
  return result;
}

Gesture* LoggingFilterInterpreter::HandleTimer(stime_t now, stime_t* timeout) {
  log_.LogTimerCallback(now);
  Gesture* result = next_->HandleTimer(now, timeout);
  LogOutputs(result, timeout);
  return result;
}

void LoggingFilterInterpreter::LogOutputs(Gesture* result, stime_t* timeout) {
  if (result)
    log_.LogGesture(*result);
  if (*timeout >= 0.0)
    log_.LogCallbackRequest(*timeout);
}

void LoggingFilterInterpreter::SetHardwareProperties(
    const HardwareProperties& hwprops) {
  log_.SetHardwareProperties(hwprops);
  next_->SetHardwareProperties(hwprops);
}

void LoggingFilterInterpreter::IntWasWritten(IntProperty* prop) {
  if (prop == &logging_notify_)
    log_.Dump("/var/log/touchpad_activity_log.txt");
};

}  // namespace gestures
