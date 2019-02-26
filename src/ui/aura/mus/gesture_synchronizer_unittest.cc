// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/gesture_synchronizer.h"

#include <memory>

#include "ui/aura/env.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/test/aura_mus_test_base.h"
#include "ui/aura/test/mus/test_window_tree.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/events/test/event_generator.h"

namespace aura {

class GestureSynchronizerTest : public test::AuraMusClientTestBase {
 public:
  GestureSynchronizerTest() = default;
  ~GestureSynchronizerTest() override = default;

 protected:
  ui::GestureRecognizer* gesture_recognizer() {
    return Env::GetInstance()->gesture_recognizer();
  }

  std::unique_ptr<Window> NewWindow(aura::WindowDelegate* delegate = nullptr) {
    return std::unique_ptr<Window>(aura::test::CreateTestWindowWithDelegate(
        delegate, 0, gfx::Rect(0, 0, 100, 100), root_window()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GestureSynchronizerTest);
};

TEST_F(GestureSynchronizerTest, CancelActiveTouchesExcept) {
  std::unique_ptr<Window> window = NewWindow();
  gesture_recognizer()->CancelActiveTouchesExcept(window.get());
  EXPECT_EQ(window_tree()->last_not_cancelled_window_id(),
            WindowMus::Get(window.get())->server_id());
}

TEST_F(GestureSynchronizerTest, CancelActiveTouchesExceptForNullptr) {
  gesture_recognizer()->CancelActiveTouchesExcept(nullptr);
  EXPECT_EQ(window_tree()->last_not_cancelled_window_id(), kInvalidServerId);
}

TEST_F(GestureSynchronizerTest, CancelActiveTouches) {
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<Window> window = NewWindow(&delegate);
  ui::test::EventGenerator event_generator(root_window());
  event_generator.MoveTouch(window->GetBoundsInScreen().CenterPoint());
  event_generator.PressTouch();
  gesture_recognizer()->CancelActiveTouches(window.get());
  EXPECT_EQ(window_tree()->last_cancelled_window_id(),
            WindowMus::Get(window.get())->server_id());
}

TEST_F(GestureSynchronizerTest, CancelActiveTouchesNotSentWithoutTouches) {
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<Window> window = NewWindow(&delegate);
  gesture_recognizer()->CancelActiveTouches(window.get());
  EXPECT_EQ(window_tree()->last_cancelled_window_id(), 0u);
}

TEST_F(GestureSynchronizerTest, TransferGestureEventsTo) {
  std::unique_ptr<Window> window1 = NewWindow();
  std::unique_ptr<Window> window2 = NewWindow();
  gesture_recognizer()->TransferEventsTo(window1.get(), window2.get(),
                                         ui::TransferTouchesBehavior::kCancel);
  EXPECT_EQ(window_tree()->last_transfer_current(),
            WindowMus::Get(window1.get())->server_id());
  EXPECT_EQ(window_tree()->last_transfer_new(),
            WindowMus::Get(window2.get())->server_id());
  EXPECT_TRUE(window_tree()->last_transfer_should_cancel());

  gesture_recognizer()->TransferEventsTo(
      window2.get(), window1.get(), ui::TransferTouchesBehavior::kDontCancel);
  EXPECT_EQ(window_tree()->last_transfer_current(),
            WindowMus::Get(window2.get())->server_id());
  EXPECT_EQ(window_tree()->last_transfer_new(),
            WindowMus::Get(window1.get())->server_id());
  EXPECT_FALSE(window_tree()->last_transfer_should_cancel());
}

}  // namespace aura
