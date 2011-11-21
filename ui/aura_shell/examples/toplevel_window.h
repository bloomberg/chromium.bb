// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_EXAMPLES_TOPLEVEL_WINDOW_H_
#define UI_AURA_SHELL_EXAMPLES_TOPLEVEL_WINDOW_H_
#pragma once

#include "views/widget/widget_delegate.h"

namespace aura_shell {
namespace examples {

class ToplevelWindow : public views::WidgetDelegateView {
 public:
  struct CreateParams {
    CreateParams();

    bool can_resize;
    bool can_maximize;
  };
  static void CreateToplevelWindow(const CreateParams& params);

 private:
  explicit ToplevelWindow(const CreateParams& params);
  virtual ~ToplevelWindow();

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual View* GetContentsView() OVERRIDE;
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

  const CreateParams params_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelWindow);
};

}  // namespace examples
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_EXAMPLES_TOPLEVEL_WINDOW_H_
