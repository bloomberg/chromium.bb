// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "views/test/views_test_base.h"
#include "views/view.h"
#include "views/controls/native/native_view_host.h"
#include "views/widget/native_widget.h"
#include "views/widget/widget.h"
#include "views/widget/native_widget_test_utils.h"

#if defined(TOOLKIT_USES_GTK)
#include "views/widget/widget_gtk.h"
#endif

namespace views {

class ScopedTestWidget {
 public:
  ScopedTestWidget(NativeWidget* native_widget)
      : native_widget_(native_widget) {
  }
  ~ScopedTestWidget() {
    native_widget_->GetWidget()->CloseNow();
  }

  NativeWidget* operator->() const  {
    return native_widget_.get();
  }
  NativeWidget* get() const { return native_widget_.get(); }

 private:
  scoped_ptr<NativeWidget> native_widget_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTestWidget);
};

class NativeWidgetTest : public ViewsTestBase {
 public:
  NativeWidgetTest() {}
  virtual ~NativeWidgetTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWidgetTest);
};

TEST_F(NativeWidgetTest, CreateNativeWidget) {
  ScopedTestWidget widget(internal::CreateNativeWidget());
  EXPECT_TRUE(widget->GetWidget()->GetNativeView() != NULL);
}

TEST_F(NativeWidgetTest, GetNativeWidgetForNativeView) {
  ScopedTestWidget widget(internal::CreateNativeWidget());
  EXPECT_EQ(widget.get(),
            NativeWidget::GetNativeWidgetForNativeView(
                widget->GetWidget()->GetNativeView()));
}

// |widget| has the toplevel NativeWidget.
TEST_F(NativeWidgetTest, GetTopLevelNativeWidget1) {
  ScopedTestWidget widget(internal::CreateNativeWidget());
  EXPECT_EQ(widget.get(),
            NativeWidget::GetTopLevelNativeWidget(
                widget->GetWidget()->GetNativeView()));
}

// |toplevel_widget| has the toplevel NativeWidget.
TEST_F(NativeWidgetTest, GetTopLevelNativeWidget2) {
  ScopedTestWidget child_widget(internal::CreateNativeWidgetWithParent(NULL));
  ScopedTestWidget toplevel_widget(internal::CreateNativeWidget());

  NativeViewHost* child_host = new NativeViewHost;
  toplevel_widget->GetWidget()->SetContentsView(child_host);
  child_host->Attach(child_widget->GetWidget()->GetNativeView());

  EXPECT_EQ(toplevel_widget.get(),
            NativeWidget::GetTopLevelNativeWidget(
                child_widget->GetWidget()->GetNativeView()));
}

}  // namespace views
