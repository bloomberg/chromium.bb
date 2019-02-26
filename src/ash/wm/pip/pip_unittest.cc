// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_window_resizer.h"

#include <string>
#include <utility>

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

using PipTest = AshTestBase;

std::unique_ptr<views::Widget> CreateWidget() {
  std::unique_ptr<views::Widget> widget(new views::Widget);
  views::Widget::InitParams params;
  params.delegate = new views::WidgetDelegateView();
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = Shell::GetPrimaryRootWindow();
  widget->Init(params);
  return widget;
}

TEST_F(PipTest, ShowInactive) {
  auto widget = CreateWidget();
  const wm::WMEvent pip_event(wm::WM_EVENT_PIP);
  auto* window_state = wm::GetWindowState(widget->GetNativeWindow());
  window_state->OnWMEvent(&pip_event);
  ASSERT_TRUE(window_state->IsPip());
  ASSERT_FALSE(widget->IsVisible());
  widget->Show();
  ASSERT_TRUE(widget->IsVisible());
  EXPECT_FALSE(widget->IsActive());

  widget->Activate();
  EXPECT_FALSE(widget->IsActive());

  const wm::WMEvent normal_event(wm::WM_EVENT_NORMAL);
  window_state->OnWMEvent(&normal_event);
  EXPECT_FALSE(window_state->IsPip());
  EXPECT_FALSE(widget->IsActive());

  widget->Activate();
  EXPECT_TRUE(widget->IsActive());

  window_state->OnWMEvent(&pip_event);
  EXPECT_FALSE(widget->IsActive());
}

TEST_F(PipTest, ShortcutNavigation) {
  auto widget = CreateWidget();
  auto pip_widget = CreateWidget();
  widget->Show();
  pip_widget->Show();
  const wm::WMEvent pip_event(wm::WM_EVENT_PIP);
  auto* pip_window_state = wm::GetWindowState(pip_widget->GetNativeWindow());
  pip_window_state->OnWMEvent(&pip_event);
  EXPECT_TRUE(pip_window_state->IsPip());
  EXPECT_FALSE(pip_widget->IsActive());
  ASSERT_TRUE(widget->IsActive());

  auto* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_V, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  EXPECT_TRUE(pip_widget->IsActive());
  EXPECT_FALSE(widget->IsActive());

  auto* shelf = AshTestBase::GetPrimaryShelf()->shelf_widget();
  auto* status_area =
      Shell::GetPrimaryRootWindowController()->GetStatusAreaWidget();

  // Cycle Backward.
  generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(shelf->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(status_area->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(widget->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(pip_widget->IsActive());

  // Forward
  generator->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(widget->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(status_area->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(shelf->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(pip_widget->IsActive());
}

}  // namespace ash
