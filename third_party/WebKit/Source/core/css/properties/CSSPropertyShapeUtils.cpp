// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyShapeUtils.h"

#include "core/css/CSSBasicShapeValues.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/frame/WebFeature.h"

namespace blink {

using namespace CSSPropertyParserHelpers;

namespace {

static CSSValue* ConsumeShapeRadius(CSSParserTokenRange& args,
                                    CSSParserMode css_parser_mode) {
  if (IdentMatches<CSSValueClosestSide, CSSValueFarthestSide>(args.Peek().Id()))
    return ConsumeIdent(args);
  return ConsumeLengthOrPercent(args, css_parser_mode, kValueRangeNonNegative);
}

static CSSBasicShapeCircleValue* ConsumeBasicShapeCircle(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
  // circle( [<shape-radius>]? [at <position>]? )
  CSSBasicShapeCircleValue* shape = CSSBasicShapeCircleValue::Create();
  if (CSSValue* radius = ConsumeShapeRadius(args, context.Mode()))
    shape->SetRadius(radius);
  if (ConsumeIdent<CSSValueAt>(args)) {
    CSSValue* center_x = nullptr;
    CSSValue* center_y = nullptr;
    if (!ConsumePosition(args, context, UnitlessQuirk::kForbid,
                         WebFeature::kThreeValuedPositionBasicShape, center_x,
                         center_y))
      return nullptr;
    shape->SetCenterX(center_x);
    shape->SetCenterY(center_y);
  }
  return shape;
}

static CSSBasicShapeEllipseValue* ConsumeBasicShapeEllipse(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
  // ellipse( [<shape-radius>{2}]? [at <position>]? )
  CSSBasicShapeEllipseValue* shape = CSSBasicShapeEllipseValue::Create();
  if (CSSValue* radius_x = ConsumeShapeRadius(args, context.Mode())) {
    shape->SetRadiusX(radius_x);
    if (CSSValue* radius_y = ConsumeShapeRadius(args, context.Mode()))
      shape->SetRadiusY(radius_y);
  }
  if (ConsumeIdent<CSSValueAt>(args)) {
    CSSValue* center_x = nullptr;
    CSSValue* center_y = nullptr;
    if (!ConsumePosition(args, context, UnitlessQuirk::kForbid,
                         WebFeature::kThreeValuedPositionBasicShape, center_x,
                         center_y))
      return nullptr;
    shape->SetCenterX(center_x);
    shape->SetCenterY(center_y);
  }
  return shape;
}

static CSSBasicShapePolygonValue* ConsumeBasicShapePolygon(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  CSSBasicShapePolygonValue* shape = CSSBasicShapePolygonValue::Create();
  if (IdentMatches<CSSValueEvenodd, CSSValueNonzero>(args.Peek().Id())) {
    shape->SetWindRule(args.ConsumeIncludingWhitespace().Id() == CSSValueEvenodd
                           ? RULE_EVENODD
                           : RULE_NONZERO);
    if (!ConsumeCommaIncludingWhitespace(args))
      return nullptr;
  }

  do {
    CSSPrimitiveValue* x_length =
        ConsumeLengthOrPercent(args, context.Mode(), kValueRangeAll);
    if (!x_length)
      return nullptr;
    CSSPrimitiveValue* y_length =
        ConsumeLengthOrPercent(args, context.Mode(), kValueRangeAll);
    if (!y_length)
      return nullptr;
    shape->AppendPoint(x_length, y_length);
  } while (ConsumeCommaIncludingWhitespace(args));
  return shape;
}

static CSSBasicShapeInsetValue* ConsumeBasicShapeInset(
    CSSParserTokenRange& args,
    const CSSParserContext& context) {
  CSSBasicShapeInsetValue* shape = CSSBasicShapeInsetValue::Create();
  CSSPrimitiveValue* top =
      ConsumeLengthOrPercent(args, context.Mode(), kValueRangeAll);
  if (!top)
    return nullptr;
  CSSPrimitiveValue* right =
      ConsumeLengthOrPercent(args, context.Mode(), kValueRangeAll);
  CSSPrimitiveValue* bottom = nullptr;
  CSSPrimitiveValue* left = nullptr;
  if (right) {
    bottom = ConsumeLengthOrPercent(args, context.Mode(), kValueRangeAll);
    if (bottom)
      left = ConsumeLengthOrPercent(args, context.Mode(), kValueRangeAll);
  }
  if (left)
    shape->UpdateShapeSize4Values(top, right, bottom, left);
  else if (bottom)
    shape->UpdateShapeSize3Values(top, right, bottom);
  else if (right)
    shape->UpdateShapeSize2Values(top, right);
  else
    shape->UpdateShapeSize1Value(top);

  if (ConsumeIdent<CSSValueRound>(args)) {
    CSSValue* horizontal_radii[4] = {nullptr};
    CSSValue* vertical_radii[4] = {nullptr};
    if (!CSSPropertyShapeUtils::ConsumeRadii(horizontal_radii, vertical_radii,
                                             args, context.Mode(), false))
      return nullptr;
    shape->SetTopLeftRadius(
        CSSValuePair::Create(horizontal_radii[0], vertical_radii[0],
                             CSSValuePair::kDropIdenticalValues));
    shape->SetTopRightRadius(
        CSSValuePair::Create(horizontal_radii[1], vertical_radii[1],
                             CSSValuePair::kDropIdenticalValues));
    shape->SetBottomRightRadius(
        CSSValuePair::Create(horizontal_radii[2], vertical_radii[2],
                             CSSValuePair::kDropIdenticalValues));
    shape->SetBottomLeftRadius(
        CSSValuePair::Create(horizontal_radii[3], vertical_radii[3],
                             CSSValuePair::kDropIdenticalValues));
  }
  return shape;
}

}  // namespace

bool CSSPropertyShapeUtils::ConsumeRadii(CSSValue* horizontal_radii[4],
                                         CSSValue* vertical_radii[4],
                                         CSSParserTokenRange& range,
                                         CSSParserMode css_parser_mode,
                                         bool use_legacy_parsing) {
  unsigned i = 0;
  for (; i < 4 && !range.AtEnd() && range.Peek().GetType() != kDelimiterToken;
       ++i) {
    horizontal_radii[i] =
        ConsumeLengthOrPercent(range, css_parser_mode, kValueRangeNonNegative);
    if (!horizontal_radii[i])
      return false;
  }
  if (!horizontal_radii[0])
    return false;
  if (range.AtEnd()) {
    // Legacy syntax: -webkit-border-radius: l1 l2; is equivalent to
    // border-radius: l1 / l2;
    if (use_legacy_parsing && i == 2) {
      vertical_radii[0] = horizontal_radii[1];
      horizontal_radii[1] = nullptr;
    } else {
      Complete4Sides(horizontal_radii);
      for (unsigned i = 0; i < 4; ++i)
        vertical_radii[i] = horizontal_radii[i];
      return true;
    }
  } else {
    if (!ConsumeSlashIncludingWhitespace(range))
      return false;
    for (i = 0; i < 4 && !range.AtEnd(); ++i) {
      vertical_radii[i] = ConsumeLengthOrPercent(range, css_parser_mode,
                                                 kValueRangeNonNegative);
      if (!vertical_radii[i])
        return false;
    }
    if (!vertical_radii[0] || !range.AtEnd())
      return false;
  }
  Complete4Sides(horizontal_radii);
  Complete4Sides(vertical_radii);
  return true;
}

CSSValue* CSSPropertyShapeUtils::ConsumeBasicShape(
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  CSSValue* shape = nullptr;
  if (range.Peek().GetType() != kFunctionToken)
    return nullptr;
  CSSValueID id = range.Peek().FunctionId();
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);
  if (id == CSSValueCircle)
    shape = ConsumeBasicShapeCircle(args, context);
  else if (id == CSSValueEllipse)
    shape = ConsumeBasicShapeEllipse(args, context);
  else if (id == CSSValuePolygon)
    shape = ConsumeBasicShapePolygon(args, context);
  else if (id == CSSValueInset)
    shape = ConsumeBasicShapeInset(args, context);
  if (!shape || !args.AtEnd())
    return nullptr;
  range = range_copy;
  return shape;
}

}  // namespace blink
