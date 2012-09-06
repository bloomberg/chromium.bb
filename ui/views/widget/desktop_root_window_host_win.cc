// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_root_window_host_win.h"

#include "ui/aura/root_window.h"
#include "ui/views/win/hwnd_message_handler.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWin, public:

DesktopRootWindowHostWin::DesktopRootWindowHostWin()
    : root_window_(NULL),
      message_handler_(NULL) {
}

DesktopRootWindowHostWin::~DesktopRootWindowHostWin() {
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
}

void DesktopRootWindowHostWin::Hide() {
}

void DesktopRootWindowHostWin::ToggleFullScreen() {
}

gfx::Rect DesktopRootWindowHostWin::GetBounds() const {
  return gfx::Rect(100, 100);
}

void DesktopRootWindowHostWin::SetBounds(const gfx::Rect& bounds) {
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

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWin, HWNDMessageHandlerDelegate implementation:

bool DesktopRootWindowHostWin::IsWidgetWindow() const {
  return true;
}

bool DesktopRootWindowHostWin::IsUsingCustomFrame() const {
  return true;
}

void DesktopRootWindowHostWin::SchedulePaint() {
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
  return HTCLIENT;
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
  return NULL;
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
}

void DesktopRootWindowHostWin::HandleDestroying() {
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
}

void DesktopRootWindowHostWin::HandleFrameChanged() {
}

void DesktopRootWindowHostWin::HandleNativeFocus(HWND last_focused_window) {
}

void DesktopRootWindowHostWin::HandleNativeBlur(HWND focused_window) {
}

bool DesktopRootWindowHostWin::HandleMouseEvent(const ui::MouseEvent& event) {
  return false;
}

bool DesktopRootWindowHostWin::HandleKeyEvent(const ui::KeyEvent& event) {
  return false;
}

bool DesktopRootWindowHostWin::HandleUntranslatedKeyEvent(
    const ui::KeyEvent& event) {
  return false;
}

bool DesktopRootWindowHostWin::HandleIMEMessage(UINT message,
                                           WPARAM w_param,
                                           LPARAM l_param,
                                           LRESULT* result) {
  return false;
}

void DesktopRootWindowHostWin::HandleInputLanguageChange(
    DWORD character_set,
    HKL input_language_id) {
}

bool DesktopRootWindowHostWin::HandlePaintAccelerated(
    const gfx::Rect& invalid_rect) {
  return false;
}

void DesktopRootWindowHostWin::HandlePaint(gfx::Canvas* canvas) {
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
DesktopRootWindowHost* DesktopRootWindowHost::Create() {
  return new DesktopRootWindowHostWin;
}

}  // namespace views
