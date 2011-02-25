// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

Widget::Widget() {
}

void Widget::Init(gfx::NativeView parent, const gfx::Rect& bounds) {
}

void Widget::InitWithWidget(Widget* parent, const gfx::Rect& bounds) {
}

WidgetDelegate* Widget::GetWidgetDelegate() {
  return NULL;
}

void Widget::SetWidgetDelegate(WidgetDelegate* delegate) {
}

void Widget::SetContentsView(View* view) {
}

void Widget::GetBounds(gfx::Rect* out, bool including_frame) const {
}

void Widget::SetBounds(const gfx::Rect& bounds) {
}

void Widget::MoveAbove(Widget* widget) {
}

void Widget::SetShape(gfx::NativeRegion shape) {
}

void Widget::Close() {
}

void Widget::CloseNow() {
}

void Widget::Show() {
}

void Widget::Hide() {
}

gfx::NativeView Widget::GetNativeView() const {
  return NULL;
}

void Widget::SetOpacity(unsigned char opacity) {
}

void Widget::SetAlwaysOnTop(bool on_top) {
}

RootView* Widget::GetRootView() {
  return NULL;
}

Widget* Widget::GetRootWidget() const {
  return NULL;
}

bool Widget::IsVisible() const {
  return false;
}

bool Widget::IsActive() const {
  return false;
}

bool Widget::IsAccessibleWidget() const {
  return false;
}

void Widget::GenerateMousePressedForView(View* view, const gfx::Point& point) {
}

TooltipManager* Widget::GetTooltipManager() {
  return NULL;
}

bool Widget::GetAccelerator(int cmd_id, ui::Accelerator* accelerator) {
  return false;
}

Window* Widget::GetWindow() {
  return NULL;
}

const Window* Widget::GetWindow() const {
  return NULL;
}

void Widget::SetNativeWindowProperty(const char* name, void* value) {
}

void* Widget::GetNativeWindowProperty(const char* name) {
  return NULL;
}

ThemeProvider* Widget::GetThemeProvider() const {
  return NULL;
}

ThemeProvider* Widget::GetDefaultThemeProvider() const {
  return NULL;
}

FocusManager* Widget::GetFocusManager() {
  return NULL;
}

void Widget::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
}

bool Widget::ContainsNativeView(gfx::NativeView native_view) {
  return false;
}

void Widget::StartDragForViewFromMouseEvent(View* view,
                                            const ui::OSExchangeData& data,
                                            int operation) {
}

View* Widget::GetDraggedView() {
  return NULL;
}

void Widget::SchedulePaintInRect(const gfx::Rect& rect) {
}

void Widget::SetCursor(gfx::NativeCursor cursor) {
}

FocusTraversable* Widget::GetFocusTraversable() {
  return NULL;
}

void Widget::ThemeChanged() {
}

void Widget::LocaleChanged() {
}

}  // namespace views
