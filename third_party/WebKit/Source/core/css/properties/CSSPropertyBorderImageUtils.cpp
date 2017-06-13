// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyBorderImageUtils.h"

#include "core/CSSValueKeywords.h"
#include "core/css/CSSBorderImage.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/Length.h"

namespace blink {

namespace {

static CSSIdentifierValue* ConsumeBorderImageRepeatKeyword(
    CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::ConsumeIdent<CSSValueStretch, CSSValueRepeat,
                                                CSSValueSpace, CSSValueRound>(
      range);
}

}  // namespace

CSSValue* CSSPropertyBorderImageUtils::ConsumeWebkitBorderImage(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSValue* source = nullptr;
  CSSValue* slice = nullptr;
  CSSValue* width = nullptr;
  CSSValue* outset = nullptr;
  CSSValue* repeat = nullptr;
  if (ConsumeBorderImageComponents(range, context, source, slice, width, outset,
                                   repeat, true /* default_fill */))
    return CreateBorderImageValue(source, slice, width, outset, repeat);
  return nullptr;
}

bool CSSPropertyBorderImageUtils::ConsumeBorderImageComponents(
    CSSParserTokenRange& range,
    const CSSParserContext* context,
    CSSValue*& source,
    CSSValue*& slice,
    CSSValue*& width,
    CSSValue*& outset,
    CSSValue*& repeat,
    bool default_fill) {
  do {
    if (!source) {
      source = CSSPropertyParserHelpers::ConsumeImageOrNone(range, context);
      if (source)
        continue;
    }
    if (!repeat) {
      repeat = ConsumeBorderImageRepeat(range);
      if (repeat)
        continue;
    }
    if (!slice) {
      slice = ConsumeBorderImageSlice(range, default_fill);
      if (slice) {
        DCHECK(!width);
        DCHECK(!outset);
        if (CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range)) {
          width = ConsumeBorderImageWidth(range);
          if (CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(
                  range)) {
            outset = ConsumeBorderImageOutset(range);
            if (!outset)
              return false;
          } else if (!width) {
            return false;
          }
        }
      } else {
        return false;
      }
    } else {
      return false;
    }
  } while (!range.AtEnd());
  return true;
}

CSSValue* CSSPropertyBorderImageUtils::ConsumeBorderImageRepeat(
    CSSParserTokenRange& range) {
  CSSIdentifierValue* horizontal = ConsumeBorderImageRepeatKeyword(range);
  if (!horizontal)
    return nullptr;
  CSSIdentifierValue* vertical = ConsumeBorderImageRepeatKeyword(range);
  if (!vertical)
    vertical = horizontal;
  return CSSValuePair::Create(horizontal, vertical,
                              CSSValuePair::kDropIdenticalValues);
}

CSSValue* CSSPropertyBorderImageUtils::ConsumeBorderImageSlice(
    CSSParserTokenRange& range,
    bool default_fill) {
  bool fill = CSSPropertyParserHelpers::ConsumeIdent<CSSValueFill>(range);
  CSSValue* slices[4] = {0};

  for (size_t index = 0; index < 4; ++index) {
    CSSPrimitiveValue* value =
        CSSPropertyParserHelpers::ConsumePercent(range, kValueRangeNonNegative);
    if (!value) {
      value = CSSPropertyParserHelpers::ConsumeNumber(range,
                                                      kValueRangeNonNegative);
    }
    if (!value)
      break;
    slices[index] = value;
  }
  if (!slices[0])
    return nullptr;
  if (CSSPropertyParserHelpers::ConsumeIdent<CSSValueFill>(range)) {
    if (fill)
      return nullptr;
    fill = true;
  }
  CSSPropertyParserHelpers::Complete4Sides(slices);
  if (default_fill)
    fill = true;
  return CSSBorderImageSliceValue::Create(
      CSSQuadValue::Create(slices[0], slices[1], slices[2], slices[3],
                           CSSQuadValue::kSerializeAsQuad),
      fill);
}

CSSValue* CSSPropertyBorderImageUtils::ConsumeBorderImageWidth(
    CSSParserTokenRange& range) {
  CSSValue* widths[4] = {0};

  CSSValue* value = nullptr;
  for (size_t index = 0; index < 4; ++index) {
    value =
        CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
    if (!value) {
      value = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
          range, kHTMLStandardMode, kValueRangeNonNegative,
          CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
    }
    if (!value)
      value = CSSPropertyParserHelpers::ConsumeIdent<CSSValueAuto>(range);
    if (!value)
      break;
    widths[index] = value;
  }
  if (!widths[0])
    return nullptr;
  CSSPropertyParserHelpers::Complete4Sides(widths);
  return CSSQuadValue::Create(widths[0], widths[1], widths[2], widths[3],
                              CSSQuadValue::kSerializeAsQuad);
}

CSSValue* CSSPropertyBorderImageUtils::ConsumeBorderImageOutset(
    CSSParserTokenRange& range) {
  CSSValue* outsets[4] = {0};

  CSSValue* value = nullptr;
  for (size_t index = 0; index < 4; ++index) {
    value =
        CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
    if (!value) {
      value = CSSPropertyParserHelpers::ConsumeLength(range, kHTMLStandardMode,
                                                      kValueRangeNonNegative);
    }
    if (!value)
      break;
    outsets[index] = value;
  }
  if (!outsets[0])
    return nullptr;
  CSSPropertyParserHelpers::Complete4Sides(outsets);
  return CSSQuadValue::Create(outsets[0], outsets[1], outsets[2], outsets[3],
                              CSSQuadValue::kSerializeAsQuad);
}

}  // namespace blink
