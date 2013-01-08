// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_root_window_host_win.h"

#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/ui_controls_aura.h"
#include "ui/aura/window_property.h"
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/base/ime/input_method_win.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/path_win.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/native_theme/native_theme_win.h"
#include "ui/ui_controls/ui_controls.h"
#include "ui/views/corewm/compound_event_filter.h"
#include "ui/views/corewm/input_method_event_filter.h"
#include "ui/views/ime/input_method_bridge.h"
#include "ui/views/widget/desktop_aura/desktop_activation_client.h"
#include "ui/views/widget/desktop_aura/desktop_cursor_client.h"
#include "ui/views/widget/desktop_aura/desktop_dispatcher_client.h"
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_win.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_screen_position_client.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_hwnd_utils.h"
#include "ui/views/win/fullscreen_handler.h"
#include "ui/views/win/hwnd_message_handler.h"
#include "ui/views/window/native_frame_view.h"

namespace views {

DEFINE_WINDOW_PROPERTY_KEY(aura::Window*, kContentWindowForRootWindow, NULL);

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWin, public:

DesktopRootWindowHostWin::DesktopRootWindowHostWin(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura,
    const gfx::Rect& initial_bounds)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          message_handler_(new HWNDMessageHandler(this))),
      native_widget_delegate_(native_widget_delegate),
      desktop_native_widget_aura_(desktop_native_widget_aura),
      root_window_host_delegate_(NULL),
      content_window_(NULL) {
}

DesktopRootWindowHostWin::~DesktopRootWindowHostWin() {
}

// static
aura::Window* DesktopRootWindowHostWin::GetContentWindowForHWND(HWND hwnd) {
  aura::RootWindow* root = aura::RootWindow::GetForAcceleratedWidget(hwnd);
  return root ? root->GetProperty(kContentWindowForRootWindow) : NULL;
}

