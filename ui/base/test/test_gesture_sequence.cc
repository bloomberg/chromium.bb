// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/test_gesture_sequence.h"

namespace ui {
class RootWindow;
namespace test {

TestGestureSequence::TestGestureSequence(GestureEventHelper* consumer)
    : GestureSequence(consumer) {
}

void TestGestureSequence::ForceTimeout() {
  static_cast<TestOneShotGestureSequenceTimer*>(
      long_press_timer())->ForceTimeout();
}

base::OneShotTimer<GestureSequence>*
    TestGestureSequence::CreateTimer() {
  return new TestOneShotGestureSequenceTimer();
}

}  // namespace test
}  // namespace ui
