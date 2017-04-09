// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/FilteredComputedStylePropertyMap.h"

#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/dom/Element.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class FilteredComputedStylePropertyMapTest : public ::testing::Test {
 public:
  FilteredComputedStylePropertyMapTest() : page_(DummyPageHolder::Create()) {
    declaration_ = CSSComputedStyleDeclaration::Create(
        page_->GetDocument().documentElement());
  }

  CSSComputedStyleDeclaration* Declaration() const {
    return declaration_.Get();
  }
  Node* PageNode() { return page_->GetDocument().documentElement(); }

 private:
  std::unique_ptr<DummyPageHolder> page_;
  Persistent<CSSComputedStyleDeclaration> declaration_;
};

TEST_F(FilteredComputedStylePropertyMapTest, GetProperties) {
  Vector<CSSPropertyID> native_properties(
      {CSSPropertyColor, CSSPropertyAlignItems});
  Vector<CSSPropertyID> empty_native_properties;
  Vector<AtomicString> custom_properties({"--foo", "--bar"});
  Vector<AtomicString> empty_custom_properties;

  FilteredComputedStylePropertyMap* map =
      FilteredComputedStylePropertyMap::Create(Declaration(), native_properties,
                                               custom_properties, PageNode());
  EXPECT_TRUE(map->getProperties().Contains("color"));
  EXPECT_TRUE(map->getProperties().Contains("align-items"));
  EXPECT_TRUE(map->getProperties().Contains("--foo"));
  EXPECT_TRUE(map->getProperties().Contains("--bar"));

  map = FilteredComputedStylePropertyMap::Create(
      Declaration(), native_properties, empty_custom_properties, PageNode());
  EXPECT_TRUE(map->getProperties().Contains("color"));
  EXPECT_TRUE(map->getProperties().Contains("align-items"));

  map = FilteredComputedStylePropertyMap::Create(
      Declaration(), empty_native_properties, custom_properties, PageNode());
  EXPECT_TRUE(map->getProperties().Contains("--foo"));
  EXPECT_TRUE(map->getProperties().Contains("--bar"));
}

TEST_F(FilteredComputedStylePropertyMapTest, NativePropertyAccessors) {
  Vector<CSSPropertyID> native_properties(
      {CSSPropertyColor, CSSPropertyAlignItems});
  Vector<AtomicString> empty_custom_properties;
  FilteredComputedStylePropertyMap* map =
      FilteredComputedStylePropertyMap::Create(Declaration(), native_properties,
                                               empty_custom_properties,
                                               PageNode());

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
      FilteredComputedStylePropertyMap::Create(Declaration(),
                                               empty_native_properties,
                                               custom_properties, PageNode());

  DummyExceptionStateForTesting exception_state;

  map->get("--foo", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  map->has("--foo", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  map->getAll("--foo", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  map->get("--quix", exception_state);
  EXPECT_TRUE(exception_state.HadException());
  exception_state.ClearException();

  map->has("--quix", exception_state);
  EXPECT_TRUE(exception_state.HadException());
  exception_state.ClearException();

  map->getAll("--quix", exception_state);
  EXPECT_TRUE(exception_state.HadException());
  exception_state.ClearException();
}

}  // namespace blink
