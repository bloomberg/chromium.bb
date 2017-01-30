/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/css/resolver/FilterOperationResolver.h"

#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSURIValue.h"
#include "core/css/resolver/StyleBuilderConverter.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/frame/UseCounter.h"

namespace blink {

static float kOffScreenCanvasEMFontSize = 10.0;
static float kOffScreenCanvasREMFontSize = 16.0;

FilterOperation::OperationType FilterOperationResolver::filterOperationForType(
    CSSValueID type) {
  switch (type) {
    case CSSValueGrayscale:
      return FilterOperation::GRAYSCALE;
    case CSSValueSepia:
      return FilterOperation::SEPIA;
    case CSSValueSaturate:
      return FilterOperation::SATURATE;
    case CSSValueHueRotate:
      return FilterOperation::HUE_ROTATE;
    case CSSValueInvert:
      return FilterOperation::INVERT;
    case CSSValueOpacity:
      return FilterOperation::OPACITY;
    case CSSValueBrightness:
      return FilterOperation::BRIGHTNESS;
    case CSSValueContrast:
      return FilterOperation::CONTRAST;
    case CSSValueBlur:
      return FilterOperation::BLUR;
    case CSSValueDropShadow:
      return FilterOperation::DROP_SHADOW;
    default:
      NOTREACHED();
      // FIXME: We shouldn't have a type None since we never create them
      return FilterOperation::NONE;
  }
}

static void countFilterUse(FilterOperation::OperationType operationType,
                           const Document& document) {
  // This variable is always reassigned, but MSVC thinks it might be left
  // uninitialized.
  UseCounter::Feature feature = UseCounter::NumberOfFeatures;
  switch (operationType) {
    case FilterOperation::NONE:
    case FilterOperation::BOX_REFLECT:
      NOTREACHED();
      return;
    case FilterOperation::REFERENCE:
      feature = UseCounter::CSSFilterReference;
      break;
    case FilterOperation::GRAYSCALE:
      feature = UseCounter::CSSFilterGrayscale;
      break;
    case FilterOperation::SEPIA:
      feature = UseCounter::CSSFilterSepia;
      break;
    case FilterOperation::SATURATE:
      feature = UseCounter::CSSFilterSaturate;
      break;
    case FilterOperation::HUE_ROTATE:
      feature = UseCounter::CSSFilterHueRotate;
      break;
    case FilterOperation::INVERT:
      feature = UseCounter::CSSFilterInvert;
      break;
    case FilterOperation::OPACITY:
      feature = UseCounter::CSSFilterOpacity;
      break;
    case FilterOperation::BRIGHTNESS:
      feature = UseCounter::CSSFilterBrightness;
      break;
    case FilterOperation::CONTRAST:
      feature = UseCounter::CSSFilterContrast;
      break;
    case FilterOperation::BLUR:
      feature = UseCounter::CSSFilterBlur;
      break;
    case FilterOperation::DROP_SHADOW:
      feature = UseCounter::CSSFilterDropShadow;
      break;
  };
  UseCounter::count(document, feature);
}

FilterOperations FilterOperationResolver::createFilterOperations(
    StyleResolverState& state,
    const CSSValue& inValue) {
  FilterOperations operations;

  if (inValue.isIdentifierValue()) {
    DCHECK_EQ(toCSSIdentifierValue(inValue).getValueID(), CSSValueNone);
    return operations;
  }

  const CSSToLengthConversionData& conversionData =
      state.cssToLengthConversionData();

  for (auto& currValue : toCSSValueList(inValue)) {
    if (currValue->isURIValue()) {
      countFilterUse(FilterOperation::REFERENCE, state.document());

      const CSSURIValue& urlValue = toCSSURIValue(*currValue);
      SVGElementProxy& elementProxy =
          state.elementStyleResources().cachedOrPendingFromValue(urlValue);
      operations.operations().push_back(
          ReferenceFilterOperation::create(urlValue.value(), elementProxy));
      continue;
    }

    const CSSFunctionValue* filterValue = toCSSFunctionValue(currValue.get());
    FilterOperation::OperationType operationType =
        filterOperationForType(filterValue->functionType());
    countFilterUse(operationType, state.document());
    DCHECK_LE(filterValue->length(), 1u);

    const CSSPrimitiveValue* firstValue =
        filterValue->length() && filterValue->item(0).isPrimitiveValue()
            ? &toCSSPrimitiveValue(filterValue->item(0))
            : nullptr;
    double firstNumber =
        StyleBuilderConverter::convertValueToNumber(filterValue, firstValue);

    switch (filterValue->functionType()) {
      case CSSValueGrayscale:
      case CSSValueSepia:
      case CSSValueSaturate:
      case CSSValueHueRotate: {
        operations.operations().push_back(
            BasicColorMatrixFilterOperation::create(firstNumber,
                                                    operationType));
        break;
      }
      case CSSValueInvert:
      case CSSValueBrightness:
      case CSSValueContrast:
      case CSSValueOpacity: {
        operations.operations().push_back(
            BasicComponentTransferFilterOperation::create(firstNumber,
                                                          operationType));
        break;
      }
      case CSSValueBlur: {
        Length stdDeviation = Length(0, Fixed);
        if (filterValue->length() >= 1) {
          stdDeviation = firstValue->convertToLength(conversionData);
        }
        operations.operations().push_back(
            BlurFilterOperation::create(stdDeviation));
        break;
      }
      case CSSValueDropShadow: {
        ShadowData shadow = StyleBuilderConverter::convertShadow(
            conversionData, &state, filterValue->item(0));
        // TODO(fs): Resolve 'currentcolor' when constructing the filter chain.
        if (shadow.color().isCurrentColor()) {
          shadow.overrideColor(state.style()->color());
        }
        operations.operations().push_back(
            DropShadowFilterOperation::create(shadow));
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }

  return operations;
}

FilterOperations FilterOperationResolver::createOffscreenFilterOperations(
    const CSSValue& inValue) {
  FilterOperations operations;

  if (inValue.isIdentifierValue()) {
    DCHECK_EQ(toCSSIdentifierValue(inValue).getValueID(), CSSValueNone);
    return operations;
  }

  FontDescription fontDescription;
  Font font(fontDescription);
  CSSToLengthConversionData::FontSizes fontSizes(
      kOffScreenCanvasEMFontSize, kOffScreenCanvasREMFontSize, &font);
  CSSToLengthConversionData::ViewportSize viewportSize(1024, 768);
  CSSToLengthConversionData conversionData(&ComputedStyle::initialStyle(),
                                           fontSizes, viewportSize, 1);

  for (auto& currValue : toCSSValueList(inValue)) {
    if (currValue->isURIValue())
      continue;

    const CSSFunctionValue* filterValue = toCSSFunctionValue(currValue.get());
    FilterOperation::OperationType operationType =
        filterOperationForType(filterValue->functionType());
    // TODO(fserb): Take an ExecutionContext argument to this function,
    // so we can have workers using UseCounter as well.
    // countFilterUse(operationType, state.document());
    DCHECK_LE(filterValue->length(), 1u);

    const CSSPrimitiveValue* firstValue =
        filterValue->length() && filterValue->item(0).isPrimitiveValue()
            ? &toCSSPrimitiveValue(filterValue->item(0))
            : nullptr;
    double firstNumber =
        StyleBuilderConverter::convertValueToNumber(filterValue, firstValue);

    switch (filterValue->functionType()) {
      case CSSValueGrayscale:
      case CSSValueSepia:
      case CSSValueSaturate:
      case CSSValueHueRotate: {
        operations.operations().push_back(
            BasicColorMatrixFilterOperation::create(firstNumber,
                                                    operationType));
        break;
      }
      case CSSValueInvert:
      case CSSValueBrightness:
      case CSSValueContrast:
      case CSSValueOpacity: {
        operations.operations().push_back(
            BasicComponentTransferFilterOperation::create(firstNumber,
                                                          operationType));
        break;
      }
      case CSSValueBlur: {
        Length stdDeviation = Length(0, Fixed);
        if (filterValue->length() >= 1) {
          stdDeviation = firstValue->convertToLength(conversionData);
        }
        operations.operations().push_back(
            BlurFilterOperation::create(stdDeviation));
        break;
      }
      case CSSValueDropShadow: {
        ShadowData shadow = StyleBuilderConverter::convertShadow(
            conversionData, nullptr, filterValue->item(0));
        // For offscreen canvas, the default color is always black.
        if (shadow.color().isCurrentColor()) {
          shadow.overrideColor(Color::black);
        }
        operations.operations().push_back(
            DropShadowFilterOperation::create(shadow));
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }
  return operations;
}

}  // namespace blink
