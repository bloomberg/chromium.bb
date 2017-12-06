// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/ComputedStyleUtils.h"

#include "core/css/CSSColorValue.h"
#include "core/css/CSSValue.h"
#include "core/css/StyleColor.h"

namespace blink {

const CSSValue* ComputedStyleUtils::CurrentColorOrValidColor(
    const ComputedStyle& style,
    const StyleColor& color) {
  // This function does NOT look at visited information, so that computed style
  // doesn't expose that.
  return cssvalue::CSSColorValue::Create(color.Resolve(style.GetColor()).Rgb());
}

const blink::Color ComputedStyleUtils::BorderSideColor(
    const ComputedStyle& style,
    const StyleColor& color,
    EBorderStyle border_style,
    bool visited_link) {
  if (!color.IsCurrentColor())
    return color.GetColor();
  // FIXME: Treating styled borders with initial color differently causes
  // problems, see crbug.com/316559, crbug.com/276231
  if (!visited_link && (border_style == EBorderStyle::kInset ||
                        border_style == EBorderStyle::kOutset ||
                        border_style == EBorderStyle::kRidge ||
                        border_style == EBorderStyle::kGroove))
    return blink::Color(238, 238, 238);
  return visited_link ? style.VisitedLinkColor() : style.GetColor();
}

}  // namespace blink
