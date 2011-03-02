// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget.h"

#include "base/logging.h"
#include "views/widget/default_theme_provider.h"
#include "views/widget/root_view.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

Widget::Widget() : native_widget_(NULL), delegate_(NULL) {
}

Widget::~Widget() {
}

void Widget::Init(gfx::NativeView parent, const gfx::Rect& bounds) {
  // TODO(beng): This is called before the native widget is created.
  GetRootView();
  default_theme_provider_.reset(new DefaultThemeProvider);
}

void Widget::InitWithWidget(Widget* parent, const gfx::Rect& bounds) {
}

WidgetDelegate* Widget::GetWidgetDelegate() {
  return delegate_;
}

void Widget::SetWidgetDelegate(WidgetDelegate* delegate) {
  delegate_ = delegate;
}

void Widget::SetContentsView(View* view) {
  root_view_->SetContentsView(view);
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
  if (!root_view_.get()) {
    // First time the root view is being asked for, create it now.
    root_view_.reset(CreateRootView());
  }
  return root_view_.get();
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
  return default_theme_provider_.get();
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
  return root_view_.get();
}

void Widget::ThemeChanged() {
  root_view_->ThemeChanged();
}

void Widget::LocaleChanged() {
  root_view_->LocaleChanged();
}

void Widget::SetFocusTraversableParent(FocusTraversable* parent) {
  root_view_->SetFocusTraversableParent(parent);
}

void Widget::SetFocusTraversableParentView(View* parent_view) {
  root_view_->SetFocusTraversableParentView(parent_view);
}

////////////////////////////////////////////////////////////////////////////////
// Widget, protected:

RootView* Widget::CreateRootView() {
  return new RootView(this);
}

void Widget::DestroyRootView() {
  root_view_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, FocusTraversable implementation:

FocusSearch* Widget::GetFocusSearch() {
  return root_view_->GetFocusSearch();
}

FocusTraversable* Widget::GetFocusTraversableParent() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

View* Widget::GetFocusTraversableParentView() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

}  // namespace views
