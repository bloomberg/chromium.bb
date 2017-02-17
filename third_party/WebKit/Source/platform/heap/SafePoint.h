// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SafePoint_h
#define SafePoint_h

#include "platform/heap/ThreadState.h"
#include "wtf/ThreadingPrimitives.h"

namespace blink {

class SafePointScope final {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(SafePointScope);

 public:
  explicit SafePointScope(BlinkGC::StackState stackState,
                          ThreadState* state = ThreadState::current())
      : m_state(state) {
    if (m_state) {
      m_state->enterSafePoint(stackState, this);
    }
  }

  ~SafePointScope() {
    if (m_state)
      m_state->leaveSafePoint();
  }

 private:
  ThreadState* m_state;
};

}  // namespace blink

#endif
