// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/webkit_margin_after_collapse.h"

namespace blink {
namespace css_longhand {

const CSSValue* WebkitMarginAfterCollapse::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.MarginAfterCollapse());
}

}  // namespace css_longhand
}  // namespace blink
