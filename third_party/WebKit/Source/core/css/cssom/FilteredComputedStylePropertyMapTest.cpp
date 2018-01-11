// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/FilteredComputedStylePropertyMap.h"

#include <memory>
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/dom/Element.h"
#include "core/testing/PageTestBase.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FilteredComputedStylePropertyMapTest : public PageTestBase {
 public:
  FilteredComputedStylePropertyMapTest() = default;

  CSSComputedStyleDeclaration* Declaration() const {
    return declaration_.Get();
  }

  void SetUp() override {
    PageTestBase::SetUp(IntSize());
    declaration_ =
        CSSComputedStyleDeclaration::Create(GetDocument().documentElement());
  }

  Node* PageNode() { return GetDocument().documentElement(); }

 private:
  Persistent<CSSComputedStyleDeclaration> declaration_;
};

TEST_F(FilteredComputedStylePropertyMapTest, GetProperties) {
  GetDocument().documentElement()->SetInnerHTMLFromString(
      "<div id='test' style='--foo: 1; --bar: banana'></div>");
  Element* test_element = GetDocument().getElementById("test");

  Vector<CSSPropertyID> native_properties(
      {CSSPropertyColor, CSSPropertyAlignItems});
  Vector<CSSPropertyID> empty_native_properties;
  Vector<AtomicString> custom_properties({"--foo", "--bar"});
  Vector<AtomicString> empty_custom_properties;

  FilteredComputedStylePropertyMap* map =
      FilteredComputedStylePropertyMap::Create(test_element, native_properties,
                                               custom_properties);
  EXPECT_TRUE(map->getProperties().Contains("color"));
  EXPECT_TRUE(map->getProperties().Contains("align-items"));
  EXPECT_TRUE(map->getProperties().Contains("--foo"));
  EXPECT_TRUE(map->getProperties().Contains("--bar"));

  map = FilteredComputedStylePropertyMap::Create(
      test_element, native_properties, empty_custom_properties);
  EXPECT_TRUE(map->getProperties().Contains("color"));
  EXPECT_TRUE(map->getProperties().Contains("align-items"));
  EXPECT_FALSE(map->getProperties().Contains("--foo"));
  EXPECT_FALSE(map->getProperties().Contains("--bar"));

  map = FilteredComputedStylePropertyMap::Create(
      test_element, empty_native_properties, custom_properties);
  EXPECT_FALSE(map->getProperties().Contains("color"));
  EXPECT_FALSE(map->getProperties().Contains("align-items"));
  EXPECT_TRUE(map->getProperties().Contains("--foo"));
  EXPECT_TRUE(map->getProperties().Contains("--bar"));
}

TEST_F(FilteredComputedStylePropertyMapTest, NativePropertyAccessors) {
  Vector<CSSPropertyID> native_properties(
      {CSSPropertyColor, CSSPropertyAlignItems});
  Vector<AtomicString> empty_custom_properties;
  FilteredComputedStylePropertyMap* map =
      FilteredComputedStylePropertyMap::Create(PageNode(), native_properties,
                                               empty_custom_properties);

  DummyExceptionStateForTesting exception_state;

  map->get("color", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  map->has("color", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  map->getAll("color", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  map->get("align-contents", exception_state);
  EXPECT_TRUE(exception_state.HadException());
  exception_state.ClearException();

  map->has("align-contents", exception_state);
  EXPECT_TRUE(exception_state.HadException());
  exception_state.ClearException();

  map->getAll("align-contents", exception_state);
  EXPECT_TRUE(exception_state.HadException());
  exception_state.ClearException();
}

TEST_F(FilteredComputedStylePropertyMapTest, CustomPropertyAccessors) {
  Vector<CSSPropertyID> empty_native_properties;
  Vector<AtomicString> custom_properties({"--foo", "--bar"});
  FilteredComputedStylePropertyMap* map =
      FilteredComputedStylePropertyMap::Create(
          PageNode(), empty_native_properties, custom_properties);

  DummyExceptionStateForTesting exception_state;

  EXPECT_EQ(nullptr, map->get("--foo", exception_state));
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(false, map->has("--foo", exception_state));
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(CSSStyleValueVector(), map->getAll("--foo", exception_state));
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(nullptr, map->get("--quix", exception_state));
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(false, map->has("--quix", exception_state));
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(CSSStyleValueVector(), map->getAll("--quix", exception_state));
  EXPECT_FALSE(exception_state.HadException());
}

}  // namespace blink
