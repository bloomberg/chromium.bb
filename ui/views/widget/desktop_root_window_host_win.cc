// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_root_window_host_win.h"

#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/desktop/desktop_activation_client.h"
#include "ui/aura/desktop/desktop_cursor_client.h"
#include "ui/aura/desktop/desktop_dispatcher_client.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/compound_event_filter.h"
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/path_win.h"
#include "ui/views/ime/input_method_win.h"
#include "ui/views/widget/desktop_capture_client.h"
#include "ui/views/widget/desktop_screen_position_client.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/win/hwnd_message_handler.h"
#include "ui/views/window/native_frame_view.h"

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

  message_handler_->set_remove_standard_frame(!ShouldUseNativeFrame());

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

  cursor_client_.reset(new aura::DesktopCursorClient(root_window_.get()));
  aura::client::SetCursorClient(root_window_.get(), cursor_client_.get());


  position_client_.reset(new DesktopScreenPositionClient());
  aura::client::SetScreenPositionClient(root_window_.get(),
                                        position_client_.get());

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

gfx::Rect DesktopRootWindowHostWin::GetWorkAreaBoundsInScreen() const {
  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(MonitorFromWindow(message_handler_->hwnd(),
                                   MONITOR_DEFAULTTONEAREST),
                 &monitor_info);
  return gfx::Rect(monitor_info.rcWork);
}

