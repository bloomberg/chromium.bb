// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/root_view.h"

namespace ui {

namespace {

// TODO(beng): move to platform file
int GetHorizontalDragThreshold() {
  static int threshold = -1;
  if (threshold == -1)
    threshold = GetSystemMetrics(SM_CXDRAG) / 2;
  return threshold;
}

// TODO(beng): move to platform file
int GetVerticalDragThreshold() {
  static int threshold = -1;
  if (threshold == -1)
    threshold = GetSystemMetrics(SM_CYDRAG) / 2;
  return threshold;
}

bool ExceededDragThreshold(int delta_x, int delta_y) {
  return (abs(delta_x) > GetHorizontalDragThreshold() ||
          abs(delta_y) > GetVerticalDragThreshold());
}

}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

Widget::Widget(View* contents_view)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          native_widget_(NativeWidget::CreateNativeWidget(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          root_view_(new internal::RootView(this, contents_view))),
      is_mouse_button_pressed_(false),
      last_mouse_event_was_move_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_widget_factory_(this)),
      delete_on_destroy_(true) {
}

Widget::~Widget() {
}

void Widget::InitWithNativeViewParent(gfx::NativeView parent,
                                      const gfx::Rect& bounds) {
  native_widget_->InitWithNativeViewParent(parent, bounds);
}

void Widget::InitWithWidgetParent(Widget* parent, const gfx::Rect& bounds) {
  native_widget_->InitWithWidgetParent(parent, bounds);
}

void Widget::InitWithViewParent(View* parent, const gfx::Rect& bounds) {
  native_widget_->InitWithViewParent(parent, bounds);
}

Widget* Widget::GetTopLevelWidget() const {
  NativeWidget* native_widget =
      NativeWidget::GetTopLevelNativeWidget(native_widget_->GetNativeView());
  return native_widget->GetWidget();
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

void Widget::SetShape(const gfx::Path& shape) {
  native_widget_->SetShape(shape);
}

void Widget::Show() {
  native_widget_->Show();
}

void Widget::Hide() {
  native_widget_->Hide();
}

void Widget::Close() {
  native_widget_->Hide();

  if (close_widget_factory_.empty()) {
    MessageLoop::current()->PostTask(FROM_HERE,
      close_widget_factory_.NewRunnableMethod(&Widget::CloseNow));
  }
}

void Widget::MoveAbove(Widget* other) {
  native_widget_->MoveAbove(other->native_widget());
}

void Widget::InvalidateRect(const gfx::Rect& invalid_rect) {
  native_widget_->InvalidateRect(invalid_rect);
}

ThemeProvider* Widget::GetThemeProvider() const {
  return NULL;
}

FocusManager* Widget::GetFocusManager() const {
  return GetTopLevelWidget()->focus_manager_.get();
}

FocusTraversable* Widget::GetFocusTraversable() const {
  return root_view_.get();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, NativeWidgetListener implementation:

void Widget::OnClose() {
  Close();
}

void Widget::OnDestroy() {
  if (delete_on_destroy_)
    delete this;
}

void Widget::OnDisplayChanged() {
  // TODO(beng):
}

bool Widget::OnKeyEvent(const KeyEvent& event) {
  // find root view.

  //return root_view_->OnKeyEvent(event);
  return true;
}

void Widget::OnMouseCaptureLost() {
  if (native_widget_->HasMouseCapture()) {
    if (is_mouse_button_pressed_)
      root_view_->OnMouseCaptureLost();
    is_mouse_button_pressed_ = false;
  }
}

bool Widget::OnMouseEvent(const MouseEvent& event) {
  last_mouse_event_was_move_ = false;
  switch (event.type()) {
    case Event::ET_MOUSE_PRESSED:
      if (root_view_->OnMousePressed(event)) {
        is_mouse_button_pressed_ = true;
        if (!native_widget_->HasMouseCapture())
          native_widget_->SetMouseCapture();
        return true;
      }
      return false;
    case Event::ET_MOUSE_RELEASED:
      // TODO(beng): NativeWidgetGtk should not call this function if drag data
      //             exists, see comment in this function in WidgetGtk.
      // Release the capture first, that way we don't get confused if
      // OnMouseReleased blocks.
      if (native_widget_->HasMouseCapture() &&
          native_widget_->ShouldReleaseCaptureOnMouseReleased()) {
        native_widget_->ReleaseMouseCapture();
      }
      is_mouse_button_pressed_ = false;
      root_view_->OnMouseReleased(event);
      return true;
    case Event::ET_MOUSE_MOVED:
      if (native_widget_->HasMouseCapture() && is_mouse_button_pressed_) {
        last_mouse_event_was_move_ = false;
        root_view_->OnMouseDragged(event);
      } else {
        gfx::Point screen_loc(event.location());
        View::ConvertPointToScreen(root_view_.get(), &screen_loc);
        if (last_mouse_event_was_move_ &&
            last_mouse_event_position_ == screen_loc) {
          // Don't generate a mouse event for the same location as the last.
          return true;
        }
        last_mouse_event_position_ = screen_loc;
        last_mouse_event_was_move_ = true;
        root_view_->OnMouseMoved(event);
      }
      break;
    case Event::ET_MOUSE_EXITED:
      root_view_->OnMouseExited(event);
      return true;
  }
  return true;
}

bool Widget::OnMouseWheelEvent(const MouseWheelEvent& event) {
  return root_view_->OnMouseWheel(event);
}

void Widget::OnNativeWidgetCreated() {
  if (GetTopLevelWidget() == this)
    focus_manager_.reset(new FocusManager(this));
}

void Widget::OnPaint(gfx::Canvas* canvas) {
  root_view_->Paint(canvas);
}

void Widget::OnSizeChanged(const gfx::Size& size) {
  root_view_->SetSize(size);
}

void Widget::OnNativeFocus(gfx::NativeView focused_view) {
  GetFocusManager()->GetWidgetFocusManager()->OnWidgetFocusEvent(
      focused_view, native_widget_->GetNativeView());
}

void Widget::OnNativeBlur(gfx::NativeView focused_view) {
  GetFocusManager()->GetWidgetFocusManager()->OnWidgetFocusEvent(
      native_widget_->GetNativeView(), focused_view);
}

void Widget::OnWorkAreaChanged() {

}

Widget* Widget::GetWidget() const {
  return const_cast<Widget*>(this);
}

////////////////////////////////////////////////////////////////////////////////
// Widget, private:

void Widget::CloseNow() {
  native_widget_->Close();
}

}  // namespace ui

