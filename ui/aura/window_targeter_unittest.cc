// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_targeter.h"

#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_event_handler.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"

namespace aura {

class WindowTargeterTest : public test::AuraTestBase {
 public:
  WindowTargeterTest() {}
  virtual ~WindowTargeterTest() {}

  Window* root_window() { return AuraTestBase::root_window(); }
};

TEST_F(WindowTargeterTest, Basic) {
  test::TestWindowDelegate delegate;
  scoped_ptr<Window> window(CreateNormalWindow(1, root_window(), &delegate));
  Window* one = CreateNormalWindow(2, window.get(), &delegate);
  Window* two = CreateNormalWindow(3, window.get(), &delegate);

  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  one->SetBounds(gfx::Rect(0, 0, 500, 100));
  two->SetBounds(gfx::Rect(501, 0, 500, 1000));

  root_window()->Show();

  test::TestEventHandler handler;
  one->AddPreTargetHandler(&handler);

  ui::MouseEvent press(ui::ET_MOUSE_PRESSED,
                       gfx::Point(20, 20),
                       gfx::Point(20, 20),
                       ui::EF_NONE);
  root_window()->GetDispatcher()->AsRootWindowHostDelegate()->
      OnHostMouseEvent(&press);
  EXPECT_EQ(1, handler.num_mouse_events());

  handler.Reset();
  ui::EventDispatchDetails details =
      root_window()->GetDispatcher()->OnEventFromSource(&press);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, handler.num_mouse_events());

  one->RemovePreTargetHandler(&handler);
}

}  // namespace aura
