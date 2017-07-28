// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UserGestureIndicator_h
#define UserGestureIndicator_h

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

// A UserGestureToken represents a user gesture. It can be referenced and saved
// for later (see, e.g., DOMTimer, which propagates user gestures to the timer
// fire in certain situations). Passing it to a UserGestureIndicator will cause
// it to be considered as currently being processed.
class CORE_EXPORT UserGestureToken : public RefCounted<UserGestureToken> {
  WTF_MAKE_NONCOPYABLE(UserGestureToken);

 public:
  enum Status { kNewGesture, kPossiblyExistingGesture };
  enum TimeoutPolicy { kDefault, kOutOfProcess, kHasPaused };

  // Creates a UserGestureToken with the given status. Also if a non-null
  // Document* is provided, associates the token with the document.
  static PassRefPtr<UserGestureToken> Create(Document*,
                                             Status = kPossiblyExistingGesture);
  static PassRefPtr<UserGestureToken> Adopt(Document*, UserGestureToken*);

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

class CORE_EXPORT UserGestureIndicator final {
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
