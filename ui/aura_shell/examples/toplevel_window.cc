// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/examples/toplevel_window.h"

#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"
#include "views/widget/widget.h"

namespace aura_shell {
namespace examples {

// static
void ToplevelWindow::CreateToplevelWindow() {
  views::Widget* widget =
      views::Widget::CreateWindowWithBounds(new ToplevelWindow,
                                            gfx::Rect(120, 150, 400, 300));
  widget->GetNativeView()->set_name(L"Examples:ToplevelWindow");
  widget->Show();
}

ToplevelWindow::ToplevelWindow() {
}

ToplevelWindow::~ToplevelWindow() {
}

void ToplevelWindow::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRectInt(SK_ColorDKGRAY, 0, 0, width(), height());
}

std::wstring ToplevelWindow::GetWindowTitle() const {
  return L"Examples: Toplevel Window";
}

views::View* ToplevelWindow::GetContentsView() {
  return this;
}

}  // namespace examples
}  // namespace aura_shell
