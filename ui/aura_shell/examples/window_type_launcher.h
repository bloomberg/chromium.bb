// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_EXAMPLES_WINDOW_TYPE_LAUNCHER_H_
#define UI_AURA_SHELL_EXAMPLES_WINDOW_TYPE_LAUNCHER_H_
#pragma once

#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/widget/widget_delegate.h"
#include "views/context_menu_controller.h"

namespace views {
class MenuRunner;
class NativeTextButton;
}

namespace aura_shell {
namespace examples {

// The contents view/delegate of a window that shows some buttons that create
// various window types.
class WindowTypeLauncher : public views::WidgetDelegateView,
                           public views::ButtonListener,
                           public views::MenuDelegate,
                           public views::ContextMenuController {
 public:
  WindowTypeLauncher();
  virtual ~WindowTypeLauncher();

 private:
  typedef std::pair<aura::Window*, gfx::Rect> WindowAndBoundsPair;

  enum MenuCommands {
    COMMAND_NEW_WINDOW = 1,
    COMMAND_TOGGLE_FULLSCREEN = 3,
  };

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE;
  virtual bool CanResize() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Overridden from views::MenuDelegate:
  virtual void ExecuteCommand(int id) OVERRIDE;

  // Override from views::ContextMenuController:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& p,
                                      bool is_mouse_gesture) OVERRIDE;

  views::NativeTextButton* create_button_;
  views::NativeTextButton* create_nonresizable_button_;
  views::NativeTextButton* bubble_button_;
  views::NativeTextButton* lock_button_;
  views::NativeTextButton* widgets_button_;
  views::NativeTextButton* modal_button_;
  views::NativeTextButton* transient_button_;
  views::NativeTextButton* examples_button_;
  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(WindowTypeLauncher);
};

}  // namespace examples
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_EXAMPLES_WINDOW_TYPE_LAUNCHER_H_
