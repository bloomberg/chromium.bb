// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/stacking_controller.h"

#include "ui/aura/desktop.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura_shell/test/aura_shell_test_base.h"

namespace aura_shell {
namespace test {

typedef aura_shell::test::AuraShellTestBase StackingControllerTest;

TEST_F(StackingControllerTest, GetTopmostWindowToActivate) {
  aura::test::ActivateWindowDelegate activate;
  aura::test::ActivateWindowDelegate non_activate(false);

  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(
      &non_activate, 1, gfx::Rect(), NULL));
  scoped_ptr<aura::Window> w2(aura::test::CreateTestWindowWithDelegate(
      &activate, 2, gfx::Rect(), NULL));
  scoped_ptr<aura::Window> w3(aura::test::CreateTestWindowWithDelegate(
      &non_activate, 3, gfx::Rect(), NULL));
  EXPECT_EQ(w2.get(), aura::Desktop::GetInstance()->stacking_client()->
      GetTopmostWindowToActivate(NULL));
}

// Test if the clicking on a menu picks the transient parent as activatable
// window.
TEST_F(StackingControllerTest, ClickOnMenu) {
  aura::test::ActivateWindowDelegate activate;
  aura::test::ActivateWindowDelegate non_activate(false);

  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(
      &activate, 1, gfx::Rect(100, 100), NULL));
  EXPECT_EQ(NULL, aura::Desktop::GetInstance()->active_window());

  // Clicking on an activatable window activtes the window.
  aura::test::EventGenerator generator(w1.get());
  generator.ClickLeftButton();
  EXPECT_EQ(w1.get(), aura::Desktop::GetInstance()->active_window());

  // Creates a menu that covers the transient parent.
  scoped_ptr<aura::Window> menu(aura::test::CreateTestWindowWithDelegateAndType(
      &non_activate, aura::WINDOW_TYPE_MENU, 2, gfx::Rect(100, 100), NULL));
  w1->AddTransientChild(menu.get());

  // Clicking on a menu whose transient parent is active window shouldn't
  // change the active window.
  generator.ClickLeftButton();
  EXPECT_EQ(w1.get(), aura::Desktop::GetInstance()->active_window());
}

}  // namespace test
}  // namespace aura_shell
