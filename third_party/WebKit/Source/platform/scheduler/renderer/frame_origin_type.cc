// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/frame_origin_type.h"

#include "platform/WebFrameScheduler.h"

namespace blink {
namespace scheduler {

FrameOriginType GetFrameOriginType(WebFrameScheduler* scheduler) {
  DCHECK(scheduler);

  if (scheduler->GetFrameType() == WebFrameScheduler::FrameType::kMainFrame)
    return FrameOriginType::kMainFrame;

  if (scheduler->IsCrossOrigin()) {
    return FrameOriginType::kCrossOriginFrame;
  } else {
    return FrameOriginType::kSameOriginFrame;
  }
}

}  // namespace scheduler
}  // namespace blink
