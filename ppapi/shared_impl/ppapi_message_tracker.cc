// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppapi_message_tracker.h"

#include "base/logging.h"
#include "base/memory/singleton.h"

namespace ppapi {

//static
PpapiMessageTracker* PpapiMessageTracker::GetInstance() {
  return Singleton<PpapiMessageTracker>::get();
}

PpapiMessageTracker::PpapiMessageTracker() : enter_count_(0) {
}

PpapiMessageTracker::~PpapiMessageTracker() {
}

void PpapiMessageTracker::EnterMessageHandling() {
  base::AutoLock auto_lock(lock_);
  enter_count_++;
}

void PpapiMessageTracker::ExitMessageHandling() {
  base::AutoLock auto_lock(lock_);
  DCHECK_GT(enter_count_, 0);
  enter_count_--;
}

bool PpapiMessageTracker::IsHandlingMessage() {
  base::AutoLock auto_lock(lock_);
  return enter_count_ > 0;
}

ScopedTrackPpapiMessage::ScopedTrackPpapiMessage() {
  PpapiMessageTracker::GetInstance()->EnterMessageHandling();
}

ScopedTrackPpapiMessage::~ScopedTrackPpapiMessage() {
  PpapiMessageTracker::GetInstance()->ExitMessageHandling();
}

}  // namespace ppapi
