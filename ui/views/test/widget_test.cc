// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_test.h"

#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/root_view.h"

namespace views {
namespace test {

// A widget that assumes mouse capture always works. It won't in testing, so we
// mock it.
NativeWidgetCapture::NativeWidgetCapture(
    internal::NativeWidgetDelegate* delegate)
    : PlatformNativeWidget(delegate),
      mouse_capture_(false) {}

NativeWidgetCapture::~NativeWidgetCapture() {}

void NativeWidgetCapture::SetCapture() {
  mouse_capture_ = true;
}

void NativeWidgetCapture::ReleaseCapture() {
  if (mouse_capture_)
    delegate()->OnMouseCaptureLost();
  mouse_capture_ = false;
}

bool NativeWidgetCapture::HasCapture() const {
  return mouse_capture_;
}

WidgetTest::WidgetTest() {}
WidgetTest::~WidgetTest() {}

NativeWidget* WidgetTest::CreatePlatformNativeWidget(
    internal::NativeWidgetDelegate* delegate) {
  return new NativeWidgetCapture(delegate);
}

Widget* WidgetTest::CreateTopLevelPlatformWidget() {
  Widget* toplevel = new Widget;
  Widget::InitParams toplevel_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  toplevel_params.native_widget = CreatePlatformNativeWidget(toplevel);
  toplevel->Init(toplevel_params);
  return toplevel;
}

Widget* WidgetTest::CreateTopLevelFramelessPlatformWidget() {
  Widget* toplevel = new Widget;
  Widget::InitParams toplevel_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  toplevel_params.native_widget = CreatePlatformNativeWidget(toplevel);
  toplevel->Init(toplevel_params);
  return toplevel;
}

Widget* WidgetTest::CreateChildPlatformWidget(
    gfx::NativeView parent_native_view) {
  Widget* child = new Widget;
  Widget::InitParams child_params =
      CreateParams(Widget::InitParams::TYPE_CONTROL);
  child_params.native_widget = CreatePlatformNativeWidget(child);
  child_params.parent = parent_native_view;
  child->Init(child_params);
  child->SetContentsView(new View);
  return child;
}

Widget* WidgetTest::CreateTopLevelNativeWidget() {
  Widget* toplevel = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  toplevel->Init(params);
  return toplevel;
}

Widget* WidgetTest::CreateChildNativeWidgetWithParent(Widget* parent) {
  Widget* child = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_CONTROL);
  params.parent = parent->GetNativeView();
  child->Init(params);
  child->SetContentsView(new View);
  return child;
}

Widget* WidgetTest::CreateChildNativeWidget() {
  return CreateChildNativeWidgetWithParent(NULL);
}

View* WidgetTest::GetMousePressedHandler(internal::RootView* root_view) {
  return root_view->mouse_pressed_handler_;
}

View* WidgetTest::GetMouseMoveHandler(internal::RootView* root_view) {
  return root_view->mouse_move_handler_;
}

View* WidgetTest::GetGestureHandler(internal::RootView* root_view) {
  return root_view->gesture_handler_;
}

}  // namespace test
}  // namespace views
