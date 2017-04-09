// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/PaintPropertyFunctions.h"

#include "core/css/StyleColor.h"
#include "core/style/ComputedStyle.h"

namespace blink {

bool PaintPropertyFunctions::GetInitialColor(CSSPropertyID property,
                                             StyleColor& result) {
  return GetColor(property, ComputedStyle::InitialStyle(), result);
}

static bool GetColorFromPaint(const SVGPaintType type,
                              const Color color,
                              StyleColor& result) {
  switch (type) {
    case SVG_PAINTTYPE_RGBCOLOR:
      result = color;
      return true;
    case SVG_PAINTTYPE_CURRENTCOLOR:
      result = StyleColor::CurrentColor();
      return true;
    default:
      return false;
  }
}

bool PaintPropertyFunctions::GetColor(CSSPropertyID property,
                                      const ComputedStyle& style,
                                      StyleColor& result) {
  switch (property) {
    case CSSPropertyFill:
      return GetColorFromPaint(style.SvgStyle().FillPaintType(),
                               style.SvgStyle().FillPaintColor(), result);
    case CSSPropertyStroke:
      return GetColorFromPaint(style.SvgStyle().StrokePaintType(),
                               style.SvgStyle().StrokePaintColor(), result);
    default:
      NOTREACHED();
      return false;
  }
}

void PaintPropertyFunctions::SetColor(CSSPropertyID property,
                                      ComputedStyle& style,
                                      const Color& color) {
  switch (property) {
    case CSSPropertyFill:
      style.AccessSVGStyle().SetFillPaint(SVG_PAINTTYPE_RGBCOLOR, color,
                                          String(), true, true);
      break;
    case CSSPropertyStroke:
      style.AccessSVGStyle().SetStrokePaint(SVG_PAINTTYPE_RGBCOLOR, color,
                                            String(), true, true);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace blink
