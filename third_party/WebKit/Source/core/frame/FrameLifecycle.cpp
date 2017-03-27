// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/FrameLifecycle.h"

#include "wtf/Assertions.h"

namespace blink {

FrameLifecycle::FrameLifecycle() : m_state(Attached) {}

void FrameLifecycle::advanceTo(State state) {
  switch (state) {
    case Attached:
    case Detached:
      // Normally, only allow state to move forward.
      DCHECK_GT(state, m_state);
      break;
    case Detaching:
      // We can go from Detaching to Detaching since the detach() method can be
      // re-entered.
      DCHECK_GE(state, m_state);
      break;
  }
  m_state = state;
}

}  // namespace blink
