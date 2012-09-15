// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_root_window_host_win.h"

#include "ui/aura/desktop/desktop_activation_client.h"
#include "ui/aura/desktop/desktop_dispatcher_client.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/compound_event_filter.h"
#include "ui/views/ime/input_method_win.h"
#include "ui/views/widget/desktop_capture_client.h"
#include "ui/views/win/hwnd_message_handler.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWin, public:

DesktopRootWindowHostWin::DesktopRootWindowHostWin(
    internal::NativeWidgetDelegate* native_widget_delegate,
    const gfx::Rect& initial_bounds)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          message_handler_(new HWNDMessageHandler(this))),
      native_widget_delegate_(native_widget_delegate),
      root_window_host_delegate_(NULL),
      content_window_(NULL) {
}

DesktopRootWindowHostWin::~DesktopRootWindowHostWin() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWin, DesktopRootWindowHost implementation:

void DesktopRootWindowHostWin::Init(aura::Window* content_window,
                                    const Widget::InitParams& params) {
  // TODO(beng): SetInitParams().
  content_window_ = content_window;
  message_handler_->Init(NULL, params.bounds);

  aura::RootWindow::CreateParams rw_params(params.bounds);
  rw_params.host = this;
  root_window_.reset(new aura::RootWindow(rw_params));
  root_window_->Init();
  root_window_->AddChild(content_window_);
  root_window_host_delegate_ = root_window_.get();

  native_widget_delegate_->OnNativeWidgetCreated();

  capture_client_.reset(new DesktopCaptureClient);
  aura::client::SetCaptureClient(root_window_.get(), capture_client_.get());

  focus_manager_.reset(new aura::FocusManager);
  root_window_->set_focus_manager(focus_manager_.get());

  activation_client_.reset(
      new aura::DesktopActivationClient(root_window_->GetFocusManager()));
  aura::client::SetActivationClient(root_window_.get(),
                                    activation_client_.get());

  dispatcher_client_.reset(new aura::DesktopDispatcherClient);
  aura::client::SetDispatcherClient(root_window_.get(),
                                    dispatcher_client_.get());

  // CEF sets focus to the window the user clicks down on.
  // TODO(beng): see if we can't do this some other way. CEF seems a heavy-
  //             handed way of accomplishing focus.
  root_window_->SetEventFilter(new aura::shared::CompoundEventFilter);
}

void DesktopRootWindowHostWin::Close() {
  message_handler_->Close();
}

void DesktopRootWindowHostWin::CloseNow() {
  message_handler_->CloseNow();
}

aura::RootWindowHost* DesktopRootWindowHostWin::AsRootWindowHost() {
  return this;
}

void DesktopRootWindowHostWin::ShowWindowWithState(
    ui::WindowShowState show_state) {
  message_handler_->ShowWindowWithState(show_state);
}

void DesktopRootWindowHostWin::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  message_handler_->ShowMaximizedWithBounds(restored_bounds);
}

bool DesktopRootWindowHostWin::IsVisible() const {
  return message_handler_->IsVisible();
}

void DesktopRootWindowHostWin::SetSize(const gfx::Size& size) {
  message_handler_->SetSize(size);
}

void DesktopRootWindowHostWin::CenterWindow(const gfx::Size& size) {
  message_handler_->CenterWindow(size);
}

void DesktopRootWindowHostWin::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  message_handler_->GetWindowPlacement(bounds, show_state);
}

gfx::Rect DesktopRootWindowHostWin::GetWindowBoundsInScreen() const {
  return message_handler_->GetWindowBoundsInScreen();
}

gfx::Rect DesktopRootWindowHostWin::GetClientAreaBoundsInScreen() const {
  return message_handler_->GetClientAreaBoundsInScreen();
}

gfx::Rect DesktopRootWindowHostWin::GetRestoredBounds() const {
  return message_handler_->GetRestoredBounds();
}

void DesktopRootWindowHostWin::Activate() {
  message_handler_->Activate();
}

void DesktopRootWindowHostWin::Deactivate() {
  message_handler_->Deactivate();
}

bool DesktopRootWindowHostWin::IsActive() const {
  return message_handler_->IsActive();
}

void DesktopRootWindowHostWin::Maximize() {
  message_handler_->Maximize();
}

void DesktopRootWindowHostWin::Minimize() {
  message_handler_->Minimize();
}

void DesktopRootWindowHostWin::Restore() {
  message_handler_->Restore();
}

bool DesktopRootWindowHostWin::IsMaximized() const {
  return message_handler_->IsMaximized();
}

bool DesktopRootWindowHostWin::IsMinimized() const {
  return message_handler_->IsMinimized();
}

void DesktopRootWindowHostWin::SetAlwaysOnTop(bool always_on_top) {
  message_handler_->SetAlwaysOnTop(always_on_top);
}

InputMethod* DesktopRootWindowHostWin::CreateInputMethod() {
  return new InputMethodWin(message_handler_.get(), message_handler_->hwnd());
}

internal::InputMethodDelegate*
    DesktopRootWindowHostWin::GetInputMethodDelegate() {
  return message_handler_.get();
}

