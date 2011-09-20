// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_EXAMPLES_WINDOW_TYPE_LAUNCHER_H_
#define UI_AURA_SHELL_EXAMPLES_WINDOW_TYPE_LAUNCHER_H_
#pragma once

#include "views/widget/widget_delegate.h"
#include "views/controls/button/button.h"

namespace views {
class NativeTextButton;
}

namespace aura_shell {
namespace examples {

// The contents view/delegate of a window that shows some buttons that create
// various window types.
class WindowTypeLauncher : public views::WidgetDelegateView,
                           public views::ButtonListener {
 public:
  WindowTypeLauncher();
  virtual ~WindowTypeLauncher();

 private:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE;
  virtual std::wstring GetWindowTitle() const OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  views::NativeTextButton* create_button_;
  views::NativeTextButton* bubble_button_;

  DISALLOW_COPY_AND_ASSIGN(WindowTypeLauncher);
};

}  // namespace examples
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_EXAMPLES_WINDOW_TYPE_LAUNCHER_H_
