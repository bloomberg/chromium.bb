// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_WINDOW_TARGETER_H_
#define UI_AURA_TEST_TEST_WINDOW_TARGETER_H_

#include "base/macros.h"
#include "ui/aura/window_targeter.h"
#include "ui/events/event_targeter.h"

namespace aura {
namespace test {

// A test WindowTargeter implementation that would always return the
// EventTarget it received from FindTargetForEvent or FindNextBestTarget
// as the EventTarget that should receive the event.
class TestWindowTargeter : public WindowTargeter {
 public:
  TestWindowTargeter();
  ~TestWindowTargeter() override;

 protected:
  // WindowTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override;
  ui::EventTarget* FindNextBestTarget(ui::EventTarget* previous_target,
                                      ui::Event* event) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWindowTargeter);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_WINDOW_TARGETER_H_
