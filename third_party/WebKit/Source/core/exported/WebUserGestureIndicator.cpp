/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/web/WebUserGestureIndicator.h"

#include "core/dom/UserGestureIndicator.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "public/web/WebUserGestureToken.h"

namespace blink {

bool WebUserGestureIndicator::IsProcessingUserGesture() {
  return UserGestureIndicator::ProcessingUserGesture();
}

bool WebUserGestureIndicator::IsProcessingUserGestureThreadSafe() {
  return UserGestureIndicator::ProcessingUserGestureThreadSafe();
}

// TODO(csharrison): consumeUserGesture() and currentUserGestureToken() use
// the thread-safe API, which many callers probably do not need. Consider
// updating them if they are in any sort of critical path or called often.
bool WebUserGestureIndicator::ConsumeUserGesture() {
  return UserGestureIndicator::ConsumeUserGestureThreadSafe();
}

bool WebUserGestureIndicator::ProcessedUserGestureSinceLoad(
    WebLocalFrame* frame) {
  return ToWebLocalFrameImpl(frame)->GetFrame()->HasReceivedUserGesture();
}

WebUserGestureToken WebUserGestureIndicator::CurrentUserGestureToken() {
  return WebUserGestureToken(UserGestureIndicator::CurrentTokenThreadSafe());
}

void WebUserGestureIndicator::ExtendTimeout() {
  UserGestureIndicator::SetTimeoutPolicy(UserGestureToken::kOutOfProcess);
}

void WebUserGestureIndicator::DisableTimeout() {
  UserGestureIndicator::SetTimeoutPolicy(UserGestureToken::kHasPaused);
}

}  // namespace blink
