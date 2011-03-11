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
      disable_inactive_rendering_(false),
      window_closed_(false) {
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
      window->CloseWindow();
  } else {
    // If it's not a Window, then close it anyway since it probably is
    // secondary.
    widget->Close();
  }
}

gfx::Rect Window::GetBounds() const {
  // TODO(beng): Clean this up once Window subclasses Widget.
  return native_window_->AsNativeWidget()->GetWidget()->GetWindowScreenBounds();
}

gfx::Rect Window::GetNormalBounds() const {
  return native_window_->GetRestoredBounds();
}

void Window::SetWindowBounds(const gfx::Rect& bounds,
                             gfx::NativeWindow other_window) {
  native_window_->SetWindowBounds(bounds, other_window);
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
  native_window_->HideWindow();
}

void Window::DisableInactiveRendering() {
  disable_inactive_rendering_ = true;
  non_client_view_->DisableInactiveRendering(disable_inactive_rendering_);
}

void Window::Activate() {
  native_window_->Activate();
}

void Window::Deactivate() {
  native_window_->Deactivate();
}

void Window::CloseWindow() {
  if (window_closed_) {
    // It appears we can hit this code path if you close a modal dialog then
    // close the last browser before the destructor is hit, which triggers
    // invoking Close again.
    return;
  }

  if (non_client_view_->CanClose()) {
    SaveWindowPosition();
    // TODO(beng): This can be simplified to Widget::Close() once Window
    //             subclasses Widget.
    native_window_->AsNativeWidget()->GetWidget()->Close();
    window_closed_ = true;
  }
}

void Window::Maximize() {
  native_window_->Maximize();
}

void Window::Minimize() {
  native_window_->Minimize();
}

void Window::Restore() {
  native_window_->Restore();
}

bool Window::IsActive() const {
  return native_window_->IsActive();
}

bool Window::IsVisible() const {
  return native_window_->IsVisible();
}

bool Window::IsMaximized() const {
  return native_window_->IsMaximized();
}

bool Window::IsMinimized() const {
  return native_window_->IsMinimized();
}

void Window::SetFullscreen(bool fullscreen) {
  native_window_->SetFullscreen(fullscreen);
}

bool Window::IsFullscreen() const {
  return native_window_->IsFullscreen();
}

void Window::SetUseDragFrame(bool use_drag_frame) {
  native_window_->SetUseDragFrame(use_drag_frame);
}

bool Window::IsAppWindow() const {
  return native_window_->IsAppWindow();
}

void Window::EnableClose(bool enable) {
  non_client_view_->EnableClose(enable);
  native_window_->EnableClose(enable);
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
  native_window_->SetAlwaysOnTop(always_on_top);
}

NonClientFrameView* Window::CreateFrameViewForWindow() {
  return native_window_->CreateFrameViewForWindow();
}

void Window::UpdateFrameAfterFrameChange() {
  native_window_->UpdateFrameAfterFrameChange();
}

gfx::NativeWindow Window::GetNativeWindow() const {
  return native_window_->GetNativeWindow();
}

bool Window::ShouldUseNativeFrame() const {
  return native_window_->ShouldUseNativeFrame();
}

void Window::FrameTypeChanged() {
  native_window_->FrameTypeChanged();
}

Widget* Window::AsWidget() {
  return const_cast<Widget*>(const_cast<const Window*>(this)->AsWidget());
}

const Widget* Window::AsWidget() const {
  return native_window_->AsNativeWidget()->GetWidget();
}

////////////////////////////////////////////////////////////////////////////////
// Window, internal::NativeWindowDelegate implementation:

bool Window::CanActivate() const {
  return window_delegate_->CanActivate();
}

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

bool Window::IsDialogBox() const {
  return !!window_delegate_->AsDialogDelegate();
}

bool Window::IsUsingNativeFrame() const {
  return non_client_view_->UseNativeFrame();
}

gfx::Size Window::GetMinimumSize() const {
  return non_client_view_->GetMinimumSize();
}

int Window::GetNonClientComponent(const gfx::Point& point) const {
  return non_client_view_->NonClientHitTest(point);
}

bool Window::ExecuteCommand(int command_id) {
  return window_delegate_->ExecuteWindowsCommand(command_id);
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

void Window::OnNativeWindowActivationChanged(bool active) {
  if (!active)
    SaveWindowPosition();
  window_delegate_->OnWindowActivate(active);
}

void Window::OnNativeWindowDestroying() {
  non_client_view_->WindowClosing();
  window_delegate_->WindowClosing();
}

void Window::OnNativeWindowDestroyed() {
  window_delegate_->DeleteDelegate();
  window_delegate_ = NULL;
}

void Window::OnNativeWindowBoundsChanged() {
  SaveWindowPosition();
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

void Window::SaveWindowPosition() {
  // The window delegate does the actual saving for us. It seems like (judging
  // by go/crash) that in some circumstances we can end up here after
  // WM_DESTROY, at which point the window delegate is likely gone. So just
  // bail.
  if (!window_delegate_)
    return;

  bool maximized;
  gfx::Rect bounds;
  native_window_->GetWindowBoundsAndMaximizedState(&bounds, &maximized);
  window_delegate_->SaveWindowPlacement(bounds, maximized);
}

}  // namespace views
