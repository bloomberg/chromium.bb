// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/text_align.h"

namespace blink {
namespace css_longhand {

const CSSValue* TextAlign::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetTextAlign());
}

void TextAlign::ApplyValue(StyleResolverState& state,
                           const CSSValue& value) const {
  const auto* ident_value = DynamicTo<CSSIdentifierValue>(value);
  if (ident_value && ident_value->GetValueID() != CSSValueWebkitMatchParent) {
    // Special case for th elements - UA stylesheet text-align does not apply if
    // parent's computed value for text-align is not its initial value
    // https://html.spec.whatwg.org/C/#tables-2
    if (ident_value->GetValueID() == CSSValueInternalCenter &&
        state.ParentStyle()->GetTextAlign() !=
            ComputedStyleInitialValues::InitialTextAlign())
      state.Style()->SetTextAlign(state.ParentStyle()->GetTextAlign());
    else
      state.Style()->SetTextAlign(ident_value->ConvertTo<ETextAlign>());
  } else if (state.ParentStyle()->GetTextAlign() == ETextAlign::kStart) {
    state.Style()->SetTextAlign(state.ParentStyle()->IsLeftToRightDirection()
                                    ? ETextAlign::kLeft
                                    : ETextAlign::kRight);
  } else if (state.ParentStyle()->GetTextAlign() == ETextAlign::kEnd) {
    state.Style()->SetTextAlign(state.ParentStyle()->IsLeftToRightDirection()
                                    ? ETextAlign::kRight
                                    : ETextAlign::kLeft);
  } else {
    state.Style()->SetTextAlign(state.ParentStyle()->GetTextAlign());
  }
}

}  // namespace css_longhand
}  // namespace blink
