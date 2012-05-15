// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/shared/input_method_event_filter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/root_window_event_filter.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_activation_client.h"
#include "ui/aura/test/test_event_filter.h"
#include "ui/aura/test/test_windows.h"

#if !defined(OS_WIN) && !defined(USE_X11)
// On platforms except Windows and X11, aura::test::EventGenerator::PressKey
// generates a key event without native_event(), which is not supported by
// ui::MockInputMethod.
#define TestInputMethodKeyEventPropagation \
DISABLED_TestInputMethodKeyEventPropagation
#endif

namespace aura {
namespace test {

typedef AuraTestBase InputMethodEventFilterTest;

TEST_F(InputMethodEventFilterTest, TestInputMethodProperty) {
  aura::shared::RootWindowEventFilter* root_filter =
      new aura::shared::RootWindowEventFilter(root_window());
  root_window()->SetEventFilter(root_filter);

  // Add the InputMethodEventFilter before the TestEventFilter.
  aura::shared::InputMethodEventFilter input_method_event_filter(
      root_window());
  root_filter->AddFilter(&input_method_event_filter);

  // Tests if InputMethodEventFilter adds a window property on its
  // construction.
  EXPECT_TRUE(
      root_window()->GetProperty(aura::client::kRootWindowInputMethodKey));

  root_filter->RemoveFilter(&input_method_event_filter);
}

// Tests if InputMethodEventFilter dispatches a ui::ET_TRANSLATED_KEY_* event to
// the root window.
TEST_F(InputMethodEventFilterTest, TestInputMethodKeyEventPropagation) {
  aura::client::SetActivationClient(root_window(),
                                    new TestActivationClient(root_window()));

  aura::shared::RootWindowEventFilter* root_filter =
      new shared::RootWindowEventFilter(root_window());
  root_window()->SetEventFilter(root_filter);

  // Add the InputMethodEventFilter before the TestEventFilter.
  aura::shared::InputMethodEventFilter input_method_event_filter(
      root_window());
  root_filter->AddFilter(&input_method_event_filter);

  // Add TestEventFilter to the RootWindow.
  aura::test::TestEventFilter test_filter;
  root_filter->AddFilter(&test_filter);

  // We need an active window. Otherwise, the root window will not forward a key
  // event to event filters.
  aura::test::TestWindowDelegate test_delegate;
  scoped_ptr<aura::Window> window(aura::test::CreateTestWindowWithDelegate(
      &test_delegate,
      -1,
      gfx::Rect(),
      root_window()));
  aura::client::GetActivationClient(root_window())->ActivateWindow(
      window.get());

  // Send a fake key event to the root window. InputMethodEventFilter, which is
  // automatically set up by AshTestBase, consumes it and sends a new
  // ui::ET_TRANSLATED_KEY_* event to the root window, which will be consumed by
  // the test event filter.
  aura::test::EventGenerator generator(root_window());
  EXPECT_EQ(0, test_filter.key_event_count());
  generator.PressKey(ui::VKEY_SPACE, 0);
  EXPECT_EQ(1, test_filter.key_event_count());
  generator.ReleaseKey(ui::VKEY_SPACE, 0);
  EXPECT_EQ(2, test_filter.key_event_count());

  root_filter->RemoveFilter(&input_method_event_filter);
  root_filter->RemoveFilter(&test_filter);

  // Reset window before |test_delegate| gets deleted.
  window.reset();
}

}  // namespace test
}  // namespace aura
