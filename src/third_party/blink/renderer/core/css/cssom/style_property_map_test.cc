// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/style_property_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssstylevalue_string.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/inline_style_property_map.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class StylePropertyMapTest : public PageTestBase {};

TEST_F(StylePropertyMapTest, SetRevertWithFeatureEnabled) {
  DummyExceptionStateForTesting exception_state;

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  HeapVector<Member<V8UnionCSSStyleValueOrString>> revert_string;
  revert_string.push_back(
      MakeGarbageCollected<V8UnionCSSStyleValueOrString>(" revert"));

  HeapVector<Member<V8UnionCSSStyleValueOrString>> revert_style_value;
  revert_style_value.push_back(
      MakeGarbageCollected<V8UnionCSSStyleValueOrString>(
          CSSKeywordValue::Create("revert", exception_state)));
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  HeapVector<CSSStyleValueOrString> revert_string;
  revert_string.push_back(CSSStyleValueOrString::FromString(" revert"));

  HeapVector<CSSStyleValueOrString> revert_style_value;
  revert_style_value.push_back(CSSStyleValueOrString::FromCSSStyleValue(
      CSSKeywordValue::Create("revert", exception_state)));
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  auto* map =
      MakeGarbageCollected<InlineStylePropertyMap>(GetDocument().body());

  map->set(GetDocument().GetExecutionContext(), "top", revert_string,
           exception_state);
  map->set(GetDocument().GetExecutionContext(), "left", revert_style_value,
           exception_state);

  CSSStyleValue* top =
      map->get(GetDocument().GetExecutionContext(), "top", exception_state);
  CSSStyleValue* left =
      map->get(GetDocument().GetExecutionContext(), "left", exception_state);

  ASSERT_TRUE(DynamicTo<CSSKeywordValue>(top));
  EXPECT_EQ(CSSValueID::kRevert,
            DynamicTo<CSSKeywordValue>(top)->KeywordValueID());

  ASSERT_TRUE(DynamicTo<CSSKeywordValue>(left));
  EXPECT_EQ(CSSValueID::kRevert,
            DynamicTo<CSSKeywordValue>(top)->KeywordValueID());

  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(StylePropertyMapTest, SetOverflowClipString) {
  ScopedOverflowClipForTest overflow_clip_feature_enabler(true);

  DummyExceptionStateForTesting exception_state;

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  HeapVector<Member<V8UnionCSSStyleValueOrString>> clip_string;
  clip_string.push_back(
      MakeGarbageCollected<V8UnionCSSStyleValueOrString>(" clip"));
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  HeapVector<CSSStyleValueOrString> clip_string;
  clip_string.push_back(CSSStyleValueOrString::FromString(" clip"));
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  auto* map =
      MakeGarbageCollected<InlineStylePropertyMap>(GetDocument().body());

  map->set(GetDocument().GetExecutionContext(), "overflow-x", clip_string,
           exception_state);

  CSSStyleValue* overflow = map->get(GetDocument().GetExecutionContext(),
                                     "overflow-x", exception_state);
  ASSERT_TRUE(DynamicTo<CSSKeywordValue>(overflow));
  EXPECT_EQ(CSSValueID::kClip,
            DynamicTo<CSSKeywordValue>(overflow)->KeywordValueID());

  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(StylePropertyMapTest, SetOverflowClipStyleValue) {
  ScopedOverflowClipForTest overflow_clip_feature_enabler(true);

  DummyExceptionStateForTesting exception_state;

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  HeapVector<Member<V8UnionCSSStyleValueOrString>> clip_style_value;
  clip_style_value.push_back(MakeGarbageCollected<V8UnionCSSStyleValueOrString>(
      CSSKeywordValue::Create("clip", exception_state)));
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  HeapVector<CSSStyleValueOrString> clip_style_value;
  clip_style_value.push_back(CSSStyleValueOrString::FromCSSStyleValue(
      CSSKeywordValue::Create("clip", exception_state)));
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  auto* map =
      MakeGarbageCollected<InlineStylePropertyMap>(GetDocument().body());

  map->set(GetDocument().GetExecutionContext(), "overflow-x", clip_style_value,
           exception_state);

  CSSStyleValue* overflow = map->get(GetDocument().GetExecutionContext(),
                                     "overflow-x", exception_state);
  ASSERT_TRUE(DynamicTo<CSSKeywordValue>(overflow));
  EXPECT_EQ(CSSValueID::kClip,
            DynamicTo<CSSKeywordValue>(overflow)->KeywordValueID());

  EXPECT_FALSE(exception_state.HadException());
}

}  // namespace blink
