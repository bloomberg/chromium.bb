// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/controls/menu/menu_runner_impl_cocoa.h"

#import "ui/base/cocoa/menu_controller.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_runner_impl_adapter.h"

namespace views {
namespace internal {

// static
MenuRunnerImplInterface* MenuRunnerImplInterface::Create(
    ui::MenuModel* menu_model,
    int32 run_types) {
  if ((run_types & MenuRunner::CONTEXT_MENU) != 0 &&
      (run_types & MenuRunner::IS_NESTED) == 0) {
    return new MenuRunnerImplCocoa(menu_model);
  }

  return new MenuRunnerImplAdapter(menu_model);
}

MenuRunnerImplCocoa::MenuRunnerImplCocoa(ui::MenuModel* menu)
    : delete_after_run_(false), closing_event_time_(base::TimeDelta()) {
  menu_controller_.reset(
      [[MenuController alloc] initWithModel:menu useWithPopUpButtonCell:NO]);
}

bool MenuRunnerImplCocoa::IsRunning() const {
  return [menu_controller_ isMenuOpen];
}

void MenuRunnerImplCocoa::Release() {
  if (IsRunning()) {
    if (delete_after_run_)
      return;  // We already canceled.

    delete_after_run_ = true;
    [menu_controller_ cancel];
  } else {
    delete this;
  }
}

MenuRunner::RunResult MenuRunnerImplCocoa::RunMenuAt(Widget* parent,
                                                     MenuButton* button,
                                                     const gfx::Rect& bounds,
                                                     MenuAnchorPosition anchor,
                                                     int32 run_types) {
  DCHECK(run_types & MenuRunner::CONTEXT_MENU);
  DCHECK(!IsRunning());
  closing_event_time_ = base::TimeDelta();
  [NSMenu popUpContextMenu:[menu_controller_ menu]
                 withEvent:[NSApp currentEvent]
                   forView:nil];
  closing_event_time_ = ui::EventTimeForNow();

  if (delete_after_run_) {
    delete this;
    return MenuRunner::MENU_DELETED;
  }

  return MenuRunner::NORMAL_EXIT;
}

void MenuRunnerImplCocoa::Cancel() {
  [menu_controller_ cancel];
}

base::TimeDelta MenuRunnerImplCocoa::GetClosingEventTime() const {
  return closing_event_time_;
}

MenuRunnerImplCocoa::~MenuRunnerImplCocoa() {
}

}  // namespace internal
}  // namespace views
