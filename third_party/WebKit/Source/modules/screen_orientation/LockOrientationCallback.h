// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LockOrientationCallback_h
#define LockOrientationCallback_h

#include "base/memory/scoped_refptr.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/modules/screen_orientation/WebLockOrientationCallback.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationType.h"

namespace blink {

class ScriptPromiseResolver;

// LockOrientationCallback is an implementation of WebLockOrientationCallback
// that will resolve the underlying promise depending on the result passed to
// the callback.
class LockOrientationCallback final : public WebLockOrientationCallback {
  USING_FAST_MALLOC(LockOrientationCallback);
  WTF_MAKE_NONCOPYABLE(LockOrientationCallback);

 public:
  explicit LockOrientationCallback(ScriptPromiseResolver*);
  ~LockOrientationCallback() override;

  void OnSuccess() override;
  void OnError(WebLockOrientationError) override;

 private:
  Persistent<ScriptPromiseResolver> resolver_;
};

}  // namespace blink

#endif  // LockOrientationCallback_h
