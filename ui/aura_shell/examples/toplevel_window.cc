// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/examples/toplevel_window.h"

#include "base/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/toplevel_frame_view.h"
#include "ui/gfx/canvas.h"
#include "views/widget/widget.h"

namespace aura_shell {
namespace examples {

ToplevelWindow::CreateParams::CreateParams()
    : can_resize(false),
      can_maximize(false) {
}

// static
void ToplevelWindow::CreateToplevelWindow(const CreateParams& params) {
  views::Widget* widget =
      views::Widget::CreateWindowWithBounds(new ToplevelWindow(params),
                                            gfx::Rect(120, 150, 400, 300));
  widget->GetNativeView()->set_name("Examples:ToplevelWindow");
  widget->Show();
}

ToplevelWindow::ToplevelWindow(const CreateParams& params) : params_(params) {
}

ToplevelWindow::~ToplevelWindow() {
}

void ToplevelWindow::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRect(SK_ColorDKGRAY, GetLocalBounds());
}

string16 ToplevelWindow::GetWindowTitle() const {
  return ASCIIToUTF16("Examples: Toplevel Window");
}

views::View* ToplevelWindow::GetContentsView() {
  return this;
}

bool ToplevelWindow::CanResize() const {
  return params_.can_resize;
}

bool ToplevelWindow::CanMaximize() const {
  return params_.can_maximize;
}

views::NonClientFrameView* ToplevelWindow::CreateNonClientFrameView() {
  return new aura_shell::internal::ToplevelFrameView;
}

}  // namespace examples
}  // namespace aura_shell
