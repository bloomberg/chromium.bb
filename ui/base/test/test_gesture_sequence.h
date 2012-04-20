// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_TEST_GESTURE_SEQUENCE_H_
#define UI_BASE_TEST_TEST_GESTURE_SEQUENCE_H_
#pragma once

#include "ui/base/gestures/gesture_sequence.h"

namespace ui {
class RootWindow;
namespace test {

class TestGestureSequence : public ui::GestureSequence {
 public:
  explicit TestGestureSequence(ui::GestureEventHelper* consumer);

  // Make the timer fire.
  void ForceTimeout();
  ui::GestureState state() const { return state_; }
  virtual base::OneShotTimer<GestureSequence>* CreateTimer() OVERRIDE;

 private:
  class TestOneShotGestureSequenceTimer
      : public base::OneShotTimer<ui::GestureSequence> {
   public:
    TestOneShotGestureSequenceTimer() {
    }

    void ForceTimeout() {
      if (IsRunning()) {
        user_task().Run();
        Stop();
      }
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(TestOneShotGestureSequenceTimer);
  };

  DISALLOW_COPY_AND_ASSIGN(TestGestureSequence);
};

}  // namespace test
}  // namespace ui

#endif  // UI_BASE_TEST_TEST_GESTURE_SEQUENCE_H_
