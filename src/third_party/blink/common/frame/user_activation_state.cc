// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/user_activation_state.h"

namespace blink {

// The expiry time should be long enough to allow network round trips even in a
// very slow connection (to support xhr-like calls with user activation), yet
// not too long to make an "unattneded" page feel activated.
constexpr base::TimeDelta kActivationLifespan =
    base::TimeDelta::FromSeconds(30);

void UserActivationState::Activate() {
  has_been_active_ = true;
  is_active_ = true;
  activation_timestamp_ = base::TimeTicks::Now();
}

void UserActivationState::Clear() {
  has_been_active_ = false;
  is_active_ = false;
}

bool UserActivationState::IsActive() {
  if (is_active_ &&
      (base::TimeTicks::Now() - activation_timestamp_ > kActivationLifespan)) {
    is_active_ = false;
  }
  return is_active_;
}

bool UserActivationState::ConsumeIfActive() {
  if (!IsActive())
    return false;
  is_active_ = false;
  return true;
}

}  // namespace blink
