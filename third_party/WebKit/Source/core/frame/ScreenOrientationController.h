// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientationController_h
#define ScreenOrientationController_h

#include "core/CoreExport.h"
#include "core/frame/LocalFrame.h"
#include "platform/Supplementable.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"

namespace blink {

class WebLockOrientationCallback;

// ScreenOrientationController allows to manipulate screen orientation in Blink
// outside of the screen_orientation/ modules. It is an interface that the
// module will implement and add a provider for.
// Callers of ScreenOrientationController::from() should always assume the
// returned pointer can be nullptr.
class CORE_EXPORT ScreenOrientationController
    : public GarbageCollectedFinalized<ScreenOrientationController>,
      public Supplement<LocalFrame> {
  USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientationController);

 public:
  virtual ~ScreenOrientationController() = default;

  static ScreenOrientationController* from(LocalFrame&);

  virtual void lock(WebScreenOrientationLockType,
                    std::unique_ptr<WebLockOrientationCallback>) = 0;
  virtual void unlock() = 0;

  // Returns whether a lock() call was made without an unlock() call. Others
  // frames might have changed the lock state so this should only be used to
  // know whether the current frame made an attempt to lock without explicitly
  // unlocking.
  virtual bool maybeHasActiveLock() const = 0;

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit ScreenOrientationController(LocalFrame&);
  // To be called by an ScreenOrientationController to register its
  // implementation.
  static void provideTo(LocalFrame&, ScreenOrientationController*);

 private:
  static const char* supplementName();
};

}  // namespace blink

#endif  // ScreenOrientationController_h
