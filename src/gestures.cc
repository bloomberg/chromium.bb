// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/gestures.h"

#include <sys/time.h>

#include "gestures/include/accel_filter_interpreter.h"
#include "gestures/include/immediate_interpreter.h"
#include "gestures/include/integral_gesture_filter_interpreter.h"
#include "gestures/include/logging.h"
#include "gestures/include/lookahead_filter_interpreter.h"
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
    Err("Client too old. It's using version %d"
        ", but library has min supported version %d",
        version,
        kMinSupportedVersion);
    return NULL;
  }
  if (version > kMaxSupportedVersion) {
    Err("Client too new. It's using version %d"
        ", but library has max supported version %d",
        version,
        kMaxSupportedVersion);
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

void GestureInterpreterSetTimerProvider(GestureInterpreter* obj,
                                        GesturesTimerProvider* tp,
                                        void* data) {
  obj->SetTimerProvider(tp, data);
}

void GestureInterpreterSetPropProvider(GestureInterpreter* obj,
                                       GesturesPropProvider* pp,
                                       void* data) {
  obj->SetPropProvider(pp, data);
}

// C++ API:

GestureInterpreter::GestureInterpreter(int version)
    : callback_(NULL),
      callback_data_(NULL),
      timer_provider_(NULL),
      timer_provider_data_(NULL),
      interpret_timer_(NULL),
      prop_provider_(NULL),
      prop_provider_data_(NULL) {
  interpreter_.reset(new IntegralGestureFilterInterpreter(
      new ScalingFilterInterpreter(
          new AccelFilterInterpreter(
              new LookaheadFilterInterpreter(
                  new ImmediateInterpreter)))));
}

GestureInterpreter::~GestureInterpreter() {
  SetTimerProvider(NULL, NULL);
  SetPropProvider(NULL, NULL);
}

namespace {
stime_t InternalTimerCallback(stime_t now, void* callback_data) {
  Log("TimerCallback called");
  GestureInterpreter* gi = reinterpret_cast<GestureInterpreter*>(callback_data);
  stime_t next = -1.0;
  gi->TimerCallback(now, &next);
  return next;
}
}

void GestureInterpreter::PushHardwareState(HardwareState* hwstate) {
  stime_t timeout = -1.0;
  Gesture* gs = interpreter_->SyncInterpret(hwstate, &timeout);
  if (timer_provider_ && interpret_timer_) {
    timer_provider_->cancel_fn(timer_provider_data_, interpret_timer_);
    if (timeout > 0.0) {
      timer_provider_->set_fn(timer_provider_data_,
                              interpret_timer_,
                              timeout,
                              InternalTimerCallback,
                              this);
      Log("Setting timer for %f s out.", timeout);
    }
  } else {
    Err("No timer!");
  }
  if (gs && callback_) {
    (*callback_)(callback_data_, gs);
  }
}

void GestureInterpreter::SetHardwareProperties(
    const HardwareProperties& hwprops) {
  interpreter_->SetHardwareProperties(hwprops);
}

void GestureInterpreter::TimerCallback(stime_t now, stime_t* timeout) {
  Gesture* gs = interpreter_->HandleTimer(now, timeout);
  if (gs && callback_)
    (*callback_)(callback_data_, gs);
}

void GestureInterpreter::SetTimerProvider(GesturesTimerProvider* tp,
                                          void* data) {
  if (timer_provider_ == tp && timer_provider_data_ == data)
    return;
  if (timer_provider_ && interpret_timer_) {
    timer_provider_->free_fn(timer_provider_data_, interpret_timer_);
    interpret_timer_ = NULL;
  }
  if (interpret_timer_)
    Log("How was interpret_timer_ not NULL?!");
  timer_provider_ = tp;
  timer_provider_data_ = data;
  if (timer_provider_)
    interpret_timer_ = timer_provider_->create_fn(timer_provider_data_);
}

void GestureInterpreter::SetPropProvider(GesturesPropProvider* pp,
                                         void* data) {
  if (prop_provider_ == pp && prop_provider_data_ == data)
    return;
  if (prop_provider_)
    interpreter_->Deconfigure(prop_provider_, prop_provider_data_);
  prop_provider_ = pp;
  prop_provider_data_ = data;
  if (prop_provider_)
    interpreter_->Configure(prop_provider_, prop_provider_data_);
}
