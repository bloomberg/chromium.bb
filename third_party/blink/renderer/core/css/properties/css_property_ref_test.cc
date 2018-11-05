// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/property_descriptor.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

namespace {

class CSSPropertyRefTest : public PageTestBase {
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

TEST_F(CSSPropertyRefTest, LookupUnregistred) {
  CSSPropertyRef ref("--x", GetDocument());
  EXPECT_TRUE(ref.IsValid());
  EXPECT_EQ(ref.GetProperty().PropertyID(), CSSPropertyVariable);
}

TEST_F(CSSPropertyRefTest, LookupRegistered) {
  RegisterProperty("--x", "<length>", "42px", false);
  CSSPropertyRef ref("--x", GetDocument());
  EXPECT_TRUE(ref.IsValid());
  EXPECT_EQ(ref.GetProperty().PropertyID(), CSSPropertyVariable);
}

TEST_F(CSSPropertyRefTest, LookupStandard) {
  CSSPropertyRef ref("font-size", GetDocument());
  EXPECT_TRUE(ref.IsValid());
  EXPECT_EQ(ref.GetProperty().PropertyID(), CSSPropertyFontSize);
}

TEST_F(CSSPropertyRefTest, IsValid) {
  CSSPropertyRef ref("nosuchproperty", GetDocument());
  EXPECT_FALSE(ref.IsValid());
}

TEST_F(CSSPropertyRefTest, FromCustomProperty) {
  CustomProperty custom(AtomicString("--x"), GetDocument());
  CSSPropertyRef ref(custom);
  EXPECT_TRUE(ref.IsValid());
  EXPECT_EQ(ref.GetProperty().PropertyID(), CSSPropertyVariable);
}

TEST_F(CSSPropertyRefTest, FromStandardProperty) {
  CSSPropertyRef ref(GetCSSPropertyFontSize());
  EXPECT_TRUE(ref.IsValid());
  EXPECT_EQ(ref.GetProperty().PropertyID(), CSSPropertyFontSize);
}

TEST_F(CSSPropertyRefTest, FromStaticVariableInstance) {
  CSSPropertyRef ref(GetCSSPropertyVariable());
  EXPECT_FALSE(ref.IsValid());
}

}  // namespace blink
