// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/WebkitAppearance.h"

#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* WebkitAppearance::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.Appearance());
}

}  // namespace CSSLonghand
}  // namespace blink
