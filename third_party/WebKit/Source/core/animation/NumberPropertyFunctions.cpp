// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/NumberPropertyFunctions.h"

#include "core/style/ComputedStyle.h"

namespace blink {

bool NumberPropertyFunctions::GetInitialNumber(const CSSProperty& property,
                                               double& result) {
  return GetNumber(property, ComputedStyle::InitialStyle(), result);
}

bool NumberPropertyFunctions::GetNumber(const CSSProperty& property,
                                        const ComputedStyle& style,
                                        double& result) {
  switch (property.PropertyID()) {
    case CSSPropertyFillOpacity:
      result = style.FillOpacity();
      return true;
    case CSSPropertyFlexGrow:
      result = style.FlexGrow();
      return true;
    case CSSPropertyFlexShrink:
      result = style.FlexShrink();
      return true;
    case CSSPropertyFloodOpacity:
      result = style.FloodOpacity();
      return true;
    case CSSPropertyOpacity:
      result = style.Opacity();
      return true;
    case CSSPropertyOrder:
      result = style.Order();
      return true;
    case CSSPropertyOrphans:
      result = style.Orphans();
      return true;
    case CSSPropertyShapeImageThreshold:
      result = style.ShapeImageThreshold();
      return true;
    case CSSPropertyStopOpacity:
      result = style.StopOpacity();
      return true;
    case CSSPropertyStrokeMiterlimit:
      result = style.StrokeMiterLimit();
      return true;
    case CSSPropertyStrokeOpacity:
      result = style.StrokeOpacity();
      return true;
    case CSSPropertyWidows:
      result = style.Widows();
      return true;

    case CSSPropertyFontSizeAdjust:
      if (!style.HasFontSizeAdjust())
        return false;
      result = style.FontSizeAdjust();
      return true;
    case CSSPropertyColumnCount:
      if (style.HasAutoColumnCount())
        return false;
      result = style.ColumnCount();
      return true;
    case CSSPropertyZIndex:
      if (style.HasAutoZIndex())
        return false;
      result = style.ZIndex();
      return true;

    case CSSPropertyLineHeight: {
      const Length& length = style.SpecifiedLineHeight();
      // Numbers are represented by percentages.
      if (length.GetType() != kPercent)
        return false;
      double value = length.Value();
      // -100% represents the keyword "normal".
      if (value == -100)
        return false;
      result = value / 100;
      return true;
    }

    default:
      return false;
  }
}

double NumberPropertyFunctions::ClampNumber(const CSSProperty& property,
                                            double value) {
  switch (property.PropertyID()) {
    case CSSPropertyStrokeMiterlimit:
      return clampTo<float>(value, 1);

    case CSSPropertyFloodOpacity:
    case CSSPropertyStopOpacity:
    case CSSPropertyStrokeOpacity:
    case CSSPropertyShapeImageThreshold:
      return clampTo<float>(value, 0, 1);

    case CSSPropertyFillOpacity:
    case CSSPropertyOpacity:
      return clampTo<float>(value, 0, nextafterf(1, 0));

    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
    case CSSPropertyFontSizeAdjust:
    case CSSPropertyLineHeight:
      return clampTo<float>(value, 0);

    case CSSPropertyOrphans:
    case CSSPropertyWidows:
      return clampTo<short>(round(value), 1);

    case CSSPropertyColumnCount:
      return clampTo<unsigned short>(round(value), 1);

    case CSSPropertyColumnRuleWidth:
      return clampTo<unsigned short>(round(value));

    case CSSPropertyOrder:
    case CSSPropertyZIndex:
      return clampTo<int>(round(value));

    default:
      NOTREACHED();
      return value;
  }
}

bool NumberPropertyFunctions::SetNumber(const CSSProperty& property,
                                        ComputedStyle& style,
                                        double value) {
  DCHECK_EQ(value, ClampNumber(property, value));
  switch (property.PropertyID()) {
    case CSSPropertyFillOpacity:
      style.SetFillOpacity(value);
      return true;
    case CSSPropertyFlexGrow:
      style.SetFlexGrow(value);
      return true;
    case CSSPropertyFlexShrink:
      style.SetFlexShrink(value);
      return true;
    case CSSPropertyFloodOpacity:
      style.SetFloodOpacity(value);
      return true;
    case CSSPropertyLineHeight:
      style.SetLineHeight(Length(value * 100, kPercent));
      return true;
    case CSSPropertyOpacity:
      style.SetOpacity(value);
      return true;
    case CSSPropertyOrder:
      style.SetOrder(value);
      return true;
    case CSSPropertyOrphans:
      style.SetOrphans(value);
      return true;
    case CSSPropertyShapeImageThreshold:
      style.SetShapeImageThreshold(value);
      return true;
    case CSSPropertyStopOpacity:
      style.SetStopOpacity(value);
      return true;
    case CSSPropertyStrokeMiterlimit:
      style.SetStrokeMiterLimit(value);
      return true;
    case CSSPropertyStrokeOpacity:
      style.SetStrokeOpacity(value);
      return true;
    case CSSPropertyColumnCount:
      style.SetColumnCount(value);
      return true;
    case CSSPropertyWidows:
      style.SetWidows(value);
      return true;
    case CSSPropertyZIndex:
      style.SetZIndex(value);
      return true;
    default:
      return false;
  }
}

}  // namespace blink