void DesktopRootWindowHostWin::SetWindowTitle(const string16& title) {
  message_handler_->SetTitle(title);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWin, RootWindowHost implementation:

aura::RootWindow* DesktopRootWindowHostWin::GetRootWindow() {
  return root_window_.get();
}

gfx::AcceleratedWidget DesktopRootWindowHostWin::GetAcceleratedWidget() {
  return message_handler_->hwnd();
}

void DesktopRootWindowHostWin::Show() {
  message_handler_->Show();
}

void DesktopRootWindowHostWin::Hide() {
  message_handler_->Hide();
}

void DesktopRootWindowHostWin::ToggleFullScreen() {
}

gfx::Rect DesktopRootWindowHostWin::GetBounds() const {
  // TODO(beng): Should be an ash-only method??
  return GetWindowBoundsInScreen();
}

void DesktopRootWindowHostWin::SetBounds(const gfx::Rect& bounds) {
  message_handler_->SetBounds(bounds);
}

gfx::Point DesktopRootWindowHostWin::GetLocationOnNativeScreen() const {
  return gfx::Point(1, 1);
}

void DesktopRootWindowHostWin::SetCapture() {
}

void DesktopRootWindowHostWin::ReleaseCapture() {
}

void DesktopRootWindowHostWin::SetCursor(gfx::NativeCursor cursor) {
}

void DesktopRootWindowHostWin::ShowCursor(bool show) {
}

bool DesktopRootWindowHostWin::QueryMouseLocation(gfx::Point* location_return) {
  return false;
}

bool DesktopRootWindowHostWin::ConfineCursorToRootWindow() {
  return false;
}

void DesktopRootWindowHostWin::UnConfineCursor() {
}

void DesktopRootWindowHostWin::MoveCursorTo(const gfx::Point& location) {
}

void DesktopRootWindowHostWin::SetFocusWhenShown(bool focus_when_shown) {
}

bool DesktopRootWindowHostWin::GrabSnapshot(
      const gfx::Rect& snapshot_bounds,
      std::vector<unsigned char>* png_representation) {
  return false;
}

void DesktopRootWindowHostWin::PostNativeEvent(
    const base::NativeEvent& native_event) {
}

void DesktopRootWindowHostWin::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
}

void DesktopRootWindowHostWin::PrepareForShutdown() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWin, HWNDMessageHandlerDelegate implementation:

bool DesktopRootWindowHostWin::IsWidgetWindow() const {
  return true;
}

bool DesktopRootWindowHostWin::IsUsingCustomFrame() const {
  return true;
}

void DesktopRootWindowHostWin::SchedulePaint() {
  native_widget_delegate_->AsWidget()->GetRootView()->SchedulePaint();
}

void DesktopRootWindowHostWin::EnableInactiveRendering() {
}

bool DesktopRootWindowHostWin::IsInactiveRenderingDisabled() {
  return false;
}

bool DesktopRootWindowHostWin::CanResize() const {
  return true;
}

bool DesktopRootWindowHostWin::CanMaximize() const {
  return true;
}

bool DesktopRootWindowHostWin::CanActivate() const {
  return true;
}

bool DesktopRootWindowHostWin::WidgetSizeIsClientSize() const {
  return false;
}

bool DesktopRootWindowHostWin::CanSaveFocus() const {
  return true;
}

void DesktopRootWindowHostWin::SaveFocusOnDeactivate() {
}

void DesktopRootWindowHostWin::RestoreFocusOnActivate() {
}

void DesktopRootWindowHostWin::RestoreFocusOnEnable() {
}

bool DesktopRootWindowHostWin::IsModal() const {
  return false;
}

int DesktopRootWindowHostWin::GetInitialShowState() const {
  return ui::SHOW_STATE_NORMAL;
}

bool DesktopRootWindowHostWin::WillProcessWorkAreaChange() const {
  return false;
}

int DesktopRootWindowHostWin::GetNonClientComponent(
    const gfx::Point& point) const {
  return native_widget_delegate_->GetNonClientComponent(point);
}

void DesktopRootWindowHostWin::GetWindowMask(const gfx::Size& size,
                                             gfx::Path* path) {
}

bool DesktopRootWindowHostWin::GetClientAreaInsets(gfx::Insets* insets) const {
  return false;
}

void DesktopRootWindowHostWin::GetMinMaxSize(gfx::Size* min_size,
                                             gfx::Size* max_size) const {
}

gfx::Size DesktopRootWindowHostWin::GetRootViewSize() const {
  return gfx::Size(100, 100);
}

void DesktopRootWindowHostWin::ResetWindowControls() {
}

void DesktopRootWindowHostWin::PaintLayeredWindow(gfx::Canvas* canvas) {
}

gfx::NativeViewAccessible DesktopRootWindowHostWin::GetNativeViewAccessible() {
  return NULL;
}

InputMethod* DesktopRootWindowHostWin::GetInputMethod() {
  return native_widget_delegate_->AsWidget()->GetInputMethodDirect();
}

void DesktopRootWindowHostWin::HandleAppDeactivated() {
}

void DesktopRootWindowHostWin::HandleActivationChanged(bool active) {
}