void DesktopRootWindowHostWin::SetShape(gfx::NativeRegion native_region) {
  SkPath path;
  native_region->getBoundaryPath(&path);
  message_handler_->SetRegion(gfx::CreateHRGNFromSkPath(path));
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

bool DesktopRootWindowHostWin::HasCapture() const {
  return message_handler_->HasCapture();
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

Widget::MoveLoopResult DesktopRootWindowHostWin::RunMoveLoop(
    const gfx::Point& drag_offset) {
  return message_handler_->RunMoveLoop(drag_offset) ?
      Widget::MOVE_LOOP_SUCCESSFUL : Widget::MOVE_LOOP_CANCELED;
}

void DesktopRootWindowHostWin::EndMoveLoop() {
  message_handler_->EndMoveLoop();
}

void DesktopRootWindowHostWin::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  message_handler_->SetVisibilityChangedAnimationsEnabled(value);
}

bool DesktopRootWindowHostWin::ShouldUseNativeFrame() {
  return false;
  // TODO(beng): allow BFW customizations.
  // return ui::win::IsAeroGlassEnabled();
}

void DesktopRootWindowHostWin::FrameTypeChanged() {
  message_handler_->FrameTypeChanged();
}

NonClientFrameView* DesktopRootWindowHostWin::CreateNonClientFrameView() {
  return GetWidget()->ShouldUseNativeFrame() ?
      new NativeFrameView(GetWidget()) : NULL;
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
  message_handler_->SetCapture();
}

void DesktopRootWindowHostWin::ReleaseCapture() {
  message_handler_->ReleaseCapture();
}

void DesktopRootWindowHostWin::SetCursor(gfx::NativeCursor cursor) {
  // Custom web cursors are handled directly.
  if (cursor == ui::kCursorCustom)
    return;
  ui::CursorLoaderWin cursor_loader;
  cursor_loader.SetPlatformCursor(&cursor);

  message_handler_->SetCursor(cursor.platform());
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
  return !GetWidget()->ShouldUseNativeFrame();
}

void DesktopRootWindowHostWin::SchedulePaint() {
  GetWidget()->GetRootView()->SchedulePaint();
}

void DesktopRootWindowHostWin::EnableInactiveRendering() {
  native_widget_delegate_->EnableInactiveRendering();
}

bool DesktopRootWindowHostWin::IsInactiveRenderingDisabled() {
  return native_widget_delegate_->IsInactiveRenderingDisabled();
}

bool DesktopRootWindowHostWin::CanResize() const {
  return GetWidget()->widget_delegate()->CanResize();
}

bool DesktopRootWindowHostWin::CanMaximize() const {
  return GetWidget()->widget_delegate()->CanMaximize();
}

bool DesktopRootWindowHostWin::CanActivate() const {
  return native_widget_delegate_->CanActivate();
}

bool DesktopRootWindowHostWin::WidgetSizeIsClientSize() const {
  const Widget* widget = GetWidget()->GetTopLevelWidget();
  return IsMaximized() || (widget && widget->ShouldUseNativeFrame());
}

bool DesktopRootWindowHostWin::CanSaveFocus() const {
  return GetWidget()->is_top_level();
}

void DesktopRootWindowHostWin::SaveFocusOnDeactivate() {
  GetWidget()->GetFocusManager()->StoreFocusedView();
}

void DesktopRootWindowHostWin::RestoreFocusOnActivate() {
  RestoreFocusOnEnable();
}

void DesktopRootWindowHostWin::RestoreFocusOnEnable() {
  GetWidget()->GetFocusManager()->RestoreFocusedView();
}

bool DesktopRootWindowHostWin::IsModal() const {
  return native_widget_delegate_->IsModal();
}

int DesktopRootWindowHostWin::GetInitialShowState() const {
  return SW_SHOWNORMAL;
}

bool DesktopRootWindowHostWin::WillProcessWorkAreaChange() const {
  return GetWidget()->widget_delegate()->WillProcessWorkAreaChange();
}

int DesktopRootWindowHostWin::GetNonClientComponent(
    const gfx::Point& point) const {
  return native_widget_delegate_->GetNonClientComponent(point);
}

void DesktopRootWindowHostWin::GetWindowMask(const gfx::Size& size,
                                             gfx::Path* path) {
  if (GetWidget()->non_client_view())
    GetWidget()->non_client_view()->GetWindowMask(size, path);
}

bool DesktopRootWindowHostWin::GetClientAreaInsets(gfx::Insets* insets) const {
  return false;
}

void DesktopRootWindowHostWin::GetMinMaxSize(gfx::Size* min_size,
                                             gfx::Size* max_size) const {
  *min_size = native_widget_delegate_->GetMinimumSize();
  *max_size = native_widget_delegate_->GetMaximumSize();
}

gfx::Size DesktopRootWindowHostWin::GetRootViewSize() const {
  return GetWidget()->GetRootView()->size();
}

void DesktopRootWindowHostWin::ResetWindowControls() {
  GetWidget()->non_client_view()->ResetWindowControls();
}

void DesktopRootWindowHostWin::PaintLayeredWindow(gfx::Canvas* canvas) {
  GetWidget()->GetRootView()->Paint(canvas);
}

gfx::NativeViewAccessible DesktopRootWindowHostWin::GetNativeViewAccessible() {
  return GetWidget()->GetRootView()->GetNativeViewAccessible();
}

InputMethod* DesktopRootWindowHostWin::GetInputMethod() {
  return GetWidget()->GetInputMethodDirect();
}

void DesktopRootWindowHostWin::HandleAppDeactivated() {
  native_widget_delegate_->EnableInactiveRendering();
}

void DesktopRootWindowHostWin::HandleActivationChanged(bool active) {
  native_widget_delegate_->OnNativeWidgetActivationChanged(active);
}

bool DesktopRootWindowHostWin::HandleAppCommand(short command) {
  // We treat APPCOMMAND ids as an extension of our command namespace, and just
  // let the delegate figure out what to do...
  return GetWidget()->widget_delegate() &&
      GetWidget()->widget_delegate()->ExecuteWindowsCommand(command);
}

void DesktopRootWindowHostWin::HandleCaptureLost() {
  native_widget_delegate_->OnMouseCaptureLost();
}

void DesktopRootWindowHostWin::HandleClose() {
  GetWidget()->Close();
}

bool DesktopRootWindowHostWin::HandleCommand(int command) {
  return GetWidget()->widget_delegate()->ExecuteWindowsCommand(command);
}

void DesktopRootWindowHostWin::HandleAccelerator(
    const ui::Accelerator& accelerator) {
  GetWidget()->GetFocusManager()->ProcessAccelerator(accelerator);
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
  return GetWidget()->SetInitialFocus();
}

void DesktopRootWindowHostWin::HandleDisplayChange() {
  GetWidget()->widget_delegate()->OnDisplayChanged();
}

void DesktopRootWindowHostWin::HandleBeginWMSizeMove() {
  native_widget_delegate_->OnNativeWidgetBeginUserBoundsChange();
}

void DesktopRootWindowHostWin::HandleEndWMSizeMove() {
  native_widget_delegate_->OnNativeWidgetEndUserBoundsChange();
}

void DesktopRootWindowHostWin::HandleMove() {
  native_widget_delegate_->OnNativeWidgetMove();
}

void DesktopRootWindowHostWin::HandleWorkAreaChanged() {
  GetWidget()->widget_delegate()->OnWorkAreaChanged();
}

void DesktopRootWindowHostWin::HandleVisibilityChanged(bool visible) {
  native_widget_delegate_->OnNativeWidgetVisibilityChanged(visible);
}

void DesktopRootWindowHostWin::HandleClientSizeChanged(
    const gfx::Size& new_size) {
  if (root_window_host_delegate_)
    root_window_host_delegate_->OnHostResized(new_size);
  // TODO(beng): replace with a layout manager??
  content_window_->SetBounds(gfx::Rect(new_size));
}

void DesktopRootWindowHostWin::HandleFrameChanged() {
  // Replace the frame and layout the contents.
  GetWidget()->non_client_view()->UpdateFrame(true);
}

void DesktopRootWindowHostWin::HandleNativeFocus(HWND last_focused_window) {
  // TODO(beng): inform the native_widget_delegate_.
  InputMethod* input_method = GetInputMethod();
  if (input_method)
    input_method->OnFocus();
}

void DesktopRootWindowHostWin::HandleNativeBlur(HWND focused_window) {
  // TODO(beng): inform the native_widget_delegate_.
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
// DesktopRootWindowHostWin, private:

Widget* DesktopRootWindowHostWin::GetWidget() {
  return native_widget_delegate_->AsWidget();
}

const Widget* DesktopRootWindowHostWin::GetWidget() const {
  return native_widget_delegate_->AsWidget();
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
