// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/TextDecorationStyle.h"

#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* TextDecorationStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForTextDecorationStyle(
      style.TextDecorationStyle());
}

}  // namespace CSSLonghand
}  // namespace blink
