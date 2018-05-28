// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/webkit_writing_mode.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* WebkitWritingMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.GetWritingMode());
}

void WebkitWritingMode::ApplyValue(StyleResolverState& state,
                                   const CSSValue& value) const {
  state.SetWritingMode(
      ToCSSIdentifierValue(value).ConvertTo<blink::WritingMode>());
}

}  // namespace CSSLonghand
}  // namespace blink