// static
ui::NativeTheme* DesktopRootWindowHost::GetNativeTheme(aura::Window* window) {
  // Use NativeThemeWin for windows shown on the desktop, those not on the
  // desktop come from Ash and get NativeThemeAura.
  aura::RootWindow* root = window->GetRootWindow();
  if (root) {
    HWND root_hwnd = root->GetAcceleratedWidget();
    if (root_hwnd &&
        DesktopRootWindowHostWin::GetContentWindowForHWND(root_hwnd)) {
      return ui::NativeThemeWin::instance();
    }
  }
  return ui::NativeThemeAura::instance();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWin, DesktopRootWindowHost implementation:

aura::RootWindow* DesktopRootWindowHostWin::Init(
    aura::Window* content_window,
    const Widget::InitParams& params) {
  // TODO(beng): SetInitParams().
  content_window_ = content_window;

  ConfigureWindowStyles(message_handler_.get(), params,
                        GetWidget()->widget_delegate(),
                        native_widget_delegate_);

  HWND parent_hwnd = NULL;
  aura::Window* parent_window = params.parent;
  if (parent_window)
    parent_hwnd = parent_window->GetRootWindow()->GetAcceleratedWidget();

  message_handler_->Init(parent_hwnd, params.bounds);

  message_handler_->set_remove_standard_frame(params.remove_standard_frame);

  aura::RootWindow::CreateParams rw_params(params.bounds);
  rw_params.host = this;
  root_window_ = new aura::RootWindow(rw_params);

  // TODO(beng): We probably need to move these two calls to some function that
  //             can change depending on the native-ness of the frame. For right
  //             now in the hack-n-slash days of win-aura, we can just
  //             unilaterally turn this on.
  root_window_->compositor()->SetHostHasTransparentBackground(true);
  root_window_->SetTransparent(true);

  root_window_->Init();
  root_window_->AddChild(content_window_);

  native_widget_delegate_->OnNativeWidgetCreated();

  capture_client_.reset(new aura::client::DefaultCaptureClient(root_window_));
  aura::client::SetCaptureClient(root_window_, capture_client_.get());

  focus_client_.reset(new aura::FocusManager);
  aura::client::SetFocusClient(root_window_, focus_client_.get());

  activation_client_.reset(new DesktopActivationClient(root_window_));

  dispatcher_client_.reset(new DesktopDispatcherClient);
  aura::client::SetDispatcherClient(root_window_,
                                    dispatcher_client_.get());

  cursor_client_.reset(new DesktopCursorClient(root_window_));
  aura::client::SetCursorClient(root_window_, cursor_client_.get());

  position_client_.reset(new DesktopScreenPositionClient());
  aura::client::SetScreenPositionClient(root_window_,
                                        position_client_.get());

  desktop_native_widget_aura_->InstallInputMethodEventFilter(root_window_);

  drag_drop_client_.reset(new DesktopDragDropClientWin(root_window_,
                                                       GetHWND()));
  aura::client::SetDragDropClient(root_window_, drag_drop_client_.get());

  focus_client_->FocusWindow(content_window_, NULL);
  root_window_->SetProperty(kContentWindowForRootWindow, content_window_);

  ui_controls::InstallUIControlsAura(CreateUIControlsAura(root_window_));
  return root_window_;
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

void DesktopRootWindowHostWin::SetWindowTitle(const string16& title) {
  message_handler_->SetTitle(title);
}

void DesktopRootWindowHostWin::ClearNativeFocus() {
  message_handler_->ClearNativeFocus();
}

Widget::MoveLoopResult DesktopRootWindowHostWin::RunMoveLoop(
    const gfx::Vector2d& drag_offset) {
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
  return ui::win::IsAeroGlassEnabled();
}

void DesktopRootWindowHostWin::FrameTypeChanged() {
  message_handler_->FrameTypeChanged();
}

NonClientFrameView* DesktopRootWindowHostWin::CreateNonClientFrameView() {
  return GetWidget()->ShouldUseNativeFrame() ?
      new NativeFrameView(GetWidget()) : NULL;
}

void DesktopRootWindowHostWin::SetFullscreen(bool fullscreen) {
  message_handler_->fullscreen_handler()->SetFullscreen(fullscreen);
}

bool DesktopRootWindowHostWin::IsFullscreen() const {
  return message_handler_->fullscreen_handler()->fullscreen();
}

void DesktopRootWindowHostWin::SetOpacity(unsigned char opacity) {
  message_handler_->SetOpacity(static_cast<BYTE>(opacity));
  GetWidget()->GetRootView()->SchedulePaint();
}

void DesktopRootWindowHostWin::SetWindowIcons(
    const gfx::ImageSkia& window_icon, const gfx::ImageSkia& app_icon) {
  message_handler_->SetWindowIcons(window_icon, app_icon);
}

void DesktopRootWindowHostWin::SetAccessibleName(const string16& name) {
  message_handler_->SetAccessibleName(name);
}

void DesktopRootWindowHostWin::SetAccessibleRole(
    ui::AccessibilityTypes::Role role) {
  message_handler_->SetAccessibleRole(role);
}

void DesktopRootWindowHostWin::SetAccessibleState(
    ui::AccessibilityTypes::State state) {
  message_handler_->SetAccessibleState(state);
}

void DesktopRootWindowHostWin::InitModalType(ui::ModalType modal_type) {
  message_handler_->InitModalType(modal_type);
}

void DesktopRootWindowHostWin::FlashFrame(bool flash_frame) {
  message_handler_->FlashFrame(flash_frame);
}

void DesktopRootWindowHostWin::OnNativeWidgetFocus() {
  // HWNDMessageHandler will perform the proper updating on its own.
}

void DesktopRootWindowHostWin::OnNativeWidgetBlur() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWin, RootWindowHost implementation:


void DesktopRootWindowHostWin::SetDelegate(
    aura::RootWindowHostDelegate* delegate) {
  root_window_host_delegate_ = delegate;
}

aura::RootWindow* DesktopRootWindowHostWin::GetRootWindow() {
  return root_window_;
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
  // Match the logic in HWNDMessageHandler::ClientAreaSizeChanged().
  return WidgetSizeIsClientSize() ?
      GetClientAreaBoundsInScreen() : GetWindowBoundsInScreen();
}

void DesktopRootWindowHostWin::SetBounds(const gfx::Rect& bounds) {
  message_handler_->SetBounds(bounds);
}

gfx::Point DesktopRootWindowHostWin::GetLocationOnNativeScreen() const {
  return GetBounds().origin();
}

void DesktopRootWindowHostWin::SetCapture() {
  message_handler_->SetCapture();
}

void DesktopRootWindowHostWin::ReleaseCapture() {
  message_handler_->ReleaseCapture();
}

void DesktopRootWindowHostWin::SetCursor(gfx::NativeCursor cursor) {
  ui::CursorLoaderWin cursor_loader;
  cursor_loader.SetPlatformCursor(&cursor);

  message_handler_->SetCursor(cursor.platform());
}

bool DesktopRootWindowHostWin::QueryMouseLocation(gfx::Point* location_return) {
  return false;
}

bool DesktopRootWindowHostWin::ConfineCursorToRootWindow() {
  return false;
}

void DesktopRootWindowHostWin::UnConfineCursor() {
}

void DesktopRootWindowHostWin::OnCursorVisibilityChanged(bool show) {
}

void DesktopRootWindowHostWin::MoveCursorTo(const gfx::Point& location) {
}

void DesktopRootWindowHostWin::SetFocusWhenShown(bool focus_when_shown) {
}

bool DesktopRootWindowHostWin::CopyAreaToSkCanvas(
    const gfx::Rect& source_bounds,
    const gfx::Point& dest_offset,
    SkCanvas* canvas) {
  NOTIMPLEMENTED();
  return false;
}

bool DesktopRootWindowHostWin::GrabSnapshot(
      const gfx::Rect& snapshot_bounds,
      std::vector<unsigned char>* png_representation) {
  NOTIMPLEMENTED();
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
  GetWidget()->GetFocusManager()->StoreFocusedView(true);
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
  if (active)
    root_window_host_delegate_->OnHostActivated();
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

  native_widget_delegate_->OnNativeWidgetCreated();

  // 1. Window property association
  // 2. MouseWheel.
  // 3. Tooltip Manager.
}

void DesktopRootWindowHostWin::HandleDestroying() {
  drag_drop_client_->OnNativeWidgetDestroying(GetHWND());
  native_widget_delegate_->OnNativeWidgetDestroying();
}

void DesktopRootWindowHostWin::HandleDestroyed() {
  desktop_native_widget_aura_->OnHostClosed();
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
  if (root_window_host_delegate_)
    root_window_host_delegate_->OnHostMoved(GetBounds().origin());
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
  if (event.flags() & ui::EF_IS_NON_CLIENT)
    return false;
  return root_window_host_delegate_->OnHostMouseEvent(
      const_cast<ui::MouseEvent*>(&event));
}

bool DesktopRootWindowHostWin::HandleKeyEvent(const ui::KeyEvent& event) {
  return false;
}

bool DesktopRootWindowHostWin::HandleUntranslatedKeyEvent(
    const ui::KeyEvent& event) {
  scoped_ptr<ui::KeyEvent> duplicate_event(event.Copy());
  return static_cast<aura::RootWindowHostDelegate*>(root_window_)->
      OnHostKeyEvent(duplicate_event.get());
}

bool DesktopRootWindowHostWin::HandleTouchEvent(
    const ui::TouchEvent& event) {
  return root_window_host_delegate_->OnHostTouchEvent(
      const_cast<ui::TouchEvent*>(&event));
}

bool DesktopRootWindowHostWin::HandleIMEMessage(UINT message,
                                                WPARAM w_param,
                                                LPARAM l_param,
                                                LRESULT* result) {
  // TODO(ime): Having to cast here is wrong. Maybe we should have IME events
  // and have these flow through the same path as HandleUntranslatedKeyEvent()
  // does.
  ui::InputMethodWin* ime_win =
      static_cast<ui::InputMethodWin*>(
          desktop_native_widget_aura_->input_method_event_filter()->
          input_method());
  BOOL handled = FALSE;
  *result = ime_win->OnImeMessages(message, w_param, l_param, &handled);
  return !!handled;
}

void DesktopRootWindowHostWin::HandleInputLanguageChange(
    DWORD character_set,
    HKL input_language_id) {
  // TODO(ime): Seem comment in HandleIMEMessage().
  ui::InputMethodWin* ime_win =
      static_cast<ui::InputMethodWin*>(
          desktop_native_widget_aura_->input_method_event_filter()->
          input_method());
  ime_win->OnInputLangChange(character_set, input_language_id);
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

HWND DesktopRootWindowHostWin::GetHWND() const {
  return message_handler_->hwnd();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHost, public:

// static
DesktopRootWindowHost* DesktopRootWindowHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura,
    const gfx::Rect& initial_bounds) {
  return new DesktopRootWindowHostWin(native_widget_delegate,
                                      desktop_native_widget_aura,
                                      initial_bounds);
}

}  // namespace views
