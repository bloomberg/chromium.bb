// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/examples/window_type_launcher.h"

#include "base/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/examples/example_factory.h"
#include "ui/aura_shell/examples/toplevel_window.h"
#include "ui/aura_shell/toplevel_frame_view.h"
#include "ui/gfx/canvas.h"
#include "views/controls/button/text_button.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/menu_runner.h"
#include "views/widget/widget.h"

using views::MenuItemView;
using views::MenuRunner;

namespace aura_shell {
namespace examples {

void InitWindowTypeLauncher() {
  views::Widget* widget =
      views::Widget::CreateWindowWithBounds(new WindowTypeLauncher,
                                            gfx::Rect(120, 150, 400, 300));
  widget->GetNativeView()->set_name(ASCIIToUTF16("WindowTypeLauncher"));
  widget->Show();
}

WindowTypeLauncher::WindowTypeLauncher()
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          create_button_(new views::NativeTextButton(this, L"Create Window"))),
      ALLOW_THIS_IN_INITIALIZER_LIST(bubble_button_(
          new views::NativeTextButton(this, L"Create Pointy Bubble"))) {
  AddChildView(create_button_);
  AddChildView(bubble_button_);
  set_context_menu_controller(this);
}

WindowTypeLauncher::~WindowTypeLauncher() {
}

void WindowTypeLauncher::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRectInt(SK_ColorWHITE, 0, 0, width(), height());
}

void WindowTypeLauncher::Layout() {
  gfx::Size create_button_ps = create_button_->GetPreferredSize();
  gfx::Rect local_bounds = GetLocalBounds();
  create_button_->SetBounds(
      5, local_bounds.bottom() - create_button_ps.height() - 5,
      create_button_ps.width(), create_button_ps.height());

  gfx::Size bubble_button_ps = bubble_button_->GetPreferredSize();
  bubble_button_->SetBounds(
      5, create_button_->y() - bubble_button_ps.height() - 5,
      bubble_button_ps.width(), bubble_button_ps.height());
}

gfx::Size WindowTypeLauncher::GetPreferredSize() {
  return gfx::Size(300, 500);
}

bool WindowTypeLauncher::OnMousePressed(const views::MouseEvent& event) {
  // Overriden so we get OnMouseReleased and can show the context menu.
  return true;
}

views::View* WindowTypeLauncher::GetContentsView() {
  return this;
}

bool WindowTypeLauncher::CanResize() const {
  return true;
}

std::wstring WindowTypeLauncher::GetWindowTitle() const {
  return L"Examples: Window Builder";
}

views::NonClientFrameView* WindowTypeLauncher::CreateNonClientFrameView() {
  return new aura_shell::internal::ToplevelFrameView;
}

void WindowTypeLauncher::ButtonPressed(views::Button* sender,
                                       const views::Event& event) {
  if (sender == create_button_) {
    ToplevelWindow::CreateToplevelWindow();
  } else if (sender == bubble_button_) {
    gfx::Point origin = bubble_button_->bounds().origin();
    views::View::ConvertPointToWidget(bubble_button_->parent(), &origin);
    origin.Offset(10, bubble_button_->height() - 10);
    CreatePointyBubble(GetWidget()->GetNativeWindow(), origin);
  }
}

void WindowTypeLauncher::ExecuteCommand(int id) {
  DCHECK_EQ(id, COMMAND_NEW_WINDOW);
  InitWindowTypeLauncher();
}

void WindowTypeLauncher::ShowContextMenuForView(views::View* source,
                                                const gfx::Point& p,
                                                bool is_mouse_gesture) {
  MenuItemView* root = new MenuItemView(this);
  root->AppendMenuItem(COMMAND_NEW_WINDOW, L"New Window", MenuItemView::NORMAL);
  // MenuRunner takes ownership of root.
  menu_runner_.reset(new MenuRunner(root));
  menu_runner_->RunMenuAt(
      GetWidget(), NULL, gfx::Rect(p, gfx::Size(0, 0)),
      MenuItemView::TOPLEFT, MenuRunner::HAS_MNEMONICS);
}

}  // namespace examples
}  // namespace aura_shell
