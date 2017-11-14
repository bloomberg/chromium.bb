// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/ScheduledNavigation.h"

#include <memory>

#include "core/frame/Frame.h"
#include "core/frame/LocalFrame.h"

namespace blink {

ScheduledNavigation::ScheduledNavigation(Reason reason,
                                         double delay,
                                         Document* origin_document,
                                         bool replaces_current_item,
                                         bool is_location_change)
    : reason_(reason),
      delay_(delay),
      origin_document_(origin_document),
      replaces_current_item_(replaces_current_item),
      is_location_change_(is_location_change) {
  if (Frame::HasTransientUserActivation(
          origin_document ? origin_document->GetFrame() : nullptr))
    user_gesture_token_ = UserGestureIndicator::CurrentToken();
}

ScheduledNavigation::~ScheduledNavigation() {}

std::unique_ptr<UserGestureIndicator>
ScheduledNavigation::CreateUserGestureIndicator() {
  return std::make_unique<UserGestureIndicator>(user_gesture_token_);
}

}  // namespace blink
