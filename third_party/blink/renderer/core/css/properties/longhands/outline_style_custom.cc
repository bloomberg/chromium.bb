// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/outline_style.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* OutlineStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  if (style.OutlineStyleIsAuto())
    return CSSIdentifierValue::Create(CSSValueAuto);
  return CSSIdentifierValue::Create(style.OutlineStyle());
}

void OutlineStyle::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetOutlineStyleIsAuto(
      ComputedStyleInitialValues::InitialOutlineStyleIsAuto());
  state.Style()->SetOutlineStyle(EBorderStyle::kNone);
}

void OutlineStyle::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetOutlineStyleIsAuto(
      state.ParentStyle()->OutlineStyleIsAuto());
  state.Style()->SetOutlineStyle(state.ParentStyle()->OutlineStyle());
}

void OutlineStyle::ApplyValue(StyleResolverState& state,
                              const CSSValue& value) const {
  const CSSIdentifierValue& identifier_value = ToCSSIdentifierValue(value);
  state.Style()->SetOutlineStyleIsAuto(
      static_cast<bool>(identifier_value.ConvertTo<OutlineIsAuto>()));
  state.Style()->SetOutlineStyle(identifier_value.ConvertTo<EBorderStyle>());
}

}  // namespace CSSLonghand
}  // namespace blink
