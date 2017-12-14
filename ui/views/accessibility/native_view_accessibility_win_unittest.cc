// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <oleacc.h>
#include <wrl/client.h>

#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/views/accessibility/native_view_accessibility_base.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"

using Microsoft::WRL::ComPtr;
using base::win::ScopedBstr;
using base::win::ScopedVariant;

namespace views {
namespace test {

namespace {

// Whether |left| represents the same COM object as |right|.
template <typename T, typename U>
bool IsSameObject(T* left, U* right) {
  if (!left && !right)
    return true;

  if (!left || !right)
    return false;

  ComPtr<IUnknown> left_unknown;
  left->QueryInterface(IID_PPV_ARGS(&left_unknown));

  ComPtr<IUnknown> right_unknown;
  right->QueryInterface(IID_PPV_ARGS(&right_unknown));

  return left_unknown == right_unknown;
}

}  // namespace

class NativeViewAccessibilityWinTest : public ViewsTestBase {
 public:
  NativeViewAccessibilityWinTest() {}
  ~NativeViewAccessibilityWinTest() override {}

 protected:
  void GetIAccessible2InterfaceForView(View* view, IAccessible2_2** result) {
    ComPtr<IAccessible> view_accessible(view->GetNativeViewAccessible());
    ComPtr<IServiceProvider> service_provider;
    ASSERT_EQ(S_OK, view_accessible.CopyTo(service_provider.GetAddressOf()));
    ASSERT_EQ(S_OK,
        service_provider->QueryService(IID_IAccessible2_2, result));
  }
};

TEST_F(NativeViewAccessibilityWinTest, TextfieldAccessibility) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);

  View* content = new View;
  widget.SetContentsView(content);

  Textfield* textfield = new Textfield;
  textfield->SetAccessibleName(L"Name");
  textfield->SetText(L"Value");
  content->AddChildView(textfield);

  ComPtr<IAccessible> content_accessible(content->GetNativeViewAccessible());
  LONG child_count = 0;
  ASSERT_EQ(S_OK, content_accessible->get_accChildCount(&child_count));
  ASSERT_EQ(1L, child_count);

  ComPtr<IDispatch> textfield_dispatch;
  ComPtr<IAccessible> textfield_accessible;
  ScopedVariant child_index(1);
  ASSERT_EQ(S_OK, content_accessible->get_accChild(
                      child_index, textfield_dispatch.GetAddressOf()));
  ASSERT_EQ(S_OK,
            textfield_dispatch.CopyTo(textfield_accessible.GetAddressOf()));

  ScopedBstr name;
  ScopedVariant childid_self(CHILDID_SELF);
  ASSERT_EQ(S_OK, textfield_accessible->get_accName(
      childid_self, name.Receive()));
  ASSERT_STREQ(L"Name", name);

  ScopedBstr value;
  ASSERT_EQ(S_OK, textfield_accessible->get_accValue(
      childid_self, value.Receive()));
  ASSERT_STREQ(L"Value", value);

  ScopedBstr new_value(L"New value");
  ASSERT_EQ(S_OK, textfield_accessible->put_accValue(childid_self, new_value));

  ASSERT_STREQ(L"New value", textfield->text().c_str());
}

// A subclass of NativeViewAccessibilityWinTest that we run twice,
// first where we create an transient child widget (child = false), the second
// time where we create a child widget (child = true).
class NativeViewAccessibilityWinTestWithBoolChildFlag
    : public NativeViewAccessibilityWinTest,
      public testing::WithParamInterface<bool> {
 public:
  NativeViewAccessibilityWinTestWithBoolChildFlag() {}
  ~NativeViewAccessibilityWinTestWithBoolChildFlag() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeViewAccessibilityWinTestWithBoolChildFlag);
};

INSTANTIATE_TEST_CASE_P(,
                        NativeViewAccessibilityWinTestWithBoolChildFlag,
                        testing::Bool());

