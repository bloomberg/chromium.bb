// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/menu_test_utils.h"

#include "ui/views/controls/menu/menu_controller.h"

namespace views {
namespace test {

// TestMenuDelegate -----------------------------------------------------------

TestMenuDelegate::TestMenuDelegate()
    : execute_command_id_(0),
      on_menu_closed_called_count_(0),
      on_menu_closed_menu_(nullptr),
      on_menu_closed_run_result_(MenuRunner::MENU_DELETED),
      on_perform_drop_called_(false) {}

TestMenuDelegate::~TestMenuDelegate() {}

void TestMenuDelegate::ExecuteCommand(int id) {
  execute_command_id_ = id;
}

void TestMenuDelegate::OnMenuClosed(MenuItemView* menu,
                                    MenuRunner::RunResult result) {
  on_menu_closed_called_count_++;
  on_menu_closed_menu_ = menu;
  on_menu_closed_run_result_ = result;
}

int TestMenuDelegate::OnPerformDrop(MenuItemView* menu,
                                    DropPosition position,
                                    const ui::DropTargetEvent& event) {
  on_perform_drop_called_ = true;
  return ui::DragDropTypes::DRAG_COPY;
}

int TestMenuDelegate::GetDragOperations(MenuItemView* sender) {
  return ui::DragDropTypes::DRAG_COPY;
}

void TestMenuDelegate::WriteDragData(MenuItemView* sender,
                                     OSExchangeData* data) {}

// MenuControllerTestApi ------------------------------------------------------

MenuControllerTestApi::MenuControllerTestApi()
    : controller_(MenuController::GetActiveInstance()->AsWeakPtr()) {}

MenuControllerTestApi::~MenuControllerTestApi() {}

void MenuControllerTestApi::ClearState() {
  if (!controller_)
    return;
  controller_->ClearStateForTest();
}

void MenuControllerTestApi::SetShowing(bool showing) {
  if (!controller_)
    return;
  controller_->showing_ = showing;
}

}  // namespace test
}  // namespace views
