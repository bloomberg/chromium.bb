// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/UserGestureIndicator.h"

#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "platform/Histogram.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

// User gestures timeout in 1 second.
const double kUserGestureTimeout = 1.0;

// For out of process tokens we allow a 10 second delay.
const double kUserGestureOutOfProcessTimeout = 10.0;

// static
RefPtr<UserGestureToken> UserGestureToken::Adopt(Document* document,
                                                 UserGestureToken* token) {
  if (!token || !token->HasGestures())
    return nullptr;
  if (document && document->GetFrame())
    document->GetFrame()->NotifyUserActivation();
  return token;
}

UserGestureToken::UserGestureToken(Status status)
    : consumable_gestures_(0),
      timestamp_(WTF::CurrentTime()),
      timeout_policy_(kDefault) {
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

void UserGestureIndicator::UpdateRootToken() {
  if (!root_token_) {
    root_token_ = token_.Get();
  } else {
    RecordUserGestureMerge(*root_token_, *token_);
    token_->TransferGestureTo(root_token_);
  }
}

UserGestureIndicator::UserGestureIndicator(PassRefPtr<UserGestureToken> token) {
  if (!IsMainThread() || !token || token == root_token_)
    return;
  token_ = std::move(token);
  token_->ResetTimestamp();
  UpdateRootToken();
}

UserGestureIndicator::UserGestureIndicator(UserGestureToken::Status status) {
  if (!IsMainThread())
    return;
  token_ = AdoptRef(new UserGestureToken(status));
  UpdateRootToken();
}

UserGestureIndicator::~UserGestureIndicator() {
  if (IsMainThread() && token_ && token_ == root_token_)
    root_token_ = nullptr;
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
    if (token->ConsumeGesture())
      return true;
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