TEST_P(NativeViewAccessibilityWinTestWithBoolChildFlag, AuraChildWidgets) {
  // Create the parent widget.
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.bounds = gfx::Rect(0, 0, 400, 200);
  widget.Init(init_params);
  widget.Show();

  // Initially it has 1 child.
  ComPtr<IAccessible> root_view_accessible(
      widget.GetRootView()->GetNativeViewAccessible());
  LONG child_count = 0;
  ASSERT_EQ(S_OK, root_view_accessible->get_accChildCount(&child_count));
  ASSERT_EQ(1L, child_count);

  // Create the child widget, one of two ways (see below).
  Widget child_widget;
  Widget::InitParams child_init_params =
      CreateParams(Widget::InitParams::TYPE_BUBBLE);
  child_init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  child_init_params.parent = widget.GetNativeView();
  child_init_params.bounds = gfx::Rect(30, 40, 100, 50);

  // NOTE: this test is run two times, GetParam() returns a different
  // value each time. The first time we test with child = false,
  // making this an owned widget (a transient child).  The second time
  // we test with child = true, making it a child widget.
  child_init_params.child = GetParam();

  child_widget.Init(child_init_params);
  child_widget.Show();

  // Now the IAccessible for the parent widget should have 2 children.
  ASSERT_EQ(S_OK, root_view_accessible->get_accChildCount(&child_count));
  ASSERT_EQ(2L, child_count);

  // Ensure the bounds of the parent widget are as expected.
  ScopedVariant childid_self(CHILDID_SELF);
  LONG x, y, width, height;
  ASSERT_EQ(S_OK, root_view_accessible->accLocation(&x, &y, &width, &height,
                                                    childid_self));
  EXPECT_EQ(0, x);
  EXPECT_EQ(0, y);
  EXPECT_EQ(400, width);
  EXPECT_EQ(200, height);

  // Get the IAccessible for the second child of the parent widget,
  // which should be the one for our child widget.
  ComPtr<IDispatch> child_widget_dispatch;
  ComPtr<IAccessible> child_widget_accessible;
  ScopedVariant child_index_2(2);
  ASSERT_EQ(S_OK, root_view_accessible->get_accChild(
                      child_index_2, child_widget_dispatch.GetAddressOf()));
  ASSERT_EQ(S_OK, child_widget_dispatch.CopyTo(
                      child_widget_accessible.GetAddressOf()));

  // Check the bounds of the IAccessible for the child widget.
  // This is a sanity check to make sure we have the right object
  // and not some other view.
  ASSERT_EQ(S_OK, child_widget_accessible->accLocation(&x, &y, &width, &height,
                                                       childid_self));
  EXPECT_EQ(30, x);
  EXPECT_EQ(40, y);
  EXPECT_EQ(100, width);
  EXPECT_EQ(50, height);

  // Now make sure that querying the parent of the child gets us back to
  // the original parent.
  ComPtr<IDispatch> child_widget_parent_dispatch;
  ComPtr<IAccessible> child_widget_parent_accessible;
  ASSERT_EQ(S_OK, child_widget_accessible->get_accParent(
                      child_widget_parent_dispatch.GetAddressOf()));
  ASSERT_EQ(S_OK, child_widget_parent_dispatch.CopyTo(
                      child_widget_parent_accessible.GetAddressOf()));
  EXPECT_EQ(root_view_accessible.Get(), child_widget_parent_accessible.Get());
}

// Flaky on Windows: https://crbug.com/461837.
TEST_F(NativeViewAccessibilityWinTest, DISABLED_RetrieveAllAlerts) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);

  View* content = new View;
  widget.SetContentsView(content);

  View* infobar = new View;
  content->AddChildView(infobar);

  View* infobar2 = new View;
  content->AddChildView(infobar2);

  View* root_view = content->parent();
  ASSERT_EQ(NULL, root_view->parent());

  ComPtr<IAccessible2_2> root_view_accessible;
  GetIAccessible2InterfaceForView(root_view,
                                  root_view_accessible.GetAddressOf());

  ComPtr<IAccessible2_2> infobar_accessible;
  GetIAccessible2InterfaceForView(infobar, infobar_accessible.GetAddressOf());

  ComPtr<IAccessible2_2> infobar2_accessible;
  GetIAccessible2InterfaceForView(infobar2, infobar2_accessible.GetAddressOf());

  // Initially, there are no alerts
  ScopedBstr alerts_bstr(L"alerts");
  IUnknown** targets;
  long n_targets;
  ASSERT_EQ(S_FALSE, root_view_accessible->get_relationTargetsOfType(
      alerts_bstr, 0, &targets, &n_targets));
  ASSERT_EQ(0, n_targets);

  // Fire alert events on the infobars.
  infobar->NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
  infobar2->NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);

  // Now calling get_relationTargetsOfType should retrieve the alerts.
  ASSERT_EQ(S_OK, root_view_accessible->get_relationTargetsOfType(
      alerts_bstr, 0, &targets, &n_targets));
  ASSERT_EQ(2, n_targets);
  ASSERT_TRUE(IsSameObject(infobar_accessible.Get(), targets[0]));
  ASSERT_TRUE(IsSameObject(infobar2_accessible.Get(), targets[1]));
  CoTaskMemFree(targets);

  // If we set max_targets to 1, we should only get the first one.
  ASSERT_EQ(S_OK, root_view_accessible->get_relationTargetsOfType(
      alerts_bstr, 1, &targets, &n_targets));
  ASSERT_EQ(1, n_targets);
  ASSERT_TRUE(IsSameObject(infobar_accessible.Get(), targets[0]));
  CoTaskMemFree(targets);

  // If we delete the first view, we should only get the second one now.
  delete infobar;
  ASSERT_EQ(S_OK, root_view_accessible->get_relationTargetsOfType(
      alerts_bstr, 0, &targets, &n_targets));
  ASSERT_EQ(1, n_targets);
  ASSERT_TRUE(IsSameObject(infobar2_accessible.Get(), targets[0]));
  CoTaskMemFree(targets);
}

// Test trying to retrieve child widgets during window close does not crash.
TEST_F(NativeViewAccessibilityWinTest, GetAllOwnedWidgetsCrash) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);
  widget.CloseNow();

  LONG child_count = 0;
  ComPtr<IAccessible> content_accessible(
      widget.GetRootView()->GetNativeViewAccessible());
  EXPECT_EQ(S_OK, content_accessible->get_accChildCount(&child_count));
  EXPECT_EQ(1L, child_count);
}

TEST_F(NativeViewAccessibilityWinTest, WindowHasRoleApplication) {
  // We expect that our internal window object does not expose
  // ROLE_SYSTEM_WINDOW, but ROLE_SYSTEM_APPLICATION instead.
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);

  ComPtr<IAccessible> accessible(
      widget.GetRootView()->GetNativeViewAccessible());
  ScopedVariant childid_self(CHILDID_SELF);
  ScopedVariant role;
  EXPECT_EQ(S_OK, accessible->get_accRole(childid_self, role.Receive()));
  EXPECT_EQ(role.type(), VT_I4);
  EXPECT_EQ(V_I4(role.ptr()), ROLE_SYSTEM_APPLICATION);
}
}  // namespace test
}  // namespace views
