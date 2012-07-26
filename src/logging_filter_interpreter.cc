// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/logging_filter_interpreter.h"

#include <errno.h>
#include <fcntl.h>
#include <string>

#include <base/file_util.h>
#include <base/file_path.h>
#include <base/values.h>

#include "gestures/include/logging.h"

namespace gestures {

LoggingFilterInterpreter::LoggingFilterInterpreter(PropRegistry* prop_reg,
                                                   Interpreter* next)
    : FilterInterpreter(prop_reg),
      logging_notify_(prop_reg, "Logging Notify", 0, this),
      logging_reset_(prop_reg, "Logging Reset", 0, this) {
  next_.reset(next);
  if (prop_reg)
    prop_reg->set_activity_log(&log_);
}

LoggingFilterInterpreter::~LoggingFilterInterpreter() {}

Gesture* LoggingFilterInterpreter::SyncInterpret(HardwareState* hwstate,
                                                 stime_t* timeout) {
  Gesture* result = next_->SyncInterpret(hwstate, timeout);
  return result;
}

Gesture* LoggingFilterInterpreter::HandleTimer(stime_t now, stime_t* timeout) {
  Gesture* result = next_->HandleTimer(now, timeout);
  return result;
}

void LoggingFilterInterpreter::SetHardwareProperties(
    const HardwareProperties& hwprops) {
  log_.SetHardwareProperties(hwprops);
  next_->SetHardwareProperties(hwprops);
}

void LoggingFilterInterpreter::IntWasWritten(IntProperty* prop) {
  if (prop == &logging_notify_)
    Dump("/var/log/touchpad_activity_log.txt");
  if (prop == &logging_reset_)
    Clear();
};

std::string LoggingFilterInterpreter::EncodeActivityLog() {
  return Encode();
}

void LoggingFilterInterpreter::Dump(const char* filename) {
  std::string data = Encode();
  std::string fn(filename);
  FilePath fp(fn);
  file_util::WriteFile(fp, data.c_str(), data.size());
}
}  // namespace gestures
