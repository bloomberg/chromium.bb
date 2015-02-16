/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#include "config.h"
#include "core/css/resolver/TransformBuilder.h"

#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "platform/heap/Handle.h"
#include "platform/transforms/Matrix3DTransformOperation.h"
#include "platform/transforms/MatrixTransformOperation.h"
#include "platform/transforms/PerspectiveTransformOperation.h"
#include "platform/transforms/RotateTransformOperation.h"
#include "platform/transforms/ScaleTransformOperation.h"
#include "platform/transforms/SkewTransformOperation.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/transforms/TranslateTransformOperation.h"

namespace blink {

static Length convertToFloatLength(CSSPrimitiveValue* primitiveValue, const CSSToLengthConversionData& conversionData)
{
    ASSERT(primitiveValue);
    return primitiveValue->convertToLength(conversionData);
}

static TransformOperation::OperationType getTransformOperationType(CSSValueID type)
{
    switch (type) {
    case CSSValueScale: return TransformOperation::Scale;
    case CSSValueScaleX: return TransformOperation::ScaleX;
    case CSSValueScaleY: return TransformOperation::ScaleY;
    case CSSValueScaleZ: return TransformOperation::ScaleZ;
    case CSSValueScale3d: return TransformOperation::Scale3D;
    case CSSValueTranslate: return TransformOperation::Translate;
    case CSSValueTranslateX: return TransformOperation::TranslateX;
    case CSSValueTranslateY: return TransformOperation::TranslateY;
    case CSSValueTranslateZ: return TransformOperation::TranslateZ;
    case CSSValueTranslate3d: return TransformOperation::Translate3D;
    case CSSValueRotate: return TransformOperation::Rotate;
    case CSSValueRotateX: return TransformOperation::RotateX;
    case CSSValueRotateY: return TransformOperation::RotateY;
    case CSSValueRotateZ: return TransformOperation::RotateZ;
    case CSSValueRotate3d: return TransformOperation::Rotate3D;
    case CSSValueSkew: return TransformOperation::Skew;
    case CSSValueSkewX: return TransformOperation::SkewX;
    case CSSValueSkewY: return TransformOperation::SkewY;
    case CSSValueMatrix: return TransformOperation::Matrix;
    case CSSValueMatrix3d: return TransformOperation::Matrix3D;
    case CSSValuePerspective: return TransformOperation::Perspective;
    default:
        ASSERT_NOT_REACHED();
        // FIXME: We shouldn't have a type None since we never create them
        return TransformOperation::None;
    }
}

bool TransformBuilder::createTransformOperations(CSSValue* inValue, const CSSToLengthConversionData& conversionData, TransformOperations& outOperations)
{
    if (!inValue || !inValue->isValueList()) {
        outOperations.clear();
        return false;
    }

    float zoomFactor = conversionData.zoom();
    TransformOperations operations;
    for (CSSValueListIterator i = inValue; i.hasMore(); i.advance()) {
        CSSValue* currValue = i.value();

        if (!currValue->isFunctionValue())
            continue;

        CSSFunctionValue* transformValue = toCSSFunctionValue(i.value());
        if (!transformValue->length())
            continue;

        bool haveNonPrimitiveValue = false;
        for (unsigned j = 0; j < transformValue->length(); ++j) {
            if (!transformValue->item(j)->isPrimitiveValue()) {
                haveNonPrimitiveValue = true;
                break;
            }
        }
        if (haveNonPrimitiveValue)
            continue;

        CSSPrimitiveValue* firstValue = toCSSPrimitiveValue(transformValue->item(0));

        switch (transformValue->functionType()) {
        case CSSValueScale:
        case CSSValueScaleX:
        case CSSValueScaleY: {
            double sx = 1.0;
            double sy = 1.0;
            if (transformValue->functionType() == CSSValueScaleY)
                sy = firstValue->getDoubleValue();
            else {
                sx = firstValue->getDoubleValue();
                if (transformValue->functionType() != CSSValueScaleX) {
                    if (transformValue->length() > 1) {
                        CSSPrimitiveValue* secondValue = toCSSPrimitiveValue(transformValue->item(1));
                        sy = secondValue->getDoubleValue();
                    } else
                        sy = sx;
                }
            }
            operations.operations().append(ScaleTransformOperation::create(sx, sy, 1.0, getTransformOperationType(transformValue->functionType())));
            break;
        }
        case CSSValueScaleZ:
        case CSSValueScale3d: {
            double sx = 1.0;
            double sy = 1.0;
            double sz = 1.0;
            if (transformValue->functionType() == CSSValueScaleZ)
                sz = firstValue->getDoubleValue();
            else if (transformValue->functionType() == CSSValueScaleY)
                sy = firstValue->getDoubleValue();
            else {
                sx = firstValue->getDoubleValue();
                if (transformValue->functionType() != CSSValueScaleX) {
                    if (transformValue->length() > 2) {
                        CSSPrimitiveValue* thirdValue = toCSSPrimitiveValue(transformValue->item(2));
                        sz = thirdValue->getDoubleValue();
                    }
                    if (transformValue->length() > 1) {
                        CSSPrimitiveValue* secondValue = toCSSPrimitiveValue(transformValue->item(1));
                        sy = secondValue->getDoubleValue();
                    } else
                        sy = sx;
                }
            }
            operations.operations().append(ScaleTransformOperation::create(sx, sy, sz, getTransformOperationType(transformValue->functionType())));
            break;
        }
        case CSSValueTranslate:
        case CSSValueTranslateX:
        case CSSValueTranslateY: {
            Length tx = Length(0, Fixed);
            Length ty = Length(0, Fixed);
            if (transformValue->functionType() == CSSValueTranslateY)
                ty = convertToFloatLength(firstValue, conversionData);
            else {
                tx = convertToFloatLength(firstValue, conversionData);
                if (transformValue->functionType() != CSSValueTranslateX) {
                    if (transformValue->length() > 1) {
                        CSSPrimitiveValue* secondValue = toCSSPrimitiveValue(transformValue->item(1));
                        ty = convertToFloatLength(secondValue, conversionData);
                    }
                }
            }

            operations.operations().append(TranslateTransformOperation::create(tx, ty, 0, getTransformOperationType(transformValue->functionType())));
            break;
        }
        case CSSValueTranslateZ:
        case CSSValueTranslate3d: {
            Length tx = Length(0, Fixed);
            Length ty = Length(0, Fixed);
            double tz = 0;
            if (transformValue->functionType() == CSSValueTranslateZ)
                tz = firstValue->computeLength<double>(conversionData);
            else if (transformValue->functionType() == CSSValueTranslateY)
                ty = convertToFloatLength(firstValue, conversionData);
            else {
                tx = convertToFloatLength(firstValue, conversionData);
                if (transformValue->functionType() != CSSValueTranslateX) {
                    if (transformValue->length() > 2) {
                        CSSPrimitiveValue* thirdValue = toCSSPrimitiveValue(transformValue->item(2));
                        tz = thirdValue->computeLength<double>(conversionData);
                    }
                    if (transformValue->length() > 1) {
                        CSSPrimitiveValue* secondValue = toCSSPrimitiveValue(transformValue->item(1));
                        ty = convertToFloatLength(secondValue, conversionData);
                    }
                }
            }

            operations.operations().append(TranslateTransformOperation::create(tx, ty, tz, getTransformOperationType(transformValue->functionType())));
            break;
        }
        case CSSValueRotate: {
            double angle = firstValue->computeDegrees();
            operations.operations().append(RotateTransformOperation::create(0, 0, 1, angle, getTransformOperationType(transformValue->functionType())));
            break;
        }
        case CSSValueRotateX:
        case CSSValueRotateY:
        case CSSValueRotateZ: {
            double x = 0;
            double y = 0;
            double z = 0;
            double angle = firstValue->computeDegrees();

            if (transformValue->functionType() == CSSValueRotateX)
                x = 1;
            else if (transformValue->functionType() == CSSValueRotateY)
                y = 1;
            else
                z = 1;
            operations.operations().append(RotateTransformOperation::create(x, y, z, angle, getTransformOperationType(transformValue->functionType())));
            break;
        }
        case CSSValueRotate3d: {
            if (transformValue->length() < 4)
                break;
            CSSPrimitiveValue* secondValue = toCSSPrimitiveValue(transformValue->item(1));
            CSSPrimitiveValue* thirdValue = toCSSPrimitiveValue(transformValue->item(2));
            CSSPrimitiveValue* fourthValue = toCSSPrimitiveValue(transformValue->item(3));
            double x = firstValue->getDoubleValue();
            double y = secondValue->getDoubleValue();
            double z = thirdValue->getDoubleValue();
            double angle = fourthValue->computeDegrees();
            operations.operations().append(RotateTransformOperation::create(x, y, z, angle, getTransformOperationType(transformValue->functionType())));
            break;
        }
        case CSSValueSkew:
        case CSSValueSkewX:
        case CSSValueSkewY: {
            double angleX = 0;
            double angleY = 0;
            double angle = firstValue->computeDegrees();
            if (transformValue->functionType() == CSSValueSkewY)
                angleY = angle;
            else {
                angleX = angle;
                if (transformValue->functionType() == CSSValueSkew) {
                    if (transformValue->length() > 1) {
                        CSSPrimitiveValue* secondValue = toCSSPrimitiveValue(transformValue->item(1));
                        angleY = secondValue->computeDegrees();
                    }
                }
            }
            operations.operations().append(SkewTransformOperation::create(angleX, angleY, getTransformOperationType(transformValue->functionType())));
            break;
        }
        case CSSValueMatrix: {
            if (transformValue->length() < 6)
                break;
            double a = firstValue->getDoubleValue();
            double b = toCSSPrimitiveValue(transformValue->item(1))->getDoubleValue();
            double c = toCSSPrimitiveValue(transformValue->item(2))->getDoubleValue();
            double d = toCSSPrimitiveValue(transformValue->item(3))->getDoubleValue();
            double e = zoomFactor * toCSSPrimitiveValue(transformValue->item(4))->getDoubleValue();
            double f = zoomFactor * toCSSPrimitiveValue(transformValue->item(5))->getDoubleValue();
            operations.operations().append(MatrixTransformOperation::create(a, b, c, d, e, f));
            break;
        }
        case CSSValueMatrix3d: {
            if (transformValue->length() < 16)
                break;
            TransformationMatrix matrix(toCSSPrimitiveValue(transformValue->item(0))->getDoubleValue(),
                toCSSPrimitiveValue(transformValue->item(1))->getDoubleValue(),
                toCSSPrimitiveValue(transformValue->item(2))->getDoubleValue(),
                toCSSPrimitiveValue(transformValue->item(3))->getDoubleValue(),
                toCSSPrimitiveValue(transformValue->item(4))->getDoubleValue(),
                toCSSPrimitiveValue(transformValue->item(5))->getDoubleValue(),
                toCSSPrimitiveValue(transformValue->item(6))->getDoubleValue(),
                toCSSPrimitiveValue(transformValue->item(7))->getDoubleValue(),
                toCSSPrimitiveValue(transformValue->item(8))->getDoubleValue(),
                toCSSPrimitiveValue(transformValue->item(9))->getDoubleValue(),
                toCSSPrimitiveValue(transformValue->item(10))->getDoubleValue(),
                toCSSPrimitiveValue(transformValue->item(11))->getDoubleValue(),
                zoomFactor * toCSSPrimitiveValue(transformValue->item(12))->getDoubleValue(),
                zoomFactor * toCSSPrimitiveValue(transformValue->item(13))->getDoubleValue(),
                toCSSPrimitiveValue(transformValue->item(14))->getDoubleValue(),
                toCSSPrimitiveValue(transformValue->item(15))->getDoubleValue());
            operations.operations().append(Matrix3DTransformOperation::create(matrix));
            break;
        }
        case CSSValuePerspective: {
            double p;
            if (firstValue->isLength())
                p = firstValue->computeLength<double>(conversionData);
            else {
                // This is a quirk that should go away when 3d transforms are finalized.
                double val = firstValue->getDoubleValue();
                if (val < 0)
                    return false;
                p = clampTo<int>(val, 0);
            }

            operations.operations().append(PerspectiveTransformOperation::create(p));
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    outOperations = operations;
    return true;
}

} // namespace blink
