// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_test.h"

#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/root_view.h"

namespace views {
namespace test {
namespace {

// Helper to encapsulate common parts of the WidgetTest::Create* methods,
template <class NativeWidgetType>
Widget* CreateHelper(Widget::InitParams params) {
  Widget* widget = new Widget;
  params.native_widget = new NativeWidgetType(widget);
  widget->Init(params);
  return widget;
}

Widget* CreateHelper(Widget::InitParams params) {
  return CreateHelper<NativeWidgetCapture>(params);
}

}  // namespace

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

Widget* WidgetTest::CreateTopLevelPlatformWidget() {
  return CreateHelper(CreateParams(Widget::InitParams::TYPE_WINDOW));
}

Widget* WidgetTest::CreateTopLevelFramelessPlatformWidget() {
  return CreateHelper(CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS));
}

Widget* WidgetTest::CreateChildPlatformWidget(
    gfx::NativeView parent_native_view) {
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_CONTROL);
  params.parent = parent_native_view;
  Widget* child = CreateHelper(params);
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

Widget* WidgetTest::CreateNativeDesktopWidget() {
#if defined(OS_CHROMEOS)
  return CreateHelper<PlatformNativeWidget>(
      CreateParams(Widget::InitParams::TYPE_WINDOW));
#else
  return CreateHelper<PlatformDesktopNativeWidget>(
      CreateParams(Widget::InitParams::TYPE_WINDOW));
#endif
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

TestDesktopWidgetDelegate::TestDesktopWidgetDelegate() : widget_(new Widget) {
}

TestDesktopWidgetDelegate::~TestDesktopWidgetDelegate() {
  if (widget_)
    widget_->CloseNow();
  EXPECT_FALSE(widget_);
}

void TestDesktopWidgetDelegate::InitWidget(Widget::InitParams init_params) {
  init_params.delegate = this;
#if !defined(OS_CHROMEOS)
  init_params.native_widget = new PlatformDesktopNativeWidget(widget_);
#endif
  init_params.bounds = initial_bounds_;
  widget_->Init(init_params);
}

void TestDesktopWidgetDelegate::WindowClosing() {
  window_closing_count_++;
  widget_ = nullptr;
}

Widget* TestDesktopWidgetDelegate::GetWidget() {
  return widget_;
}

const Widget* TestDesktopWidgetDelegate::GetWidget() const {
  return widget_;
}

View* TestDesktopWidgetDelegate::GetContentsView() {
  return contents_view_ ? contents_view_ : WidgetDelegate::GetContentsView();
}

bool TestDesktopWidgetDelegate::ShouldAdvanceFocusToTopLevelWidget() const {
  return true;  // Same default as DefaultWidgetDelegate in widget.cc.
}

}  // namespace test
}  // namespace views
