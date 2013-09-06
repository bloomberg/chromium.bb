// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <oleacc.h>

#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"

namespace views {
namespace test {

typedef ViewsTestBase NativeViewAcccessibilityWinTest;

TEST_F(NativeViewAcccessibilityWinTest, TextfieldAccessibility) {
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

  base::win::ScopedComPtr<IAccessible> content_accessible(
      content->GetNativeViewAccessible());
  LONG child_count = 0;
  ASSERT_EQ(S_OK, content_accessible->get_accChildCount(&child_count));
  ASSERT_EQ(1L, child_count);

  base::win::ScopedComPtr<IDispatch> textfield_dispatch;
  base::win::ScopedComPtr<IAccessible> textfield_accessible;
  base::win::ScopedVariant child_index(1);
  ASSERT_EQ(S_OK, content_accessible->get_accChild(
      child_index, textfield_dispatch.Receive()));
  ASSERT_EQ(S_OK, textfield_dispatch.QueryInterface(
      textfield_accessible.Receive()));

  base::win::ScopedBstr name;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  ASSERT_EQ(S_OK, textfield_accessible->get_accName(
      childid_self, name.Receive()));
  ASSERT_STREQ(L"Name", name);

  base::win::ScopedBstr value;
  ASSERT_EQ(S_OK, textfield_accessible->get_accValue(
      childid_self, value.Receive()));
  ASSERT_STREQ(L"Value", value);

  base::win::ScopedBstr new_value(L"New value");
  ASSERT_EQ(S_OK, textfield_accessible->put_accValue(childid_self, new_value));

  ASSERT_STREQ(L"New value", textfield->text().c_str());
}

}  // namespace test
}  // namespace views
