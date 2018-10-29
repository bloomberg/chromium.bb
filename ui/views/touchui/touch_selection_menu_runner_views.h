// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TOUCHUI_TOUCH_SELECTION_MENU_RUNNER_VIEWS_H_
#define UI_VIEWS_TOUCHUI_TOUCH_SELECTION_MENU_RUNNER_VIEWS_H_

#include <memory>

#include "base/macros.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/views_export.h"

namespace views {
class LabelButton;
class Widget;

// Views implementation for TouchSelectionMenuRunner.
class VIEWS_EXPORT TouchSelectionMenuRunnerViews
    : public ui::TouchSelectionMenuRunner {
 public:
  class Menu;

  // Test API to access internal state in tests.
  class VIEWS_EXPORT TestApi {
   public:
    explicit TestApi(TouchSelectionMenuRunnerViews* menu_runner);
    ~TestApi();

    gfx::Rect GetAnchorRect() const;
    LabelButton* GetFirstButton() const;
    Widget* GetWidget() const;

   private:
    TouchSelectionMenuRunnerViews* menu_runner_;

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  TouchSelectionMenuRunnerViews();
  ~TouchSelectionMenuRunnerViews() override;

 protected:
  // Sets the menu as the currently runner menu and shows it.
  void ShowMenu(Menu* menu,
                const gfx::Rect& anchor_rect,
                const gfx::Size& handle_image_size);

  // ui::TouchSelectionMenuRunner:
  bool IsMenuAvailable(
      const ui::TouchSelectionMenuClient* client) const override;
  void CloseMenu() override;
  void OpenMenu(ui::TouchSelectionMenuClient* client,
                const gfx::Rect& anchor_rect,
                const gfx::Size& handle_image_size,
                aura::Window* context) override;
  bool IsRunning() const override;

 private:
  // A pointer to the currently running menu, or |nullptr| if no menu is
  // running. The menu manages its own lifetime and deletes itself when closed.
  Menu* menu_;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionMenuRunnerViews);
};

// A bubble that contains actions available for the selected text. An object of
// this type, as a BubbleDialogDelegateView, manages its own lifetime.
class TouchSelectionMenuRunnerViews::Menu : public BubbleDialogDelegateView,
                                            public ButtonListener {
 public:
  Menu(TouchSelectionMenuRunnerViews* owner,
       ui::TouchSelectionMenuClient* client,
       aura::Window* context);

  void ShowMenu(const gfx::Rect& anchor_rect,
                const gfx::Size& handle_image_size);

  // Checks whether there is any command available to show in the menu.
  static bool IsMenuAvailable(const ui::TouchSelectionMenuClient* client);

  // Closes the menu. This will eventually self-destroy the object.
  void CloseMenu();

 protected:
  ~Menu() override;

  // Queries the |client_| for what commands to show in the menu and sizes the
  // menu appropriately.
  virtual void CreateButtons();

  // Helper method to create a single button.
  LabelButton* CreateButton(const base::string16& title, int tag);

  // ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

 private:
  friend class TouchSelectionMenuRunnerViews::TestApi;

  // Helper to disconnect this menu object from its owning menu runner.
  void DisconnectOwner();

  // BubbleDialogDelegateView:
  void OnPaint(gfx::Canvas* canvas) override;
  void WindowClosing() override;
  int GetDialogButtons() const override;

  TouchSelectionMenuRunnerViews* owner_;
  ui::TouchSelectionMenuClient* const client_;

  DISALLOW_COPY_AND_ASSIGN(Menu);
};

}  // namespace views

#endif  // UI_VIEWS_TOUCHUI_TOUCH_SELECTION_MENU_RUNNER_VIEWS_H_
