// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameLifecycle_h
#define FrameLifecycle_h

#include "platform/wtf/Noncopyable.h"

namespace blink {

class FrameLifecycle {
  WTF_MAKE_NONCOPYABLE(FrameLifecycle);

 public:
  enum State {
    kAttached,
    kDetaching,
    kDetached,
  };

  FrameLifecycle();

  State GetState() const { return state_; }
  void AdvanceTo(State);

 private:
  State state_;
};

}  // namespace blink

#endif  // FrameLifecycle_h
