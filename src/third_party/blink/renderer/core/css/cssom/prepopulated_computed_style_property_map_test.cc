// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/prepopulated_computed_style_property_map.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class PrepopulatedComputedStylePropertyMapTest : public PageTestBase {
 public:
  PrepopulatedComputedStylePropertyMapTest() = default;

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

TEST_F(PrepopulatedComputedStylePropertyMapTest, NativePropertyAccessors) {
  Vector<CSSPropertyID> native_properties(
      {CSSPropertyColor, CSSPropertyAlignItems});
  Vector<AtomicString> empty_custom_properties;

  GetDocument().View()->UpdateAllLifecyclePhases();
  Node* node = PageNode();

  PrepopulatedComputedStylePropertyMap* map =
      new PrepopulatedComputedStylePropertyMap(
          GetDocument(), node->ComputedStyleRef(), node, native_properties,
          empty_custom_properties);

  DummyExceptionStateForTesting exception_state;

  map->get(&GetDocument(), "color", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  map->has(&GetDocument(), "color", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  map->getAll(&GetDocument(), "color", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  map->get(&GetDocument(), "align-contents", exception_state);
  EXPECT_TRUE(exception_state.HadException());
  exception_state.ClearException();

  map->has(&GetDocument(), "align-contents", exception_state);
  EXPECT_TRUE(exception_state.HadException());
  exception_state.ClearException();

  map->getAll(&GetDocument(), "align-contents", exception_state);
  EXPECT_TRUE(exception_state.HadException());
  exception_state.ClearException();
}

TEST_F(PrepopulatedComputedStylePropertyMapTest, CustomPropertyAccessors) {
  Vector<CSSPropertyID> empty_native_properties;
  Vector<AtomicString> custom_properties({"--foo", "--bar"});

  GetDocument().View()->UpdateAllLifecyclePhases();
  Node* node = PageNode();

  PrepopulatedComputedStylePropertyMap* map =
      new PrepopulatedComputedStylePropertyMap(
          GetDocument(), node->ComputedStyleRef(), node,
          empty_native_properties, custom_properties);

  DummyExceptionStateForTesting exception_state;

  const CSSStyleValue* foo = map->get(&GetDocument(), "--foo", exception_state);
  ASSERT_NE(nullptr, foo);
  ASSERT_EQ(CSSStyleValue::kUnparsedType, foo->GetType());
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(true, map->has(&GetDocument(), "--foo", exception_state));
  EXPECT_FALSE(exception_state.HadException());

  CSSStyleValueVector fooAll =
      map->getAll(&GetDocument(), "--foo", exception_state);
  EXPECT_EQ(1U, fooAll.size());
  ASSERT_NE(nullptr, fooAll[0]);
  ASSERT_EQ(CSSStyleValue::kUnparsedType, fooAll[0]->GetType());
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(nullptr, map->get(&GetDocument(), "--quix", exception_state));
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(false, map->has(&GetDocument(), "--quix", exception_state));
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(CSSStyleValueVector(),
            map->getAll(&GetDocument(), "--quix", exception_state));
  EXPECT_FALSE(exception_state.HadException());
}

}  // namespace blink
