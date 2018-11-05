// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/property_descriptor.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

namespace {

class CustomPropertyTest : public PageTestBase {
 public:
  void RegisterProperty(const String& name,
                        const String& syntax,
                        const String& initial_value,
                        bool is_inherited) {
    DummyExceptionStateForTesting exception_state;
    PropertyDescriptor* property_descriptor = PropertyDescriptor::Create();
    property_descriptor->setName(name);
    property_descriptor->setSyntax(syntax);
    property_descriptor->setInitialValue(initial_value);
    property_descriptor->setInherits(is_inherited);
    PropertyRegistration::registerProperty(&GetDocument(), property_descriptor,
                                           exception_state);
    EXPECT_FALSE(exception_state.HadException());
  }
};

}  // namespace

TEST_F(CustomPropertyTest, UnregisteredPropertyIsInherited) {
  CustomProperty property("--x", GetDocument());
  EXPECT_TRUE(property.IsInherited());
}

TEST_F(CustomPropertyTest, RegisteredNonInheritedPropertyIsNotInherited) {
  RegisterProperty("--x", "<length>", "42px", false);
  CustomProperty property("--x", GetDocument());
  EXPECT_FALSE(property.IsInherited());
}

TEST_F(CustomPropertyTest, RegisteredInheritedPropertyIsInherited) {
  RegisterProperty("--x", "<length>", "42px", true);
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

}  // namespace blink
