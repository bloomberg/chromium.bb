// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/native_widget_aura.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, public:

NativeWidgetAura::NativeWidgetAura(internal::NativeWidgetDelegate* delegate) {
}

NativeWidgetAura::~NativeWidgetAura() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, internal::NativeWidgetPrivate implementation:

void NativeWidgetAura::InitNativeWidget(const Widget::InitParams& params) {
}

NonClientFrameView* NativeWidgetAura::CreateNonClientFrameView() {
  return NULL;
}

void NativeWidgetAura::UpdateFrameAfterFrameChange() {
}

bool NativeWidgetAura::ShouldUseNativeFrame() const {
  return false;
}

void NativeWidgetAura::FrameTypeChanged() {
}

Widget* NativeWidgetAura::GetWidget() {
  return NULL;
}

const Widget* NativeWidgetAura::GetWidget() const {
  return NULL;
}

gfx::NativeView NativeWidgetAura::GetNativeView() const {
  return NULL;
}

gfx::NativeWindow NativeWidgetAura::GetNativeWindow() const {
  return NULL;
}

Widget* NativeWidgetAura::GetTopLevelWidget() {
  NativeWidgetPrivate* native_widget = GetTopLevelNativeWidget(GetNativeView());
  return native_widget ? native_widget->GetWidget() : NULL;
}

const ui::Compositor* NativeWidgetAura::GetCompositor() const {
  return NULL;
}

ui::Compositor* NativeWidgetAura::GetCompositor() {
  return NULL;
}

void NativeWidgetAura::MarkLayerDirty() {
}

void NativeWidgetAura::CalculateOffsetToAncestorWithLayer(gfx::Point* offset,
                                                         View** ancestor) {
}

void NativeWidgetAura::ViewRemoved(View* view) {
}

void NativeWidgetAura::SetNativeWindowProperty(const char* name, void* value) {
}

void* NativeWidgetAura::GetNativeWindowProperty(const char* name) const {
  return NULL;
}

TooltipManager* NativeWidgetAura::GetTooltipManager() const {
  return NULL;
}

bool NativeWidgetAura::IsScreenReaderActive() const {
  return false;
}

void NativeWidgetAura::SendNativeAccessibilityEvent(
    View* view,
    ui::AccessibilityTypes::Event event_type) {
}

void NativeWidgetAura::SetMouseCapture() {
}

void NativeWidgetAura::ReleaseMouseCapture() {
}

bool NativeWidgetAura::HasMouseCapture() const {
  return false;
}

InputMethod* NativeWidgetAura::CreateInputMethod() {
  return NULL;
}

void NativeWidgetAura::CenterWindow(const gfx::Size& size) {
}

void NativeWidgetAura::GetWindowBoundsAndMaximizedState(gfx::Rect* bounds,
                                                       bool* maximized) const {
}

void NativeWidgetAura::SetWindowTitle(const std::wstring& title) {
}

void NativeWidgetAura::SetWindowIcons(const SkBitmap& window_icon,
                                     const SkBitmap& app_icon) {
}

void NativeWidgetAura::SetAccessibleName(const std::wstring& name) {
}

void NativeWidgetAura::SetAccessibleRole(ui::AccessibilityTypes::Role role) {
}

void NativeWidgetAura::SetAccessibleState(ui::AccessibilityTypes::State state) {
}

void NativeWidgetAura::BecomeModal() {
}

gfx::Rect NativeWidgetAura::GetWindowScreenBounds() const {
  return gfx::Rect();
}

gfx::Rect NativeWidgetAura::GetClientAreaScreenBounds() const {
  return gfx::Rect();
}

gfx::Rect NativeWidgetAura::GetRestoredBounds() const {
  return gfx::Rect();
}

void NativeWidgetAura::SetBounds(const gfx::Rect& bounds) {
}

void NativeWidgetAura::SetSize(const gfx::Size& size) {
}

void NativeWidgetAura::SetBoundsConstrained(const gfx::Rect& bounds,
                                           Widget* other_widget) {
}

void NativeWidgetAura::MoveAbove(gfx::NativeView native_view) {
}

void NativeWidgetAura::MoveToTop() {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::SetShape(gfx::NativeRegion region) {
}

void NativeWidgetAura::Close() {
}

void NativeWidgetAura::CloseNow() {
}

void NativeWidgetAura::EnableClose(bool enable) {
}

void NativeWidgetAura::Show() {
}

void NativeWidgetAura::Hide() {
}

void NativeWidgetAura::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
}

void NativeWidgetAura::ShowWithState(ShowState state) {
}

bool NativeWidgetAura::IsVisible() const {
  return false;
}

void NativeWidgetAura::Activate() {
}

void NativeWidgetAura::Deactivate() {
}

bool NativeWidgetAura::IsActive() const {
  return false;
}

void NativeWidgetAura::SetAlwaysOnTop(bool on_top) {
}

void NativeWidgetAura::Maximize() {
}

void NativeWidgetAura::Minimize() {
}

bool NativeWidgetAura::IsMaximized() const {
  return false;
}

bool NativeWidgetAura::IsMinimized() const {
  return false;
}

void NativeWidgetAura::Restore() {
}

void NativeWidgetAura::SetFullscreen(bool fullscreen) {
}

bool NativeWidgetAura::IsFullscreen() const {
  return false;
}

void NativeWidgetAura::SetOpacity(unsigned char opacity) {
}

void NativeWidgetAura::SetUseDragFrame(bool use_drag_frame) {
}

bool NativeWidgetAura::IsAccessibleWidget() const {
  return false;
}

void NativeWidgetAura::RunShellDrag(View* view,
                                   const ui::OSExchangeData& data,
                                   int operation) {
}

void NativeWidgetAura::SchedulePaintInRect(const gfx::Rect& rect) {
}

void NativeWidgetAura::SetCursor(gfx::NativeCursor cursor) {
}

void NativeWidgetAura::ClearNativeFocus() {
}

void NativeWidgetAura::FocusNativeView(gfx::NativeView native_view) {
}

bool NativeWidgetAura::ConvertPointFromAncestor(const Widget* ancestor,
                                                gfx::Point* point) const {
  NOTREACHED();
  return false;
}

void NativeWidgetAura::DispatchKeyEventPostIME(const KeyEvent& key) {
}

}  // namespace views
