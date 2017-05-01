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

#ifndef UserGestureIndicator_h
#define UserGestureIndicator_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

// A UserGestureToken represents a user gesture. It can be referenced and saved
// for later (see, e.g., DOMTimer, which propagates user gestures to the timer
// fire in certain situations). Passing it to a UserGestureIndicator will cause
// it to be considered as currently being processed.
class PLATFORM_EXPORT UserGestureToken : public RefCounted<UserGestureToken> {
  WTF_MAKE_NONCOPYABLE(UserGestureToken);

 public:
  enum Status { kNewGesture, kPossiblyExistingGesture };
  enum TimeoutPolicy { kDefault, kOutOfProcess, kHasPaused };

  ~UserGestureToken() {}
  bool HasGestures() const;
  void TransferGestureTo(UserGestureToken*);
  bool ConsumeGesture();
  void SetTimeoutPolicy(TimeoutPolicy);
  void ResetTimestamp();

 protected:
  UserGestureToken(Status);

 private:
  bool HasTimedOut() const;

  size_t consumable_gestures_;
  double timestamp_;
  TimeoutPolicy timeout_policy_;
};

class PLATFORM_EXPORT UserGestureIndicator final {
  USING_FAST_MALLOC(UserGestureIndicator);
  WTF_MAKE_NONCOPYABLE(UserGestureIndicator);

 public:
  // Note: All *ThreadSafe methods are safe to call from any thread. Their
  // non-suffixed counterparts *must* be called on the main thread. Consider
  // always using the non-suffixed one unless the code really
  // needs to be thread-safe

  // Returns whether a user gesture is currently in progress.
  static bool ProcessingUserGesture();
  static bool ProcessingUserGestureThreadSafe();

  // Mark the current user gesture (if any) as having been used, such that
  // it cannot be used again.  This is done only for very security-sensitive
  // operations like creating a new process.
  static bool ConsumeUserGesture();
  static bool ConsumeUserGestureThreadSafe();

  static UserGestureToken* CurrentToken();
  static UserGestureToken* CurrentTokenThreadSafe();

  explicit UserGestureIndicator(PassRefPtr<UserGestureToken>);
  ~UserGestureIndicator();

 private:
  static UserGestureToken* root_token_;

  RefPtr<UserGestureToken> token_;
};

}  // namespace blink

#endif
