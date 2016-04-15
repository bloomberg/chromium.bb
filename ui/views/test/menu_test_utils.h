// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_MENU_TEST_UTILS_H_
#define UI_VIEWS_TEST_MENU_TEST_UTILS_H_

#include "base/macros.h"
#include "ui/views/controls/menu/menu_delegate.h"

namespace views {
namespace test {

// Test implementation of MenuDelegate that tracks calls to MenuDelegate, along
// with the provided parameters.
class TestMenuDelegate : public MenuDelegate {
 public:
  TestMenuDelegate();
  ~TestMenuDelegate() override;

  int execute_command_id() const { return execute_command_id_; }

  int on_menu_closed_called() const { return on_menu_closed_called_count_; }
  MenuItemView* on_menu_closed_menu() const { return on_menu_closed_menu_; }
  MenuRunner::RunResult on_menu_closed_run_result() const {
    return on_menu_closed_run_result_;
  }
  bool on_perform_drop_called() { return on_perform_drop_called_; }

  // MenuDelegate:
  void ExecuteCommand(int id) override;
  void OnMenuClosed(MenuItemView* menu, MenuRunner::RunResult result) override;
  int OnPerformDrop(MenuItemView* menu,
                    DropPosition position,
                    const ui::DropTargetEvent& event) override;

 private:
  // ID of last executed command.
  int execute_command_id_;

  // The number of times OnMenuClosed was called.
  int on_menu_closed_called_count_;

  // The values of the last call to OnMenuClosed.
  MenuItemView* on_menu_closed_menu_;
  MenuRunner::RunResult on_menu_closed_run_result_;

  bool on_perform_drop_called_;

  DISALLOW_COPY_AND_ASSIGN(TestMenuDelegate);
};

// Test api which can be used to hide the active MenuController, without
// performing normal shutdown.
class MenuControllerTestApi {
 public:
  MenuControllerTestApi();
  ~MenuControllerTestApi();

  // Tells the active MenuController to consider itself not showing.
  void Hide();

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuControllerTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_MENU_TEST_UTILS_H_
