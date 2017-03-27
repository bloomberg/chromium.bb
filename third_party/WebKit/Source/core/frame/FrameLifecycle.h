// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameLifecycle_h
#define FrameLifecycle_h

#include "wtf/Noncopyable.h"

namespace blink {

class FrameLifecycle {
  WTF_MAKE_NONCOPYABLE(FrameLifecycle);

 public:
  enum State {
    Attached,
    Detaching,
    Detached,
  };

  FrameLifecycle();

  State state() const { return m_state; }
  void advanceTo(State);

 private:
  State m_state;
};

}  // namespace blink

#endif  // FrameLifecycle_h
