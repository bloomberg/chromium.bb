// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/gestures.h"

#include <sys/time.h>

#include <base/logging.h>

#include "gestures/include/immediate_interpreter.h"
#include "gestures/include/integral_gesture_filter_interpreter.h"
#include "gestures/include/scaling_filter_interpreter.h"

// C API:

static const int kMinSupportedVersion = 1;
static const int kMaxSupportedVersion = 1;

stime_t StimeFromTimeval(const struct timeval* tv) {
  return static_cast<stime_t>(tv->tv_sec) +
      static_cast<stime_t>(tv->tv_usec) / 1000000.0;
}

FingerState* HardwareState::GetFingerState(short tracking_id) {
  return const_cast<FingerState*>(
      const_cast<const HardwareState*>(this)->GetFingerState(tracking_id));
}

const FingerState* HardwareState::GetFingerState(short tracking_id) const {
  for (short i = 0; i < finger_cnt; i++) {
    if (fingers[i].tracking_id == tracking_id)
      return &fingers[i];
  }
  return NULL;
}

GestureInterpreter* NewGestureInterpreterImpl(int version) {
  if (version < kMinSupportedVersion) {
    LOG(ERROR) << "Client too old. It's using version " << version
               << ", but library has min supported version "
               << kMinSupportedVersion;
    return NULL;
  }
  if (version > kMaxSupportedVersion) {
    LOG(ERROR) << "Client too new. It's using version " << version
               << ", but library has max supported version "
               << kMaxSupportedVersion;
    return NULL;
  }
  return new gestures::GestureInterpreter(version);
}

void DeleteGestureInterpreter(GestureInterpreter* obj) {
  delete obj;
}

void GestureInterpreterPushHardwareState(GestureInterpreter* obj,
                                         struct HardwareState* hwstate) {
  obj->PushHardwareState(hwstate);
}

void GestureInterpreterSetHardwareProperties(
    GestureInterpreter* obj,
    const struct HardwareProperties* hwprops) {
  obj->SetHardwareProperties(*hwprops);
}

void GestureInterpreterSetCallback(GestureInterpreter* obj,
                                   GestureReadyFunction fn,
                                   void* user_data) {
  obj->set_callback(fn, user_data);
}

void GestureInterpreterSetTapToClickEnabled(GestureInterpreter* obj,
                                            int enabled) {
  obj->set_tap_to_click(enabled != 0);
}

void GestureInterpreterSetMovementSpeed(GestureInterpreter* obj,
                                        int speed) {
  obj->set_move_speed(speed);
}

void GestureInterpreterSetScrollSpeed(GestureInterpreter* obj,
                                      int speed) {
  obj->set_scroll_speed(speed);
}

// C++ API:

GestureInterpreter::GestureInterpreter(int version)
    : callback_(NULL),
      callback_data_(NULL),
      tap_to_click_(false),
      move_speed_(50),
      scroll_speed_(50) {
  interpreter_.reset(new IntegralGestureFilterInterpreter(
      new ScalingFilterInterpreter(new ImmediateInterpreter)));
}

GestureInterpreter::~GestureInterpreter() {}

void GestureInterpreter::PushHardwareState(HardwareState* hwstate) {
  Gesture* gs = interpreter_->SyncInterpret(hwstate);
  if (gs && callback_) {
    (*callback_)(callback_data_, gs);
  }
}

void GestureInterpreter::SetHardwareProperties(
    const HardwareProperties& hwprops) {
  interpreter_->SetHardwareProperties(hwprops);
}
