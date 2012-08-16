// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/apple_trackpad_filter_interpreter.h"
#include "gestures/include/tracer.h"

#include <base/memory/scoped_ptr.h>

namespace gestures {

AppleTrackpadFilterInterpreter::AppleTrackpadFilterInterpreter(
    PropRegistry* prop_reg, Interpreter* next, Tracer* tracer)
    : FilterInterpreter(next, tracer),
      enabled_(prop_reg, "Apple TP Filter Enable", 0) {
  InitName();
}

Gesture* AppleTrackpadFilterInterpreter::SyncInterpretImpl(
    HardwareState* hwstate, stime_t* timeout) {
  if (enabled_.val_) {
    for (size_t i = 0; i < hwstate->finger_cnt; i++)
      hwstate->fingers[i].pressure = hwstate->fingers[i].touch_major;
  }
  return next_->SyncInterpret(hwstate, timeout);
}

}  // namespace gestures
