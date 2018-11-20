// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/overscroll_behavior_y.h"

namespace blink {
namespace css_longhand {

const CSSValue* OverscrollBehaviorY::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.OverscrollBehaviorY());
}

}  // namespace css_longhand
}  // namespace blink
