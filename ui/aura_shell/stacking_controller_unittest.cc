// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/stacking_controller.h"

#include "ui/aura/desktop.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura_shell/shell.h"

namespace aura_shell {
namespace test {

typedef aura::test::AuraTestBase StackingControllerTest;

TEST_F(StackingControllerTest, GetTopmostWindowToActivate) {
  Shell::CreateInstance(NULL);
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

}  // namespace test
}  // namespace aura_shell
