// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_variables.h"

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {
namespace {

using namespace css_test_helpers;

class StyleVariablesTest : public PageTestBase {
 public:
  scoped_refptr<CSSVariableData> CreateData(String s) {
    auto tokens = CSSTokenizer(s).TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    bool is_animation_tainted = false;
    bool needs_variable_resolution = false;
    return CSSVariableData::Create(range, is_animation_tainted,
                                   needs_variable_resolution, KURL(),
                                   WTF::TextEncoding());
  }

  const CSSValue* CreateValue(AtomicString s) {
    return MakeGarbageCollected<CSSCustomIdentValue>(s);
  }
};

TEST_F(StyleVariablesTest, EmptyEqual) {
  StyleVariables vars1;
  StyleVariables vars2;
  EXPECT_EQ(vars1, vars1);
  EXPECT_EQ(vars2, vars2);
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, Copy) {
  auto foo_data = CreateData("foo");
  const CSSValue* foo_value = CreateValue("foo");

  StyleVariables vars1;
  vars1.SetData("--x", foo_data);
  vars1.SetValue("--x", foo_value);

  StyleVariables vars2(vars1);
  EXPECT_EQ(foo_data, vars2.GetData("--x").value_or(nullptr));
  EXPECT_EQ(foo_value, vars2.GetValue("--x").value_or(nullptr));
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, GetNames) {
  StyleVariables vars;
  vars.SetData("--x", CreateData("foo"));
  vars.SetData("--y", CreateData("bar"));

  HashSet<AtomicString> names = vars.GetNames();
  EXPECT_EQ(2u, names.size());
  EXPECT_TRUE(names.Contains("--x"));
  EXPECT_TRUE(names.Contains("--y"));
}

// CSSVariableData

TEST_F(StyleVariablesTest, IsEmptyData) {
  StyleVariables vars;
  EXPECT_TRUE(vars.IsEmpty());
  vars.SetData("--x", CreateData("foo"));
  EXPECT_FALSE(vars.IsEmpty());
}

TEST_F(StyleVariablesTest, SetData) {
  StyleVariables vars;

  auto foo = CreateData("foo");
  auto bar = CreateData("bar");

  EXPECT_FALSE(vars.GetData("--x").has_value());

  vars.SetData("--x", foo);
  EXPECT_EQ(foo, vars.GetData("--x").value_or(nullptr));

  vars.SetData("--x", bar);
  EXPECT_EQ(bar, vars.GetData("--x").value_or(nullptr));
}

TEST_F(StyleVariablesTest, SetNullData) {
  StyleVariables vars;
  EXPECT_FALSE(vars.GetData("--x").has_value());
  vars.SetData("--x", nullptr);
  auto data = vars.GetData("--x");
  ASSERT_TRUE(data.has_value());
  EXPECT_EQ(nullptr, data.value());
}

TEST_F(StyleVariablesTest, SingleDataSamePointer) {
  auto data = CreateData("foo");
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetData("--x", data);
  vars2.SetData("--x", data);
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, SingleDataSameContent) {
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetData("--x", CreateData("foo"));
  vars2.SetData("--x", CreateData("foo"));
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, SingleDataContentNotEqual) {
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetData("--x", CreateData("foo"));
  vars2.SetData("--x", CreateData("bar"));
  EXPECT_NE(vars1, vars2);
}

TEST_F(StyleVariablesTest, DifferentDataSize) {
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetData("--x", CreateData("foo"));
  vars2.SetData("--x", CreateData("bar"));
  vars2.SetData("--y", CreateData("foz"));
  EXPECT_NE(vars1, vars2);
}

// CSSValue

TEST_F(StyleVariablesTest, IsEmptyValue) {
  StyleVariables vars;
  EXPECT_TRUE(vars.IsEmpty());
  vars.SetValue("--x", CreateValue("foo"));
  EXPECT_FALSE(vars.IsEmpty());
}

TEST_F(StyleVariablesTest, SetValue) {
  StyleVariables vars;

  const CSSValue* foo = CreateValue("foo");
  const CSSValue* bar = CreateValue("bar");

  EXPECT_FALSE(vars.GetValue("--x").has_value());

  vars.SetValue("--x", foo);
  EXPECT_EQ(foo, vars.GetValue("--x").value_or(nullptr));

  vars.SetValue("--x", bar);
  EXPECT_EQ(bar, vars.GetValue("--x").value_or(nullptr));
}

TEST_F(StyleVariablesTest, SetNullValue) {
  StyleVariables vars;
  EXPECT_FALSE(vars.GetValue("--x").has_value());
  vars.SetValue("--x", nullptr);
  auto value = vars.GetValue("--x");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(nullptr, value.value());
}

TEST_F(StyleVariablesTest, SingleValueSamePointer) {
  const CSSValue* foo = CreateValue("foo");
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetValue("--x", foo);
  vars2.SetValue("--x", foo);
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, SingleValueSameContent) {
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetValue("--x", CreateValue("foo"));
  vars2.SetValue("--x", CreateValue("foo"));
  EXPECT_EQ(vars1, vars2);
}

TEST_F(StyleVariablesTest, SingleValueContentNotEqual) {
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetValue("--x", CreateValue("foo"));
  vars2.SetValue("--x", CreateValue("bar"));
  EXPECT_NE(vars1, vars2);
}

TEST_F(StyleVariablesTest, DifferentValueSize) {
  StyleVariables vars1;
  StyleVariables vars2;
  vars1.SetValue("--x", CreateValue("foo"));
  vars2.SetValue("--x", CreateValue("bar"));
  vars2.SetValue("--y", CreateValue("foz"));
  EXPECT_NE(vars1, vars2);
}

}  // namespace
}  // namespace blink
