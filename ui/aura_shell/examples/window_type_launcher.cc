// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/examples/window_type_launcher.h"

#include "base/utf_string_conversions.h"
#include "ui/aura/desktop.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/examples/example_factory.h"
#include "ui/aura_shell/examples/toplevel_window.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/aura_shell/toplevel_frame_view.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "views/controls/button/text_button.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/menu_runner.h"
#include "views/widget/widget.h"

using views::MenuItemView;
using views::MenuRunner;

namespace aura_shell {
namespace examples {

namespace {

typedef std::pair<aura::Window*, gfx::Rect> WindowAndBoundsPair;

void CalculateWindowBoundsAndScaleForTiling(
    const gfx::Size& containing_size,
    const aura::Window::Windows& windows,
    float* x_scale,
    float* y_scale,
    std::vector<WindowAndBoundsPair>* bounds) {
  *x_scale = 1.0f;
  *y_scale = 1.0f;
  int total_width = 0;
  int max_height = 0;
  int shown_window_count = 0;
  for (aura::Window::Windows::const_iterator i = windows.begin();
       i != windows.end(); ++i) {
    if ((*i)->visible()) {
      total_width += (*i)->bounds().width();
      max_height = std::max((*i)->bounds().height(), max_height);
      shown_window_count++;
    }
  }

  if (shown_window_count == 0)
    return;

  const int kPadding = 10;
  total_width += (shown_window_count - 1) * kPadding;
  if (total_width > containing_size.width()) {
    *x_scale = static_cast<float>(containing_size.width()) /
        static_cast<float>(total_width);
  }
  if (max_height > containing_size.height()) {
    *y_scale = static_cast<float>(containing_size.height()) /
        static_cast<float>(max_height);
  }
  *x_scale = *y_scale = std::min(*x_scale, *y_scale);

  int x = std::max(0, static_cast<int>(
      (containing_size.width() * - total_width * *x_scale) / 2));
  for (aura::Window::Windows::const_iterator i = windows.begin();
       i != windows.end();
       ++i) {
    if ((*i)->visible()) {
      const gfx::Rect& current_bounds((*i)->bounds());
      int y = (containing_size.height() -
               current_bounds.height() * *y_scale) / 2;
      bounds->push_back(std::make_pair(*i,
          gfx::Rect(x, y, current_bounds.width(), current_bounds.height())));
      x += static_cast<int>(
          static_cast<float>(current_bounds.width() + kPadding) * *x_scale);
    }
  }
}

}  // namespace

void InitWindowTypeLauncher() {
  views::Widget* widget =
      views::Widget::CreateWindowWithBounds(new WindowTypeLauncher,
                                            gfx::Rect(120, 150, 400, 300));
  widget->GetNativeView()->set_name("WindowTypeLauncher");
  widget->Show();
}

WindowTypeLauncher::WindowTypeLauncher()
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          create_button_(new views::NativeTextButton(this, L"Create Window"))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          create_nonresizable_button_(new views::NativeTextButton(
              this, L"Create Non-Resizable Window"))),
      ALLOW_THIS_IN_INITIALIZER_LIST(bubble_button_(
          new views::NativeTextButton(this, L"Create Pointy Bubble"))),
      ALLOW_THIS_IN_INITIALIZER_LIST(lock_button_(
          new views::NativeTextButton(this, L"Lock Screen"))),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  AddChildView(create_button_);
  AddChildView(create_nonresizable_button_);
  AddChildView(bubble_button_);
  AddChildView(lock_button_);
  set_context_menu_controller(this);
}

WindowTypeLauncher::~WindowTypeLauncher() {
}

void WindowTypeLauncher::TileWindows() {
  to_restore_.clear();
  aura::Window* window_container =
      aura::Desktop::GetInstance()->window()->GetChildById(
          internal::kShellWindowId_DefaultContainer);
  const aura::Window::Windows& windows = window_container->children();
  if (windows.empty())
    return;
  float x_scale = 1.0f;
  float y_scale = 1.0f;
  std::vector<WindowAndBoundsPair> bounds;
  CalculateWindowBoundsAndScaleForTiling(window_container->bounds().size(),
                                         windows, &x_scale, &y_scale, &bounds);
  if (bounds.empty())
    return;
  ui::Transform transform;
  transform.SetScale(x_scale, y_scale);
  for (size_t i = 0; i < bounds.size(); ++i) {
    to_restore_.push_back(
        std::make_pair(bounds[i].first, bounds[i].first->bounds()));
    bounds[i].first->layer()->SetAnimation(
        aura::Window::CreateDefaultAnimation());
    bounds[i].first->SetBounds(bounds[i].second);
    bounds[i].first->layer()->SetTransform(transform);
    bounds[i].first->layer()->SetOpacity(0.5f);
  }

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, method_factory_.NewRunnableMethod(
      &WindowTypeLauncher::RestoreTiledWindows), 2000);
}

