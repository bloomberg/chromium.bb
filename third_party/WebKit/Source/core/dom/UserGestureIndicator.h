// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UserGestureIndicator_h
#define UserGestureIndicator_h

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

// A UserGestureToken represents the current state of a user gesture. It can be
// retrieved from a UserGestureIndicator to save for later (see, e.g., DOMTimer,
// which propagates user gestures to the timer fire in certain situations).
// Passing it to a UserGestureIndicator later on will cause it to be considered
// as currently being processed.
class CORE_EXPORT UserGestureToken : public RefCounted<UserGestureToken> {
  WTF_MAKE_NONCOPYABLE(UserGestureToken);
  friend class UserGestureIndicator;

 public:
  enum Status { kNewGesture, kPossiblyExistingGesture };
  enum TimeoutPolicy { kDefault, kOutOfProcess, kHasPaused };

  ~UserGestureToken() {}

  // TODO(mustaq): The only user of this method is PepperPluginInstanceImpl.  We
  // need to investigate the usecase closely.
  bool HasGestures() const;

 private:
  UserGestureToken(Status);

  void TransferGestureTo(UserGestureToken*);
  bool ConsumeGesture();
  void SetTimeoutPolicy(TimeoutPolicy);
  void ResetTimestamp();
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

  static void SetTimeoutPolicy(UserGestureToken::TimeoutPolicy);

  explicit UserGestureIndicator(PassRefPtr<UserGestureToken>);

  // Constructs a UserGestureIndicator with a new UserGestureToken of the given
  // status.
  explicit UserGestureIndicator(
      UserGestureToken::Status = UserGestureToken::kPossiblyExistingGesture);
  ~UserGestureIndicator();

 private:
  void UpdateRootToken();

  static UserGestureToken* root_token_;

  RefPtr<UserGestureToken> token_;
};

}  // namespace blink

#endif
