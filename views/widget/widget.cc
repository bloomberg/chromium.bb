// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget.h"

#include "base/logging.h"
#include "views/widget/default_theme_provider.h"
#include "views/widget/root_view.h"
#include "views/widget/native_widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

Widget::Widget()
    : native_widget_(NULL),
      widget_delegate_(NULL),
      dragged_view_(NULL) {
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

Widget* Widget::GetTopLevelWidget() {
  return const_cast<Widget*>(
      const_cast<const Widget*>(this)->GetTopLevelWidget());
}

const Widget* Widget::GetTopLevelWidget() const {
  NativeWidget* native_widget =
      NativeWidget::GetTopLevelNativeWidget(GetNativeView());
  return native_widget ? native_widget->GetWidget() : NULL;
}

void Widget::SetContentsView(View* view) {
  root_view_->SetContentsView(view);
}

gfx::Rect Widget::GetWindowScreenBounds() const {
  return native_widget_->GetWindowScreenBounds();
}

gfx::Rect Widget::GetClientAreaScreenBounds() const {
  return native_widget_->GetClientAreaScreenBounds();
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

bool Widget::GetAccelerator(int cmd_id, ui::Accelerator* accelerator) {
  return false;
}

Window* Widget::GetWindow() {
  return NULL;
}

const Window* Widget::GetWindow() const {
  return NULL;
}

ThemeProvider* Widget::GetThemeProvider() const {
  const Widget* root_widget = GetTopLevelWidget();
  if (root_widget && root_widget != this) {
    // Attempt to get the theme provider, and fall back to the default theme
    // provider if not found.
    ThemeProvider* provider = root_widget->GetThemeProvider();
    if (provider)
      return provider;

    provider = root_widget->default_theme_provider_.get();
    if (provider)
      return provider;
  }
  return default_theme_provider_.get();
}

FocusManager* Widget::GetFocusManager() {
  return NULL;
}

void Widget::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (!is_add && child == dragged_view_)
    dragged_view_ = NULL;
}

bool Widget::ContainsNativeView(gfx::NativeView native_view) {
  if (native_widget_->ContainsNativeView(native_view))
    return true;

  // A views::NativeViewHost may contain the given native view, without it being
  // an ancestor of hwnd(), so traverse the views::View hierarchy looking for
  // such views.
  return GetRootView()->ContainsNativeView(native_view);
}

void Widget::RunShellDrag(View* view, const ui::OSExchangeData& data,
                          int operation) {
  dragged_view_ = view;
  native_widget_->RunShellDrag(view, data, operation);
  // If the view is removed during the drag operation, dragged_view_ is set to
  // NULL.
  if (view && dragged_view_ == view) {
    dragged_view_ = NULL;
    view->OnDragDone();
  }
}

void Widget::SchedulePaintInRect(const gfx::Rect& rect) {
  native_widget_->SchedulePaintInRect(rect);
}

void Widget::SetCursor(gfx::NativeCursor cursor) {
  native_widget_->SetCursor(cursor);
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

////////////////////////////////////////////////////////////////////////////////
// Widget, protected:

RootView* Widget::CreateRootView() {
  return new RootView(this);
}

void Widget::DestroyRootView() {
  root_view_.reset();
}

}  // namespace views
