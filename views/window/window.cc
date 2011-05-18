// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/window.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
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
      window_delegate_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          non_client_view_(new NonClientView(this))),
      saved_maximized_state_(false),
      minimum_size_(100, 100),
      disable_inactive_rendering_(false),
      window_closed_(false),
      frame_type_(FRAME_TYPE_DEFAULT) {
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
  window_delegate_ = params.window_delegate;
  AsWidget()->set_widget_delegate(window_delegate_);
  DCHECK(window_delegate_);
  DCHECK(!window_delegate_->window_);
  window_delegate_->window_ = this;
  set_widget_delegate(window_delegate_);
  native_window_ =
      params.native_window ? params.native_window
                           : NativeWindow::CreateNativeWindow(this);
  // If frame_view was set already, don't replace it with default one.
  if (!non_client_view()->frame_view())
    non_client_view()->SetFrameView(CreateFrameViewForWindow());
  InitParams modified_params = params;
  modified_params.widget_init_params.native_widget =
      native_window_->AsNativeWidget();
  Init(modified_params.widget_init_params);
  OnNativeWindowCreated(modified_params.widget_init_params.bounds);
}

gfx::Rect Window::GetBounds() const {
  return GetWindowScreenBounds();
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

void Window::ShowInactive() {
  native_window_->ShowNativeWindow(NativeWindow::SHOW_INACTIVE);
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

void Window::Close() {
  if (window_closed_) {
    // It appears we can hit this code path if you close a modal dialog then
    // close the last browser before the destructor is hit, which triggers
    // invoking Close again.
    return;
  }

  if (non_client_view_->CanClose()) {
    SaveWindowPosition();
    Widget::Close();
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
  string16 window_title;
  if (native_window_->AsNativeWidget()->IsScreenReaderActive())
    window_title = WideToUTF16(window_delegate_->GetAccessibleWindowTitle());
  else
    window_title = WideToUTF16(window_delegate_->GetWindowTitle());
  base::i18n::AdjustStringForLocaleDirection(&window_title);
  native_window_->SetWindowTitle(UTF16ToWide(window_title));
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
  if (frame_type_ != FRAME_TYPE_DEFAULT)
    return frame_type_ == FRAME_TYPE_FORCE_NATIVE;
  return native_window_->ShouldUseNativeFrame();
}

void Window::DebugToggleFrameType() {
  if (frame_type_ == FRAME_TYPE_DEFAULT) {
    frame_type_ = ShouldUseNativeFrame() ? FRAME_TYPE_FORCE_CUSTOM :
        FRAME_TYPE_FORCE_NATIVE;
  } else {
    frame_type_ = frame_type_ == FRAME_TYPE_FORCE_CUSTOM ?
        FRAME_TYPE_FORCE_NATIVE : FRAME_TYPE_FORCE_CUSTOM;
  }
  FrameTypeChanged();
}

void Window::FrameTypeChanged() {
  native_window_->FrameTypeChanged();
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
  SetContentsView(non_client_view_);

  UpdateWindowTitle();
  native_window_->SetAccessibleRole(
      window_delegate_->GetAccessibleWindowRole());
  native_window_->SetAccessibleState(
      window_delegate_->GetAccessibleWindowState());

  SetInitialBounds(bounds);
}

void Window::OnNativeWindowActivationChanged(bool active) {
  if (!active)
    SaveWindowPosition();
  window_delegate_->OnWindowActivationChanged(active);
}

void Window::OnNativeWindowBeginUserBoundsChange() {
  window_delegate_->OnWindowBeginUserBoundsChange();
}

void Window::OnNativeWindowEndUserBoundsChange() {
  window_delegate_->OnWindowEndUserBoundsChange();
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

Window* Window::AsWindow() {
  return this;
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
    SetBounds(saved_bounds);
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
