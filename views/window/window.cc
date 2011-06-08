// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/window.h"

#include "base/string_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_font_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "views/widget/widget.h"
#include "views/widget/native_widget.h"
#include "views/window/native_window.h"
#include "views/window/window_delegate.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Window, public:

Window::InitParams::InitParams(WindowDelegate* window_delegate)
    : window_delegate(window_delegate),
      parent_window(NULL),
      native_window(NULL),
      widget_init_params(Widget::InitParams::TYPE_WINDOW) {
}

Window::Window()
    : native_window_(NULL),
      saved_maximized_state_(false),
      minimum_size_(100, 100) {
}

Window::~Window() {
}

// static
Window* Window::CreateChromeWindow(gfx::NativeWindow parent,
                                   const gfx::Rect& bounds,
                                   WindowDelegate* window_delegate) {
  Window* window = new Window;
  Window::InitParams params(window_delegate);
  params.parent_window = parent;
#if defined(OS_WIN)
  params.widget_init_params.parent = parent;
#endif
  params.widget_init_params.bounds = bounds;
  window->InitWindow(params);
  return window;
}

// static
int Window::GetLocalizedContentsWidth(int col_resource_id) {
  return ui::GetLocalizedContentsWidthForFont(col_resource_id,
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont));
}

// static
int Window::GetLocalizedContentsHeight(int row_resource_id) {
  return ui::GetLocalizedContentsHeightForFont(row_resource_id,
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont));
}

// static
gfx::Size Window::GetLocalizedContentsSize(int col_resource_id,
                                           int row_resource_id) {
  return gfx::Size(GetLocalizedContentsWidth(col_resource_id),
                   GetLocalizedContentsHeight(row_resource_id));
}

void Window::InitWindow(const InitParams& params) {
  native_window_ =
      params.native_window ? params.native_window
                           : NativeWindow::CreateNativeWindow(this);
  InitParams modified_params = params;
  modified_params.widget_init_params.delegate = params.window_delegate;
  DCHECK(!modified_params.widget_init_params.delegate->window_);
  modified_params.widget_init_params.delegate->window_ = this;
  modified_params.widget_init_params.native_widget =
      native_window_->AsNativeWidget();
  Init(modified_params.widget_init_params);
  OnNativeWindowCreated(modified_params.widget_init_params.bounds);
}

////////////////////////////////////////////////////////////////////////////////
// Window, Widget overrides:

void Window::Show() {
  native_window_->AsNativeWidget()->ShowNativeWidget(
      saved_maximized_state_ ? NativeWidget::SHOW_MAXIMIZED
          : NativeWidget::SHOW_RESTORED);
  // |saved_maximized_state_| only applies the first time the window is shown.
  // If we don't reset the value the window will be shown maximized every time
  // it is subsequently shown after being hidden.
  saved_maximized_state_ = false;
}

Window* Window::AsWindow() {
  return this;
}

const Window* Window::AsWindow() const {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// Window, internal::NativeWindowDelegate implementation:

bool Window::IsModal() const {
  return widget_delegate()->IsModal();
}

bool Window::IsDialogBox() const {
  return !!widget_delegate()->AsDialogDelegate();
}

void Window::OnNativeWindowCreated(const gfx::Rect& bounds) {
  if (widget_delegate()->IsModal())
    native_window_->BecomeModal();

  UpdateWindowTitle();
  SetInitialBounds(bounds);
}

internal::NativeWidgetDelegate* Window::AsNativeWidgetDelegate() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// Window, private:

void Window::SetInitialBounds(const gfx::Rect& bounds) {
  // First we obtain the window's saved show-style and store it. We need to do
  // this here, rather than in Show() because by the time Show() is called,
  // the window's size will have been reset (below) and the saved maximized
  // state will have been lost. Sadly there's no way to tell on Windows when
  // a window is restored from maximized state, so we can't more accurately
  // track maximized state independently of sizing information.
  widget_delegate()->GetSavedMaximizedState(
      &saved_maximized_state_);

  // Restore the window's placement from the controller.
  gfx::Rect saved_bounds = bounds;
  if (widget_delegate()->GetSavedWindowBounds(&saved_bounds)) {
    if (!widget_delegate()->ShouldRestoreWindowSize()) {
      saved_bounds.set_size(non_client_view()->GetPreferredSize());
    } else {
      // Make sure the bounds are at least the minimum size.
      if (saved_bounds.width() < minimum_size_.width()) {
        saved_bounds.SetRect(saved_bounds.x(), saved_bounds.y(),
                             saved_bounds.right() + minimum_size_.width() -
                                 saved_bounds.width(),
                             saved_bounds.bottom());
      }

      if (saved_bounds.height() < minimum_size_.height()) {
        saved_bounds.SetRect(saved_bounds.x(), saved_bounds.y(),
                             saved_bounds.right(),
                             saved_bounds.bottom() + minimum_size_.height() -
                                 saved_bounds.height());
      }
    }

    // Widget's SetBounds method does not further modify the bounds that are
    // passed to it.
    SetBounds(saved_bounds);
  } else {
    if (bounds.IsEmpty()) {
      // No initial bounds supplied, so size the window to its content and
      // center over its parent.
      native_window_->AsNativeWidget()->CenterWindow(
          non_client_view()->GetPreferredSize());
    } else {
      // Use the supplied initial bounds.
      SetBoundsConstrained(bounds, NULL);
    }
  }
}

}  // namespace views
