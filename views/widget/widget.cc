// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "views/focus/view_storage.h"
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

// Unconverted methods (see header) --------------------------------------------

void Widget::Init(gfx::NativeView parent, const gfx::Rect& bounds) {
  GetRootView();
  default_theme_provider_.reset(new DefaultThemeProvider);
}

void Widget::InitWithWidget(Widget* parent, const gfx::Rect& bounds) {
}

gfx::NativeView Widget::GetNativeView() const {
  return NULL;
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

void Widget::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (!is_add) {
    if (child == dragged_view_)
      dragged_view_ = NULL;

    FocusManager* focus_manager = GetFocusManager();
    if (focus_manager) {
      if (focus_manager->GetFocusedView() == child)
        focus_manager->SetFocusedView(NULL);
      focus_manager->ViewRemoved(parent, child);
    }
    ViewStorage::GetInstance()->ViewRemoved(parent, child);
  }
}

// Converted methods (see header) ----------------------------------------------

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
  native_widget_->SetBounds(bounds);
}

void Widget::MoveAbove(Widget* widget) {
  native_widget_->MoveAbove(widget);
}

void Widget::SetShape(gfx::NativeRegion shape) {
  native_widget_->SetShape(shape);
}

void Widget::Close() {
  native_widget_->Close();
}

void Widget::CloseNow() {
  native_widget_->CloseNow();
}

void Widget::Show() {
  native_widget_->Show();
}

void Widget::Hide() {
  native_widget_->Hide();
}

void Widget::SetOpacity(unsigned char opacity) {
  native_widget_->SetOpacity(opacity);
}

void Widget::SetAlwaysOnTop(bool on_top) {
  native_widget_->SetAlwaysOnTop(on_top);
}

RootView* Widget::GetRootView() {
  if (!root_view_.get()) {
    // First time the root view is being asked for, create it now.
    root_view_.reset(CreateRootView());
  }
  return root_view_.get();
}

bool Widget::IsVisible() const {
  return native_widget_->IsVisible();
}

bool Widget::IsActive() const {
  return native_widget_->IsActive();
}

bool Widget::IsAccessibleWidget() const {
  return native_widget_->IsAccessibleWidget();
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
  Widget* toplevel_widget = GetTopLevelWidget();
  if (toplevel_widget && toplevel_widget != this)
    return toplevel_widget->focus_manager_.get();

  return focus_manager_.get();
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
// Widget, NativeWidgetDelegate implementation:

void Widget::OnNativeFocus(gfx::NativeView focused_view) {
  GetFocusManager()->GetWidgetFocusManager()->OnWidgetFocusEvent(
      focused_view,
      GetNativeView());
}

void Widget::OnNativeBlur(gfx::NativeView focused_view) {
  GetFocusManager()->GetWidgetFocusManager()->OnWidgetFocusEvent(
      GetNativeView(),
      focused_view);
}

void Widget::OnNativeWidgetCreated() {
  if (GetTopLevelWidget() == this) {
    // Only the top level Widget in a native widget hierarchy has a focus
    // manager.
    focus_manager_.reset(new FocusManager(this));
  }
}

void Widget::OnSizeChanged(const gfx::Size& new_size) {
  root_view_->SetSize(new_size);
}

bool Widget::HasFocusManager() const {
  return !!focus_manager_.get();
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

  // Defer focus manager's destruction. This is for the case when the
  // focus manager is referenced by a child WidgetGtk (e.g. TabbedPane in a
  // dialog). When gtk_widget_destroy is called on the parent, the destroy
  // signal reaches parent first and then the child. Thus causing the parent
  // WidgetGtk's dtor executed before the child's. If child's view hierarchy
  // references this focus manager, it crashes. This will defer focus manager's
  // destruction after child WidgetGtk's dtor.
  FocusManager* focus_manager = focus_manager_.release();
  if (focus_manager)
    MessageLoop::current()->DeleteSoon(FROM_HERE, focus_manager);
}

void Widget::ReplaceFocusManager(FocusManager* focus_manager) {
  focus_manager_.reset(focus_manager);
}

}  // namespace views
