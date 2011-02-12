// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget_impl.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// WidgetImpl, public:

WidgetImpl::WidgetImpl() {
}

WidgetImpl::~WidgetImpl() {
}

////////////////////////////////////////////////////////////////////////////////
// WidgetImpl, Widget implementation:

void WidgetImpl::Init(gfx::NativeView parent, const gfx::Rect& bounds) {

}

void WidgetImpl::InitWithWidget(Widget* parent, const gfx::Rect& bounds) {

}

WidgetDelegate* WidgetImpl::GetWidgetDelegate() {
  return NULL;
}

void WidgetImpl::SetWidgetDelegate(WidgetDelegate* delegate) {

}

void WidgetImpl::SetContentsView(View* view) {

}

void WidgetImpl::GetBounds(gfx::Rect* out, bool including_frame) const {

}

void WidgetImpl::SetBounds(const gfx::Rect& bounds) {

}

void WidgetImpl::MoveAbove(Widget* other) {

}

void WidgetImpl::SetShape(gfx::NativeRegion region) {

}

void WidgetImpl::Close() {

}

void WidgetImpl::CloseNow() {

}

void WidgetImpl::Show() {

}

void WidgetImpl::Hide() {

}

gfx::NativeView WidgetImpl::GetNativeView() const {
  return NULL;
}

void WidgetImpl::PaintNow(const gfx::Rect& update_rect) {

}

void WidgetImpl::SetOpacity(unsigned char opacity) {
}

void WidgetImpl::SetAlwaysOnTop(bool on_top) {

}

RootView* WidgetImpl::GetRootView() {
  return NULL;
}

Widget* WidgetImpl::GetRootWidget() const {
  return NULL;
}

bool WidgetImpl::IsVisible() const {
  return false;
}

bool WidgetImpl::IsActive() const {
  return false;
}

bool WidgetImpl::IsAccessibleWidget() const {
  return false;
}

TooltipManager* WidgetImpl::GetTooltipManager() {
  return NULL;
}

void WidgetImpl::GenerateMousePressedForView(View* view,
                                             const gfx::Point& point) {

}

bool WidgetImpl::GetAccelerator(int cmd_id, ui::Accelerator* accelerator) {
  return false;
}

Window* WidgetImpl::GetWindow() {
  return NULL;
}

const Window* WidgetImpl::GetWindow() const {
  return NULL;
}

void WidgetImpl::SetNativeWindowProperty(const char* name, void* value) {

}

void* WidgetImpl::GetNativeWindowProperty(const char* name) {
  return NULL;
}

ThemeProvider* WidgetImpl::GetThemeProvider() const {
  return NULL;
}

ThemeProvider* WidgetImpl::GetDefaultThemeProvider() const {
  return NULL;
}

FocusManager* WidgetImpl::GetFocusManager() {
  return NULL;
}

void WidgetImpl::ViewHierarchyChanged(bool is_add, View *parent,
                                      View *child) {

}

bool WidgetImpl::ContainsNativeView(gfx::NativeView native_view) {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// WidgetImpl, NativeWidgetListener implementation:

void WidgetImpl::OnClose() {

}

void WidgetImpl::OnDestroy() {

}

void WidgetImpl::OnDisplayChanged() {

}

bool WidgetImpl::OnKeyEvent(const KeyEvent& event) {
  return false;
}

void WidgetImpl::OnMouseCaptureLost() {

}

bool WidgetImpl::OnMouseEvent(const MouseEvent& event) {
  return false;
}

bool WidgetImpl::OnMouseWheelEvent(const MouseWheelEvent& event) {
  return false;
}

void WidgetImpl::OnNativeWidgetCreated() {

}

void WidgetImpl::OnPaint(gfx::Canvas* canvas) {

}

void WidgetImpl::OnSizeChanged(const gfx::Size& size) {

}

void WidgetImpl::OnNativeFocus(gfx::NativeView focused_view) {

}

void WidgetImpl::OnNativeBlur(gfx::NativeView focused_view) {

}

void WidgetImpl::OnWorkAreaChanged() {

}

WidgetImpl* WidgetImpl::GetWidgetImpl() const {
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// WidgetImpl, private:

}  // namespace views
