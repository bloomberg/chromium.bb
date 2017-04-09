/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/UserGestureIndicator.h"

#include "platform/Histogram.h"
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"
#include "wtf/StdLibExtras.h"

namespace blink {

// User gestures timeout in 1 second.
const double kUserGestureTimeout = 1.0;

// For out of process tokens we allow a 10 second delay.
const double kUserGestureOutOfProcessTimeout = 10.0;

UserGestureToken::UserGestureToken(Status status)
    : consumable_gestures_(0),
      timestamp_(WTF::CurrentTime()),
      timeout_policy_(kDefault),
      usage_callback_(nullptr) {
  if (status == kNewGesture || !UserGestureIndicator::CurrentTokenThreadSafe())
    consumable_gestures_++;
}

bool UserGestureToken::HasGestures() const {
  return consumable_gestures_ && !HasTimedOut();
}

void UserGestureToken::TransferGestureTo(UserGestureToken* other) {
  if (!HasGestures())
    return;
  consumable_gestures_--;
  other->consumable_gestures_++;
}

bool UserGestureToken::ConsumeGesture() {
  if (!consumable_gestures_)
    return false;
  consumable_gestures_--;
  return true;
}

void UserGestureToken::SetTimeoutPolicy(TimeoutPolicy policy) {
  if (!HasTimedOut() && HasGestures() && policy > timeout_policy_)
    timeout_policy_ = policy;
}

void UserGestureToken::ResetTimestamp() {
  timestamp_ = WTF::CurrentTime();
}

bool UserGestureToken::HasTimedOut() const {
  if (timeout_policy_ == kHasPaused)
    return false;
  double timeout = timeout_policy_ == kOutOfProcess
                       ? kUserGestureOutOfProcessTimeout
                       : kUserGestureTimeout;
  return WTF::CurrentTime() - timestamp_ > timeout;
}

void UserGestureToken::SetUserGestureUtilizedCallback(
    UserGestureUtilizedCallback* callback) {
  CHECK(this == UserGestureIndicator::CurrentToken());
  usage_callback_ = callback;
}

void UserGestureToken::UserGestureUtilized() {
  if (usage_callback_) {
    usage_callback_->UserGestureUtilized();
    usage_callback_ = nullptr;
  }
}

// This enum is used in a histogram, so its values shouldn't change.
enum GestureMergeState {
  kOldTokenHasGesture = 1 << 0,
  kNewTokenHasGesture = 1 << 1,
  kGestureMergeStateEnd = 1 << 2,
};

// Remove this when user gesture propagation is standardized. See
// https://crbug.com/404161.
static void RecordUserGestureMerge(const UserGestureToken& old_token,
                                   const UserGestureToken& new_token) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, gesture_merge_histogram,
                      ("Blink.Gesture.Merged", kGestureMergeStateEnd));
  int sample = 0;
  if (old_token.HasGestures())
    sample |= kOldTokenHasGesture;
  if (new_token.HasGestures())
    sample |= kNewTokenHasGesture;
  gesture_merge_histogram.Count(sample);
}

UserGestureToken* UserGestureIndicator::root_token_ = nullptr;

UserGestureIndicator::UserGestureIndicator(PassRefPtr<UserGestureToken> token) {
  // Silently ignore UserGestureIndicators on non-main threads and tokens that
  // are already active.
  if (!IsMainThread() || !token || token == root_token_)
    return;

  token_ = std::move(token);
  if (!root_token_) {
    root_token_ = token_.Get();
  } else {
    RecordUserGestureMerge(*root_token_, *token_);
    token_->TransferGestureTo(root_token_);
  }
  token_->ResetTimestamp();
}

UserGestureIndicator::~UserGestureIndicator() {
  if (IsMainThread() && token_ && token_ == root_token_) {
    root_token_->SetUserGestureUtilizedCallback(nullptr);
    root_token_ = nullptr;
  }
}

// static
bool UserGestureIndicator::UtilizeUserGesture() {
  if (UserGestureIndicator::ProcessingUserGesture()) {
    root_token_->UserGestureUtilized();
    return true;
  }
  return false;
}

bool UserGestureIndicator::ProcessingUserGesture() {
  if (auto* token = CurrentToken())
    return token->HasGestures();
  return false;
}

bool UserGestureIndicator::ProcessingUserGestureThreadSafe() {
  return IsMainThread() && ProcessingUserGesture();
}

// static
bool UserGestureIndicator::ConsumeUserGesture() {
  if (auto* token = CurrentToken()) {
    if (token->ConsumeGesture()) {
      token->UserGestureUtilized();
      return true;
    }
  }
  return false;
}

bool UserGestureIndicator::ConsumeUserGestureThreadSafe() {
  return IsMainThread() && ConsumeUserGesture();
}

UserGestureToken* UserGestureIndicator::CurrentToken() {
  DCHECK(IsMainThread());
  return root_token_;
}

UserGestureToken* UserGestureIndicator::CurrentTokenThreadSafe() {
  return IsMainThread() ? CurrentToken() : nullptr;
}

}  // namespace blink
