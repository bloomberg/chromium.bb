// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyShapeUtils.h"

#include "core/css/CSSBasicShapeValues.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

using namespace CSSPropertyParserHelpers;

namespace {

static CSSValue* consumeShapeRadius(CSSParserTokenRange& args,
                                    CSSParserMode cssParserMode) {
  if (identMatches<CSSValueClosestSide, CSSValueFarthestSide>(args.peek().id()))
    return consumeIdent(args);
  return consumeLengthOrPercent(args, cssParserMode, ValueRangeNonNegative);
}

static CSSBasicShapeCircleValue* consumeBasicShapeCircle(
    CSSParserTokenRange& args,
    const CSSParserContext* context) {
  // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
  // circle( [<shape-radius>]? [at <position>]? )
  CSSBasicShapeCircleValue* shape = CSSBasicShapeCircleValue::create();
  if (CSSValue* radius = consumeShapeRadius(args, context->mode()))
    shape->setRadius(radius);
  if (consumeIdent<CSSValueAt>(args)) {
    CSSValue* centerX = nullptr;
    CSSValue* centerY = nullptr;
    if (!consumePosition(args, context->mode(), UnitlessQuirk::Forbid, centerX,
                         centerY))
      return nullptr;
    shape->setCenterX(centerX);
    shape->setCenterY(centerY);
  }
  return shape;
}

static CSSBasicShapeEllipseValue* consumeBasicShapeEllipse(
    CSSParserTokenRange& args,
    const CSSParserContext* context) {
  // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
  // ellipse( [<shape-radius>{2}]? [at <position>]? )
  CSSBasicShapeEllipseValue* shape = CSSBasicShapeEllipseValue::create();
  if (CSSValue* radiusX = consumeShapeRadius(args, context->mode())) {
    shape->setRadiusX(radiusX);
    if (CSSValue* radiusY = consumeShapeRadius(args, context->mode()))
      shape->setRadiusY(radiusY);
  }
  if (consumeIdent<CSSValueAt>(args)) {
    CSSValue* centerX = nullptr;
    CSSValue* centerY = nullptr;
    if (!consumePosition(args, context->mode(), UnitlessQuirk::Forbid, centerX,
                         centerY))
      return nullptr;
    shape->setCenterX(centerX);
    shape->setCenterY(centerY);
  }
  return shape;
}

static CSSBasicShapePolygonValue* consumeBasicShapePolygon(
    CSSParserTokenRange& args,
    const CSSParserContext* context) {
  CSSBasicShapePolygonValue* shape = CSSBasicShapePolygonValue::create();
  if (identMatches<CSSValueEvenodd, CSSValueNonzero>(args.peek().id())) {
    shape->setWindRule(args.consumeIncludingWhitespace().id() == CSSValueEvenodd
                           ? RULE_EVENODD
                           : RULE_NONZERO);
    if (!consumeCommaIncludingWhitespace(args))
      return nullptr;
  }

  do {
    CSSPrimitiveValue* xLength =
        consumeLengthOrPercent(args, context->mode(), ValueRangeAll);
    if (!xLength)
      return nullptr;
    CSSPrimitiveValue* yLength =
        consumeLengthOrPercent(args, context->mode(), ValueRangeAll);
    if (!yLength)
      return nullptr;
    shape->appendPoint(xLength, yLength);
  } while (consumeCommaIncludingWhitespace(args));
  return shape;
}

static CSSBasicShapeInsetValue* consumeBasicShapeInset(
    CSSParserTokenRange& args,
    const CSSParserContext* context) {
  CSSBasicShapeInsetValue* shape = CSSBasicShapeInsetValue::create();
  CSSPrimitiveValue* top =
      consumeLengthOrPercent(args, context->mode(), ValueRangeAll);
  if (!top)
    return nullptr;
  CSSPrimitiveValue* right =
      consumeLengthOrPercent(args, context->mode(), ValueRangeAll);
  CSSPrimitiveValue* bottom = nullptr;
  CSSPrimitiveValue* left = nullptr;
  if (right) {
    bottom = consumeLengthOrPercent(args, context->mode(), ValueRangeAll);
    if (bottom)
      left = consumeLengthOrPercent(args, context->mode(), ValueRangeAll);
  }
  if (left)
    shape->updateShapeSize4Values(top, right, bottom, left);
  else if (bottom)
    shape->updateShapeSize3Values(top, right, bottom);
  else if (right)
    shape->updateShapeSize2Values(top, right);
  else
    shape->updateShapeSize1Value(top);

  if (consumeIdent<CSSValueRound>(args)) {
    CSSValue* horizontalRadii[4] = {0};
    CSSValue* verticalRadii[4] = {0};
    if (!CSSPropertyShapeUtils::consumeRadii(horizontalRadii, verticalRadii,
                                             args, context->mode(), false))
      return nullptr;
    shape->setTopLeftRadius(
        CSSValuePair::create(horizontalRadii[0], verticalRadii[0],
                             CSSValuePair::DropIdenticalValues));
    shape->setTopRightRadius(
        CSSValuePair::create(horizontalRadii[1], verticalRadii[1],
                             CSSValuePair::DropIdenticalValues));
    shape->setBottomRightRadius(
        CSSValuePair::create(horizontalRadii[2], verticalRadii[2],
                             CSSValuePair::DropIdenticalValues));
    shape->setBottomLeftRadius(
        CSSValuePair::create(horizontalRadii[3], verticalRadii[3],
                             CSSValuePair::DropIdenticalValues));
  }
  return shape;
}

}  // namespace

