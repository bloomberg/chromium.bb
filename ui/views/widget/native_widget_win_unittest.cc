// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/window_impl.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_test_util.h"

namespace ui {

class NativeWidgetTest : public testing::Test {
 public:
  NativeWidgetTest() {}
  virtual ~NativeWidgetTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWidgetTest);
};

class TestWindowImpl : public WindowImpl {
 public:
  TestWindowImpl() {}
  virtual ~TestWindowImpl() {}

  virtual BOOL ProcessWindowMessage(HWND window,
                                    UINT message,
                                    WPARAM w_param,
                                    LPARAM l_param,
                                    LRESULT& result,
                                    DWORD msg_mad_id = 0) {
    return FALSE;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWindowImpl);
};

TEST_F(NativeWidgetTest, CreateNativeWidget) {
  scoped_ptr<Widget> widget(internal::CreateWidget());
  EXPECT_TRUE(widget->native_widget()->GetNativeView() != NULL);
}

TEST_F(NativeWidgetTest, GetNativeWidgetForNativeView) {
  scoped_ptr<Widget> widget(internal::CreateWidget());
  NativeWidget* a = widget->native_widget();
  HWND nv = widget->native_widget()->GetNativeView();
  NativeWidget* b = NativeWidget::GetNativeWidgetForNativeView(nv);
  EXPECT_EQ(a, b);
}

// |widget| has the toplevel NativeWidget.
TEST_F(NativeWidgetTest, GetTopLevelNativeWidget1) {
  scoped_ptr<Widget> widget(internal::CreateWidget());
  EXPECT_EQ(widget->native_widget(),
            NativeWidget::GetTopLevelNativeWidget(
                widget->native_widget()->GetNativeView()));
}

// |toplevel_widget| has the toplevel NativeWidget.
TEST_F(NativeWidgetTest, GetTopLevelNativeWidget2) {
  scoped_ptr<Widget> child_widget(internal::CreateWidget());
  scoped_ptr<Widget> toplevel_widget(internal::CreateWidget());
  SetParent(child_widget->native_widget()->GetNativeView(),
            toplevel_widget->native_widget()->GetNativeView());
  EXPECT_EQ(toplevel_widget->native_widget(),
            NativeWidget::GetTopLevelNativeWidget(
                child_widget->native_widget()->GetNativeView()));
}

// |child_widget| has the toplevel NativeWidget.
TEST_F(NativeWidgetTest, GetTopLevelNativeWidget3) {
  scoped_ptr<Widget> child_widget(internal::CreateWidget());

  TestWindowImpl toplevel;
  toplevel.Init(NULL, gfx::Rect(10, 10, 100, 100));

  SetParent(child_widget->native_widget()->GetNativeView(), toplevel.hwnd());
  EXPECT_EQ(child_widget->native_widget(),
            NativeWidget::GetTopLevelNativeWidget(
                child_widget->native_widget()->GetNativeView()));
}

}  // namespace ui
