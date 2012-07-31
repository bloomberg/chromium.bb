// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/interpreter.h"

#include <string>

#include <base/json/json_writer.h>
#include <base/values.h>

#include "gestures/include/gestures.h"

namespace gestures {

Gesture* Interpreter::SyncInterpret(HardwareState* hwstate,
                                    stime_t* timeout) {
  if (hwstate)
    log_.LogHardwareState(*hwstate);
  Gesture* result = SyncInterpretImpl(hwstate, timeout);
  LogOutputs(result, timeout);
  return result;
}

Gesture* Interpreter::HandleTimer(stime_t now, stime_t* timeout) {
  log_.LogTimerCallback(now);
  Gesture* result = HandleTimerImpl(now, timeout);
  LogOutputs(result, timeout);
  return result;
}

void Interpreter::SetHardwareProperties(const HardwareProperties& hwprops) {
  log_.SetHardwareProperties(hwprops);
  SetHardwarePropertiesImpl(hwprops);
}

std::string Interpreter::Encode() {
  DictionaryValue *root;
  root = EncodeCommonInfo();
  root = log_.AddEncodeInfo(root);

  std::string out;
  base::JSONWriter::Write(root, true, &out);
  return out;
}

void Interpreter::LogOutputs(Gesture* result, stime_t* timeout) {
  if (result)
    log_.LogGesture(*result);
  if (timeout && *timeout >= 0.0)
    log_.LogCallbackRequest(*timeout);
}
}  // namespace gestures
