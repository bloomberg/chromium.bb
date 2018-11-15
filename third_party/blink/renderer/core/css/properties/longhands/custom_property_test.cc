// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

using namespace css_test_helpers;

namespace {

class CustomPropertyTest : public PageTestBase {
 public:
  void SetElementWithStyle(const String& value) {
    GetDocument().body()->SetInnerHTMLFromString("<div id='target' style='" +
                                                 value + "'></div>");
    UpdateAllLifecyclePhasesForTest();
  }

  const CSSValue* GetComputedValue(const CustomProperty& property) {
    Element* node = GetDocument().getElementById("target");
    return property.CSSValueFromComputedStyle(node->ComputedStyleRef(),
                                              nullptr /* layout_object*/, node,
                                              false /* allow_visisted_style */);
  }
};

}  // namespace

TEST_F(CustomPropertyTest, UnregisteredPropertyIsInherited) {
  CustomProperty property("--x", GetDocument());
  EXPECT_TRUE(property.IsInherited());
}

TEST_F(CustomPropertyTest, RegisteredNonInheritedPropertyIsNotInherited) {
  RegisterProperty(GetDocument(), "--x", "<length>", "42px", false);
  CustomProperty property("--x", GetDocument());
  EXPECT_FALSE(property.IsInherited());
}

TEST_F(CustomPropertyTest, RegisteredInheritedPropertyIsInherited) {
  RegisterProperty(GetDocument(), "--x", "<length>", "42px", true);
  CustomProperty property("--x", GetDocument());
  EXPECT_TRUE(property.IsInherited());
}

TEST_F(CustomPropertyTest, StaticVariableInstance) {
  CustomProperty property("--x", GetDocument());
  EXPECT_FALSE(Variable::IsStaticInstance(property));
  EXPECT_TRUE(Variable::IsStaticInstance(GetCSSPropertyVariable()));
}

TEST_F(CustomPropertyTest, PropertyID) {
  CustomProperty property("--x", GetDocument());
  EXPECT_EQ(CSSPropertyVariable, property.PropertyID());
}

TEST_F(CustomPropertyTest, GetPropertyNameAtomicString) {
  CustomProperty property("--x", GetDocument());
  EXPECT_EQ(AtomicString("--x"), property.GetPropertyNameAtomicString());
}

TEST_F(CustomPropertyTest, ComputedCSSValueUnregistered) {
  CustomProperty property("--x", GetDocument());
  SetElementWithStyle("--x:foo");
  const CSSValue* value = GetComputedValue(property);
  EXPECT_TRUE(value->IsCustomPropertyDeclaration());
  EXPECT_EQ("foo", value->CssText());
}

TEST_F(CustomPropertyTest, ComputedCSSValueInherited) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", true);
  CustomProperty property("--x", GetDocument());
  SetElementWithStyle("--x:100px");
  const CSSValue* value = GetComputedValue(property);
  ASSERT_TRUE(value->IsPrimitiveValue());
  const CSSPrimitiveValue* primitive_value = ToCSSPrimitiveValue(value);
  EXPECT_EQ(100, primitive_value->GetIntValue());
}

TEST_F(CustomPropertyTest, ComputedCSSValueNonInherited) {
  RegisterProperty(GetDocument(), "--x", "<length>", "0px", false);
  CustomProperty property("--x", GetDocument());
  SetElementWithStyle("--x:100px");
  const CSSValue* value = GetComputedValue(property);
  ASSERT_TRUE(value->IsPrimitiveValue());
  const CSSPrimitiveValue* primitive_value = ToCSSPrimitiveValue(value);
  EXPECT_EQ(100, primitive_value->GetIntValue());
}

TEST_F(CustomPropertyTest, ComputedCSSValueInitial) {
  RegisterProperty(GetDocument(), "--x", "<length>", "100px", false);
  CustomProperty property("--x", GetDocument());
  SetElementWithStyle("");  // Do not apply --x.
  const CSSValue* value = GetComputedValue(property);
  ASSERT_TRUE(value->IsPrimitiveValue());
  const CSSPrimitiveValue* primitive_value = ToCSSPrimitiveValue(value);
  EXPECT_EQ(100, primitive_value->GetIntValue());
}

TEST_F(CustomPropertyTest, ComputedCSSValueEmptyInitial) {
  CustomProperty property("--x", GetDocument());
  SetElementWithStyle("");  // Do not apply --x.
  const CSSValue* value = GetComputedValue(property);
  EXPECT_FALSE(value);
}

TEST_F(CustomPropertyTest, ComputedCSSValueLateRegistration) {
  CustomProperty property("--x", GetDocument());
  SetElementWithStyle("--x:100px");
  RegisterProperty(GetDocument(), "--x", "<length>", "100px", false);
  // The property was not registered when the style was computed, hence the
  // computed value should be what we expect for an unregistered property.
  const CSSValue* value = GetComputedValue(property);
  EXPECT_TRUE(value->IsCustomPropertyDeclaration());
  EXPECT_EQ("100px", value->CssText());
}

}  // namespace blink
