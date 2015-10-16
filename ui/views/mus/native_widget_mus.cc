// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/native_widget_mus.h"

#include "components/mus/public/cpp/window.h"
#include "ui/views/mus/window_tree_host_mus.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMus, public:

NativeWidgetMus::NativeWidgetMus(internal::NativeWidgetDelegate* delegate)
    : window_(nullptr),
      native_widget_delegate_(delegate) {}
NativeWidgetMus::~NativeWidgetMus() {}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMus, internal::NativeWidgetPrivate implementation:

void NativeWidgetMus::InitNativeWidget(const Widget::InitParams& params) {
  NOTIMPLEMENTED();
}

NonClientFrameView* NativeWidgetMus::CreateNonClientFrameView() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool NativeWidgetMus::ShouldUseNativeFrame() const {
  NOTIMPLEMENTED();
  return false;
}

bool NativeWidgetMus::ShouldWindowContentsBeTransparent() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMus::FrameTypeChanged() {
  NOTIMPLEMENTED();
}

Widget* NativeWidgetMus::GetWidget() {
  NOTIMPLEMENTED();
  return native_widget_delegate_->AsWidget();
}

const Widget* NativeWidgetMus::GetWidget() const {
  NOTIMPLEMENTED();
  return native_widget_delegate_->AsWidget();
}

gfx::NativeView NativeWidgetMus::GetNativeView() const {
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::NativeWindow NativeWidgetMus::GetNativeWindow() const {
  NOTIMPLEMENTED();
  return nullptr;
}

Widget* NativeWidgetMus::GetTopLevelWidget() {
  return GetWidget();
}

const ui::Compositor* NativeWidgetMus::GetCompositor() const {
  NOTIMPLEMENTED();
  return nullptr;
}

const ui::Layer* NativeWidgetMus::GetLayer() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void NativeWidgetMus::ReorderNativeViews() {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::ViewRemoved(View* view) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::SetNativeWindowProperty(const char* name,
                                                      void* value) {
  NOTIMPLEMENTED();
}

void* NativeWidgetMus::GetNativeWindowProperty(const char* name) const {
  NOTIMPLEMENTED();
  return nullptr;
}

TooltipManager* NativeWidgetMus::GetTooltipManager() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void NativeWidgetMus::SetCapture() {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::ReleaseCapture() {
  NOTIMPLEMENTED();
}

bool NativeWidgetMus::HasCapture() const {
  NOTIMPLEMENTED();
  return false;
}

ui::InputMethod* NativeWidgetMus::GetInputMethod() {
  NOTIMPLEMENTED();
  return nullptr;
}

void NativeWidgetMus::CenterWindow(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::GetWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* maximized) const {
  NOTIMPLEMENTED();
}

bool NativeWidgetMus::SetWindowTitle(const base::string16& title) {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMus::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                     const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::InitModalType(ui::ModalType modal_type) {
  NOTIMPLEMENTED();
}

gfx::Rect NativeWidgetMus::GetWindowBoundsInScreen() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

gfx::Rect NativeWidgetMus::GetClientAreaBoundsInScreen() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

gfx::Rect NativeWidgetMus::GetRestoredBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void NativeWidgetMus::SetBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::SetSize(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::StackAbove(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::StackAtTop() {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::StackBelow(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::SetShape(SkRegion* shape) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::Close() {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::CloseNow() {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::Show() {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::Hide() {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::ShowWithWindowState(ui::WindowShowState state) {
  NOTIMPLEMENTED();
}

bool NativeWidgetMus::IsVisible() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMus::Activate() {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::Deactivate() {
  NOTIMPLEMENTED();
}

bool NativeWidgetMus::IsActive() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMus::SetAlwaysOnTop(bool always_on_top) {
  NOTIMPLEMENTED();
}

bool NativeWidgetMus::IsAlwaysOnTop() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMus::SetVisibleOnAllWorkspaces(bool always_visible) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::Maximize() {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::Minimize() {
  NOTIMPLEMENTED();
}

bool NativeWidgetMus::IsMaximized() const {
  NOTIMPLEMENTED();
  return false;
}

bool NativeWidgetMus::IsMinimized() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMus::Restore() {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::SetFullscreen(bool fullscreen) {
  NOTIMPLEMENTED();
}

bool NativeWidgetMus::IsFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMus::SetOpacity(unsigned char opacity) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::SetUseDragFrame(bool use_drag_frame) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::FlashFrame(bool flash_frame) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::RunShellDrag(
    View* view,
    const ui::OSExchangeData& data,
    const gfx::Point& location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::SchedulePaintInRect(const gfx::Rect& rect) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::SetCursor(gfx::NativeCursor cursor) {
  NOTIMPLEMENTED();
}

bool NativeWidgetMus::IsMouseEventsEnabled() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMus::ClearNativeFocus() {
  NOTIMPLEMENTED();
}

gfx::Rect NativeWidgetMus::GetWorkAreaBoundsInScreen() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

Widget::MoveLoopResult NativeWidgetMus::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  NOTIMPLEMENTED();
  return Widget::MOVE_LOOP_CANCELED;
}

void NativeWidgetMus::EndMoveLoop() {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::SetVisibilityChangedAnimationsEnabled(bool value) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::SetVisibilityAnimationDuration(
    const base::TimeDelta& duration) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::SetVisibilityAnimationTransition(
    Widget::VisibilityTransition transition) {
  NOTIMPLEMENTED();
}

ui::NativeTheme* NativeWidgetMus::GetNativeTheme() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void NativeWidgetMus::OnRootViewLayout() {
  NOTIMPLEMENTED();
}

bool NativeWidgetMus::IsTranslucentWindowOpacitySupported() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMus::OnSizeConstraintsChanged() {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::RepostNativeEvent(gfx::NativeEvent native_event) {
  NOTIMPLEMENTED();
}

}  // namespace views
