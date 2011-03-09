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

Window::Window(WindowDelegate* window_delegate)
    : native_window_(NULL),
      window_delegate_(window_delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          non_client_view_(new NonClientView(this))),
      saved_maximized_state_(false),
      minimum_size_(100, 100),
      disable_inactive_rendering_(false) {
  DCHECK(window_delegate_);
  DCHECK(!window_delegate_->window_);
  window_delegate_->window_ = this;
}

Window::~Window() {
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

// static
void Window::CloseSecondaryWidget(Widget* widget) {
  if (!widget)
    return;

  // Close widget if it's identified as a secondary window.
  Window* window = widget->GetWindow();
  if (window) {
    if (!window->IsAppWindow())
      window->Close();
  } else {
    // If it's not a Window, then close it anyway since it probably is
    // secondary.
    widget->Close();
  }
}

gfx::Rect Window::GetBounds() const {
  return gfx::Rect();
}

gfx::Rect Window::GetNormalBounds() const {
  return gfx::Rect();
}

void Window::SetWindowBounds(const gfx::Rect& bounds,
                             gfx::NativeWindow other_window) {
}

void Window::Show() {
  native_window_->ShowNativeWindow(
      saved_maximized_state_ ? NativeWindow::SHOW_MAXIMIZED
                             : NativeWindow::SHOW_RESTORED);
  // |saved_maximized_state_| only applies the first time the window is shown.
  // If we don't reset the value the window will be shown maximized every time
  // it is subsequently shown after being hidden.
  saved_maximized_state_ = false;
}

void Window::HideWindow() {
}

void Window::SetNativeWindowProperty(const char* name, void* value) {
}

void* Window::GetNativeWindowProperty(const char* name) {
  return NULL;
}

void Window::DisableInactiveRendering() {
  disable_inactive_rendering_ = true;
  non_client_view_->DisableInactiveRendering(disable_inactive_rendering_);
}

void Window::Activate() {
}

void Window::Deactivate() {
}

void Window::Close() {
}

void Window::Maximize() {
}

void Window::Minimize() {
}

void Window::Restore() {
}

bool Window::IsActive() const {
  return false;
}

bool Window::IsVisible() const {
  return false;
}

bool Window::IsMaximized() const {
  return false;
}

bool Window::IsMinimized() const {
  return false;
}

void Window::SetFullscreen(bool fullscreen) {
}

bool Window::IsFullscreen() const {
  return false;
}

void Window::SetUseDragFrame(bool use_drag_frame) {
}

bool Window::IsAppWindow() const {
  return false;
}

void Window::EnableClose(bool enable) {
}

void Window::UpdateWindowTitle() {
  // If the non-client view is rendering its own title, it'll need to relayout
  // now.
  non_client_view_->Layout();

  // Update the native frame's text. We do this regardless of whether or not
  // the native frame is being used, since this also updates the taskbar, etc.
  std::wstring window_title;
  if (native_window_->AsNativeWidget()->IsScreenReaderActive())
    window_title = window_delegate_->GetAccessibleWindowTitle();
  else
    window_title = window_delegate_->GetWindowTitle();
  base::i18n::AdjustStringForLocaleDirection(&window_title);
  native_window_->SetWindowTitle(window_title);
}

void Window::UpdateWindowIcon() {
  non_client_view_->UpdateWindowIcon();
  native_window_->SetWindowIcons(window_delegate_->GetWindowIcon(),
                                 window_delegate_->GetWindowAppIcon());
}

void Window::SetIsAlwaysOnTop(bool always_on_top) {
}

NonClientFrameView* Window::CreateFrameViewForWindow() {
  return NULL;
}

void Window::UpdateFrameAfterFrameChange() {
}

gfx::NativeWindow Window::GetNativeWindow() const {
  return NULL;
}

bool Window::ShouldUseNativeFrame() const {
  return false;
}

void Window::FrameTypeChanged() {
}

////////////////////////////////////////////////////////////////////////////////
// Window, internal::NativeWindowDelegate implementation:

bool Window::IsInactiveRenderingDisabled() const {
  return disable_inactive_rendering_;
}

void Window::EnableInactiveRendering() {
  disable_inactive_rendering_ = false;
  non_client_view_->DisableInactiveRendering(false);
}

bool Window::IsModal() const {
  return window_delegate_->IsModal();
}

void Window::OnNativeWindowCreated(const gfx::Rect& bounds) {
  if (window_delegate_->IsModal())
    native_window_->BecomeModal();

  // Create the ClientView, add it to the NonClientView and add the
  // NonClientView to the RootView. This will cause everything to be parented.
  non_client_view_->set_client_view(window_delegate_->CreateClientView(this));
  // TODO(beng): make simpler once Window subclasses Widget.
  native_window_->AsNativeWidget()->GetWidget()->SetContentsView(
      non_client_view_);

  UpdateWindowTitle();
  native_window_->SetAccessibleRole(window_delegate_->accessible_role());
  native_window_->SetAccessibleState(window_delegate_->accessible_state());

  SetInitialBounds(bounds);
}

void Window::OnWindowDestroying() {
  non_client_view_->WindowClosing();
  window_delegate_->WindowClosing();
}

void Window::OnWindowDestroyed() {
  window_delegate_->DeleteDelegate();
  window_delegate_ = NULL;
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
  window_delegate_->GetSavedMaximizedState(&saved_maximized_state_);

  // Restore the window's placement from the controller.
  gfx::Rect saved_bounds = bounds;
  if (window_delegate_->GetSavedWindowBounds(&saved_bounds)) {
    if (!window_delegate_->ShouldRestoreWindowSize()) {
      saved_bounds.set_size(non_client_view_->GetPreferredSize());
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
    // TODO(beng): Should be able to call Widget::SetBounds() directly once
    //             Window subclasses Widget.
    native_window_->AsNativeWidget()->GetWidget()->SetBounds(saved_bounds);
  } else {
    if (bounds.IsEmpty()) {
      // No initial bounds supplied, so size the window to its content and
      // center over its parent.
      native_window_->CenterWindow(non_client_view_->GetPreferredSize());
    } else {
      // Use the supplied initial bounds.
      SetWindowBounds(bounds, NULL);
    }
  }
}

}  // namespace views
