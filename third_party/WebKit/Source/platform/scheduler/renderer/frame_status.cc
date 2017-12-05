// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/frame_status.h"

#include "platform/WebFrameScheduler.h"
#include "platform/scheduler/renderer/web_view_scheduler.h"

namespace blink {
namespace scheduler {

namespace {

enum class FrameThrottlingState {
  kVisible = 0,
  kVisibleService = 1,
  kHidden = 2,
  kHiddenService = 3,
  kBackground = 4,
  kBackgroundExemptSelf = 5,
  kBackgroundExemptOther = 6,

  kCount = 7
};

enum class FrameOriginState {
  kMainFrame = 0,
  kSameOrigin = 1,
  kCrossOrigin = 2,

  kCount = 3
};

FrameThrottlingState GetFrameThrottlingState(
    const WebFrameScheduler& frame_scheduler) {
  if (frame_scheduler.IsPageVisible()) {
    if (frame_scheduler.IsFrameVisible())
      return FrameThrottlingState::kVisible;
    return FrameThrottlingState::kHidden;
  }

  WebViewScheduler* web_view_scheduler = frame_scheduler.GetWebViewScheduler();
  if (web_view_scheduler && web_view_scheduler->IsPlayingAudio()) {
    if (frame_scheduler.IsFrameVisible())
      return FrameThrottlingState::kVisibleService;
    return FrameThrottlingState::kHiddenService;
  }

  if (frame_scheduler.IsExemptFromBudgetBasedThrottling())
    return FrameThrottlingState::kBackgroundExemptSelf;

  if (web_view_scheduler &&
      web_view_scheduler->IsExemptFromBudgetBasedThrottling())
    return FrameThrottlingState::kBackgroundExemptOther;

  return FrameThrottlingState::kBackground;
}

FrameOriginState GetFrameOriginState(const WebFrameScheduler& frame_scheduler) {
  if (frame_scheduler.GetFrameType() ==
      WebFrameScheduler::FrameType::kMainFrame) {
    return FrameOriginState::kMainFrame;
  }
  if (frame_scheduler.IsCrossOrigin())
    return FrameOriginState::kCrossOrigin;
  return FrameOriginState::kSameOrigin;
}

}  // namespace

FrameStatus GetFrameStatus(WebFrameScheduler* frame_scheduler) {
  if (!frame_scheduler)
    return FrameStatus::kNone;
  FrameThrottlingState throttling_state =
      GetFrameThrottlingState(*frame_scheduler);
  FrameOriginState origin_state = GetFrameOriginState(*frame_scheduler);
  return static_cast<FrameStatus>(
      static_cast<int>(FrameStatus::kSpecialCasesCount) +
      static_cast<int>(origin_state) *
          static_cast<int>(FrameThrottlingState::kCount) +
      static_cast<int>(throttling_state));
}

}  // namespace scheduler
}  // namespace blink