bool DesktopRootWindowHostWin::HandleAppCommand(short command) {
  return false;
}

void DesktopRootWindowHostWin::HandleCaptureLost() {
}

void DesktopRootWindowHostWin::HandleClose() {
}

bool DesktopRootWindowHostWin::HandleCommand(int command) {
  return false;
}

void DesktopRootWindowHostWin::HandleAccelerator(
    const ui::Accelerator& accelerator) {
}

void DesktopRootWindowHostWin::HandleCreate() {
  // TODO(beng): moar
  NOTIMPLEMENTED();

  // 1. Window property association
  // 2. MouseWheel.
  // 3. Drop target.
  // 4. Tooltip Manager.
}

void DesktopRootWindowHostWin::HandleDestroying() {
  native_widget_delegate_->OnNativeWidgetDestroying();
}

void DesktopRootWindowHostWin::HandleDestroyed() {
}

bool DesktopRootWindowHostWin::HandleInitialFocus() {
  return false;
}

void DesktopRootWindowHostWin::HandleDisplayChange() {
}

void DesktopRootWindowHostWin::HandleBeginWMSizeMove() {
}

void DesktopRootWindowHostWin::HandleEndWMSizeMove() {
}

void DesktopRootWindowHostWin::HandleMove() {
}

void DesktopRootWindowHostWin::HandleWorkAreaChanged() {
}

void DesktopRootWindowHostWin::HandleVisibilityChanged(bool visible) {
}

void DesktopRootWindowHostWin::HandleClientSizeChanged(
    const gfx::Size& new_size) {
  if (root_window_host_delegate_)
    root_window_host_delegate_->OnHostResized(new_size);
  // TODO(beng): replace with a layout manager??
  content_window_->SetBounds(gfx::Rect(new_size));
}

void DesktopRootWindowHostWin::HandleFrameChanged() {
}

void DesktopRootWindowHostWin::HandleNativeFocus(HWND last_focused_window) {
  InputMethod* input_method = GetInputMethod();
  if (input_method)
    input_method->OnFocus();
}

void DesktopRootWindowHostWin::HandleNativeBlur(HWND focused_window) {
  InputMethod* input_method = GetInputMethod();
  if (input_method)
    input_method->OnBlur();
}

bool DesktopRootWindowHostWin::HandleMouseEvent(const ui::MouseEvent& event) {
  return root_window_host_delegate_->OnHostMouseEvent(
      const_cast<ui::MouseEvent*>(&event));
}

bool DesktopRootWindowHostWin::HandleKeyEvent(const ui::KeyEvent& event) {
  return root_window_host_delegate_->OnHostKeyEvent(
      const_cast<ui::KeyEvent*>(&event));
}

bool DesktopRootWindowHostWin::HandleUntranslatedKeyEvent(
    const ui::KeyEvent& event) {
  InputMethod* input_method = GetInputMethod();
  if (input_method)
    input_method->DispatchKeyEvent(event);
  return !!input_method;
}

bool DesktopRootWindowHostWin::HandleIMEMessage(UINT message,
                                           WPARAM w_param,
                                           LPARAM l_param,
                                           LRESULT* result) {
  InputMethod* input_method = GetInputMethod();
  if (!input_method || input_method->IsMock()) {
    *result = 0;
    return false;
  }

  InputMethodWin* ime_win = static_cast<InputMethodWin*>(input_method);
  BOOL handled = FALSE;
  *result = ime_win->OnImeMessages(message, w_param, l_param, &handled);
  return !!handled;
}

void DesktopRootWindowHostWin::HandleInputLanguageChange(
    DWORD character_set,
    HKL input_language_id) {
  InputMethod* input_method = GetInputMethod();
  if (input_method && !input_method->IsMock()) {
    static_cast<InputMethodWin*>(input_method)->OnInputLangChange(
        character_set, input_language_id);
  }
}

bool DesktopRootWindowHostWin::HandlePaintAccelerated(
    const gfx::Rect& invalid_rect) {
  return native_widget_delegate_->OnNativeWidgetPaintAccelerated(invalid_rect);
}

void DesktopRootWindowHostWin::HandlePaint(gfx::Canvas* canvas) {
  root_window_host_delegate_->OnHostPaint();
}

void DesktopRootWindowHostWin::HandleScreenReaderDetected() {
}

bool DesktopRootWindowHostWin::HandleTooltipNotify(int w_param,
                                                   NMHDR* l_param,
                                                   LRESULT* l_result) {
  return false;
}

void DesktopRootWindowHostWin::HandleTooltipMouseMove(UINT message,
                                                      WPARAM w_param,
                                                      LPARAM l_param) {
}

bool DesktopRootWindowHostWin::PreHandleMSG(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param,
                                            LRESULT* result) {
  return false;
}

void DesktopRootWindowHostWin::PostHandleMSG(UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param) {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHost, public:

// static
DesktopRootWindowHost* DesktopRootWindowHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    const gfx::Rect& initial_bounds) {
  return new DesktopRootWindowHostWin(native_widget_delegate, initial_bounds);
}

}  // namespace views
