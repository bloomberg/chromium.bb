// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/examples/window_type_launcher.h"

#include "base/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/examples/toplevel_window.h"
#include "ui/gfx/canvas.h"
#include "views/controls/button/text_button.h"
#include "views/widget/widget.h"

namespace aura_shell {
namespace examples {

WindowTypeLauncher::WindowTypeLauncher()
    : ALLOW_THIS_IN_INITIALIZER_LIST(
    create_button_(new views::NativeTextButton(this, L"Create Window"))) {
  AddChildView(create_button_);
}

WindowTypeLauncher::~WindowTypeLauncher() {
}

void WindowTypeLauncher::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRectInt(SK_ColorWHITE, 0, 0, width(), height());
}

void WindowTypeLauncher::Layout() {
  gfx::Size button_ps = create_button_->GetPreferredSize();
  gfx::Rect local_bounds = GetLocalBounds();
  create_button_->SetBounds(5, local_bounds.bottom() - button_ps.height() - 5,
                            button_ps.width(), button_ps.height());
}

gfx::Size WindowTypeLauncher::GetPreferredSize() {
  return gfx::Size(300, 500);
}

views::View* WindowTypeLauncher::GetContentsView() {
  return this;
}

std::wstring WindowTypeLauncher::GetWindowTitle() const {
  return L"Examples: Window Builder";
}

void WindowTypeLauncher::ButtonPressed(views::Button* sender,
                                       const views::Event& event) {
  if (sender == create_button_)
    ToplevelWindow::CreateToplevelWindow();
}

void InitWindowTypeLauncher() {
  views::Widget* widget =
      views::Widget::CreateWindowWithBounds(new WindowTypeLauncher,
                                            gfx::Rect(120, 150, 400, 300));
  widget->GetNativeView()->set_name(ASCIIToUTF16("WindowTypeLauncher"));
  widget->Show();
}

}  // namespace examples
}  // namespace aura_shell