void WindowTypeLauncher::RestoreTiledWindows() {
  ui::Transform identity_transform;
  for (size_t i = 0; i < to_restore_.size(); ++i) {
    to_restore_[i].first->layer()->SetAnimation(
        aura::Window::CreateDefaultAnimation());
    to_restore_[i].first->SetBounds(to_restore_[i].second);
    to_restore_[i].first->layer()->SetTransform(identity_transform);
    to_restore_[i].first->layer()->SetOpacity(1.0f);
  }
  to_restore_.clear();
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

  gfx::Size create_nr_button_ps =
      create_nonresizable_button_->GetPreferredSize();
  create_nonresizable_button_->SetBounds(
      5, bubble_button_->y() - create_nr_button_ps.height() - 5,
      create_nr_button_ps.width(), create_nr_button_ps.height());

  gfx::Size lock_ps = lock_button_->GetPreferredSize();
  lock_button_->SetBounds(
      5, create_nonresizable_button_->y() - lock_ps.height() - 5,
      lock_ps.width(), lock_ps.height());
}

gfx::Size WindowTypeLauncher::GetPreferredSize() {
  return gfx::Size(300, 500);
}

bool WindowTypeLauncher::OnMousePressed(const views::MouseEvent& event) {
  // Overridden so we get OnMouseReleased and can show the context menu.
  return true;
}

views::View* WindowTypeLauncher::GetContentsView() {
  return this;
}

bool WindowTypeLauncher::CanResize() const {
  return true;
}

string16 WindowTypeLauncher::GetWindowTitle() const {
  return ASCIIToUTF16("Examples: Window Builder");
}

views::NonClientFrameView* WindowTypeLauncher::CreateNonClientFrameView() {
  return new aura_shell::internal::ToplevelFrameView;
}

void WindowTypeLauncher::ButtonPressed(views::Button* sender,
                                       const views::Event& event) {
  if (sender == create_button_) {
    ToplevelWindow::CreateParams params;
    params.can_resize = true;
    ToplevelWindow::CreateToplevelWindow(params);
  } else if (sender == create_nonresizable_button_) {
    ToplevelWindow::CreateToplevelWindow(ToplevelWindow::CreateParams());
  } else if (sender == bubble_button_) {
    gfx::Point origin = bubble_button_->bounds().origin();
    views::View::ConvertPointToWidget(bubble_button_->parent(), &origin);
    origin.Offset(10, bubble_button_->height() - 10);
    CreatePointyBubble(GetWidget()->GetNativeWindow(), origin);
  } else if (sender == lock_button_) {
    CreateLock();
  }
}

void WindowTypeLauncher::ExecuteCommand(int id) {
  switch (id) {
    case COMMAND_NEW_WINDOW:
      InitWindowTypeLauncher();
      break;
    case COMMAND_TILE_WINDOWS:
      TileWindows();
      break;
    default:
      break;
  }
}

void WindowTypeLauncher::ShowContextMenuForView(views::View* source,
                                                const gfx::Point& p,
                                                bool is_mouse_gesture) {
  MenuItemView* root = new MenuItemView(this);
  root->AppendMenuItem(COMMAND_NEW_WINDOW, L"New Window", MenuItemView::NORMAL);
  root->AppendMenuItem(COMMAND_TILE_WINDOWS, L"Tile Windows",
                       MenuItemView::NORMAL);
  // MenuRunner takes ownership of root.
  menu_runner_.reset(new MenuRunner(root));
  if (menu_runner_->RunMenuAt(GetWidget(), NULL, gfx::Rect(p, gfx::Size(0, 0)),
        MenuItemView::TOPLEFT,
        MenuRunner::HAS_MNEMONICS) == MenuRunner::MENU_DELETED)
    return;
}

}  // namespace examples
}  // namespace aura_shell
