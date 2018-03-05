// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/OutlineStyle.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/style/ComputedStyle.h"

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

}  // namespace CSSLonghand
}  // namespace blink
