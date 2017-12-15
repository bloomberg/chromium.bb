// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/probe/PlatformProbes.h"

namespace blink {
namespace probe {

double ProbeBase::CaptureStartTime() const {
  if (!start_time_)
    start_time_ = CurrentTimeTicksInSeconds();
  return start_time_;
}

double ProbeBase::CaptureEndTime() const {
  if (!end_time_)
    end_time_ = CurrentTimeTicksInSeconds();
  return end_time_;
}

double ProbeBase::Duration() const {
  DCHECK(start_time_);
  return CaptureEndTime() - start_time_;
}

}  // namespace probe
}  // namespace blink