bool CSSPropertyShapeUtils::consumeRadii(CSSValue* horizontalRadii[4],
                                         CSSValue* verticalRadii[4],
                                         CSSParserTokenRange& range,
                                         CSSParserMode cssParserMode,
                                         bool useLegacyParsing) {
  unsigned i = 0;
  for (; i < 4 && !range.atEnd() && range.peek().type() != DelimiterToken;
       ++i) {
    horizontalRadii[i] =
        consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative);
    if (!horizontalRadii[i])
      return false;
  }
  if (!horizontalRadii[0])
    return false;
  if (range.atEnd()) {
    // Legacy syntax: -webkit-border-radius: l1 l2; is equivalent to
    // border-radius: l1 / l2;
    if (useLegacyParsing && i == 2) {
      verticalRadii[0] = horizontalRadii[1];
      horizontalRadii[1] = nullptr;
    } else {
      complete4Sides(horizontalRadii);
      for (unsigned i = 0; i < 4; ++i)
        verticalRadii[i] = horizontalRadii[i];
      return true;
    }
  } else {
    if (!consumeSlashIncludingWhitespace(range))
      return false;
    for (i = 0; i < 4 && !range.atEnd(); ++i) {
      verticalRadii[i] =
          consumeLengthOrPercent(range, cssParserMode, ValueRangeNonNegative);
      if (!verticalRadii[i])
        return false;
    }
    if (!verticalRadii[0] || !range.atEnd())
      return false;
  }
  complete4Sides(horizontalRadii);
  complete4Sides(verticalRadii);
  return true;
}

CSSValue* CSSPropertyShapeUtils::consumeBasicShape(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSValue* shape = nullptr;
  if (range.peek().type() != FunctionToken)
    return nullptr;
  CSSValueID id = range.peek().functionId();
  CSSParserTokenRange rangeCopy = range;
  CSSParserTokenRange args = consumeFunction(rangeCopy);
  if (id == CSSValueCircle)
    shape = consumeBasicShapeCircle(args, context);
  else if (id == CSSValueEllipse)
    shape = consumeBasicShapeEllipse(args, context);
  else if (id == CSSValuePolygon)
    shape = consumeBasicShapePolygon(args, context);
  else if (id == CSSValueInset)
    shape = consumeBasicShapeInset(args, context);
  if (!shape || !args.atEnd())
    return nullptr;
  range = rangeCopy;
  return shape;
}

}  // namespace blink
