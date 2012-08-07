// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/interpreter.h"

#include <cxxabi.h>
#include <string>

#include <base/json/json_writer.h>
#include <base/values.h>

#include "gestures/include/activity_log.h"
#include "gestures/include/gestures.h"
#include "gestures/include/logging.h"

namespace gestures {

Interpreter::Interpreter(PropRegistry* prop_reg) : log_(prop_reg) {
#ifdef DEEP_LOGS
  logging_enabled_ = true;
#else
  logging_enabled_ = false;
#endif
}

Interpreter::Interpreter() : log_(NULL) {
#ifdef DEEP_LOGS
  logging_enabled_ = true;
#else
  logging_enabled_ = false;
#endif
}

Gesture* Interpreter::SyncInterpret(HardwareState* hwstate,
                                    stime_t* timeout) {
  if (logging_enabled_ && hwstate)
    log_.LogHardwareState(*hwstate);
  Gesture* result = SyncInterpretImpl(hwstate, timeout);
  if (logging_enabled_)
    LogOutputs(result, timeout);
  return result;
}

Gesture* Interpreter::HandleTimer(stime_t now, stime_t* timeout) {
  if (logging_enabled_)
    log_.LogTimerCallback(now);
  Gesture* result = HandleTimerImpl(now, timeout);
  if (logging_enabled_)
    LogOutputs(result, timeout);
  return result;
}

void Interpreter::SetHardwareProperties(const HardwareProperties& hwprops) {
  if (logging_enabled_)
    log_.SetHardwareProperties(hwprops);
  SetHardwarePropertiesImpl(hwprops);
}

DictionaryValue* Interpreter::EncodeCommonInfo() {
  DictionaryValue* root = log_.EncodeCommonInfo();
  root->Set(ActivityLog::kKeyInterpreterName, new StringValue(GetName()));
  return root;
}

std::string Interpreter::Encode() {
  DictionaryValue *root;
  root = EncodeCommonInfo();
  root = log_.AddEncodeInfo(root);

  std::string out;
  base::JSONWriter::Write(root, true, &out);
  return out;
}

std::string Interpreter::GetName() {
  if (name_.empty()) {
    int status;
    char* full_name = abi::__cxa_demangle(typeid(*this).name(), 0, 0, &status);
    if (full_name == NULL) {
      if (status == -1)
        Err("Memory allocation failed");
      else if (status == -2)
        Err("Mangled_name is not a valid name");
      else if (status == -3)
        Err("One of the arguments is invalid");
      return name_;
    }
    // the return value of abi::__cxa_demangle(...) is gestures::XXXInterpreter
    char* last_colon = strrchr(full_name, ':');
    char* class_name;
    if (last_colon)
      class_name = last_colon + 1;
    else
      class_name = full_name;
    name_ = std::string(class_name);
    free(full_name);
  }
  return name_;
}

void Interpreter::LogOutputs(Gesture* result, stime_t* timeout) {
  if (result)
    log_.LogGesture(*result);
  if (timeout && *timeout >= 0.0)
    log_.LogCallbackRequest(*timeout);
}
}  // namespace gestures
