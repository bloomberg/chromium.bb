// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/clear.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* Clear::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.Clear());
}

void Clear::ApplyValue(StyleResolverState& state, const CSSValue& value) const {
  const CSSIdentifierValue& identifier_value = ToCSSIdentifierValue(value);

  EClear c;
  CSSValueID id = identifier_value.GetValueID();
  switch (id) {
    case CSSValueInlineStart:
    case CSSValueInlineEnd:
      if ((id == CSSValueInlineStart) ==
          (state.Style()->Direction() == TextDirection::kLtr)) {
        c = EClear::kLeft;
      } else {
        c = EClear::kRight;
      }
      break;
    default:
      c = identifier_value.ConvertTo<EClear>();
      break;
  }
  state.Style()->SetClear(c);
}

}  // namespace CSSLonghand
}  // namespace blink
