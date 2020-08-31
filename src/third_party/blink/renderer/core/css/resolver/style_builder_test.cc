// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class StyleBuilderTest : public PageTestBase {};

TEST_F(StyleBuilderTest, WritingModeChangeDirtiesFont) {
  const CSSProperty* properties[] = {
      &GetCSSPropertyWritingMode(),
      &GetCSSPropertyWebkitWritingMode(),
  };

  HeapVector<Member<const CSSValue>> values = {
      CSSInitialValue::Create(),
      CSSInheritedValue::Create(),
      CSSIdentifierValue::Create(CSSValueID::kHorizontalTb),
  };

  for (const CSSProperty* property : properties) {
    for (const CSSValue* value : values) {
      auto parent_style = ComputedStyle::Create();
      auto style = ComputedStyle::Create();
      // This test assumes that initial 'writing-mode' is not 'vertical-lr'.
      ASSERT_NE(WritingMode::kVerticalLr, style->GetWritingMode());
      style->SetWritingMode(WritingMode::kVerticalLr);

      StyleResolverState state(GetDocument(), *GetDocument().body(),
                               parent_style.get(), parent_style.get());
      state.SetStyle(style);

      ASSERT_FALSE(state.GetFontBuilder().FontDirty());
      StyleBuilder::ApplyProperty(*property, state, *value);
      EXPECT_TRUE(state.GetFontBuilder().FontDirty());
    }
  }
}

TEST_F(StyleBuilderTest, TextOrientationChangeDirtiesFont) {
  HeapVector<Member<const CSSValue>> values = {
      CSSInitialValue::Create(),
      CSSInheritedValue::Create(),
      CSSIdentifierValue::Create(CSSValueID::kMixed),
  };

  for (const CSSValue* value : values) {
    auto parent_style = ComputedStyle::Create();
    auto style = ComputedStyle::Create();
    // This test assumes that initial 'text-orientation' is not 'upright'.
    ASSERT_NE(ETextOrientation::kUpright, style->GetTextOrientation());
    style->SetTextOrientation(ETextOrientation::kUpright);

    StyleResolverState state(GetDocument(), *GetDocument().body(),
                             parent_style.get(), parent_style.get());
    state.SetStyle(style);

    ASSERT_FALSE(state.GetFontBuilder().FontDirty());
    StyleBuilder::ApplyProperty(GetCSSPropertyTextOrientation(), state, *value);
    EXPECT_TRUE(state.GetFontBuilder().FontDirty());
  }
}

}  // namespace blink
