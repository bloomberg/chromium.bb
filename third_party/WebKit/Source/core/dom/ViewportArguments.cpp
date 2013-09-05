/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2012-2013 Intel Corporation. All rights reserved.
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
 *
 */

#include "config.h"
#include "core/dom/ViewportArguments.h"

#include "core/dom/Document.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "wtf/text/WTFString.h"

using namespace std;

namespace WebCore {

static const float& compareIgnoringAuto(const float& value1, const float& value2, const float& (*compare) (const float&, const float&))
{
    if (value1 == ViewportArguments::ValueAuto)
        return value2;

    if (value2 == ViewportArguments::ValueAuto)
        return value1;

    return compare(value1, value2);
}

static inline float clampLengthValue(float value)
{
    // Limits as defined in the css-device-adapt spec.
    if (value != ViewportArguments::ValueAuto)
        return min(float(10000), max(value, float(1)));
    return value;
}

static inline float clampScaleValue(float value)
{
    // Limits as defined in the css-device-adapt spec.
    if (value != ViewportArguments::ValueAuto)
        return min(float(10), max(value, float(0.1)));
    return value;
}

float ViewportArguments::resolveViewportLength(const Length& length, const FloatSize& initialViewportSize, Direction direction)
{
    if (length.isAuto())
        return ViewportArguments::ValueAuto;

    if (length.isFixed())
        return length.getFloatValue();

    if (length.type() == ExtendToZoom)
        return ViewportArguments::ValueExtendToZoom;

    if ((length.type() == Percent && direction == Horizontal) || length.type() == ViewportPercentageWidth)
        return initialViewportSize.width() * length.getFloatValue() / 100.0f;

    if ((length.type() == Percent && direction == Vertical) || length.type() == ViewportPercentageHeight)
        return initialViewportSize.height() * length.getFloatValue() / 100.0f;

    if (length.type() == ViewportPercentageMin)
        return min(initialViewportSize.width(), initialViewportSize.height()) * length.viewportPercentageLength() / 100.0f;

    if (length.type() == ViewportPercentageMax)
        return max(initialViewportSize.width(), initialViewportSize.height()) * length.viewportPercentageLength() / 100.0f;

    ASSERT_NOT_REACHED();
    return ViewportArguments::ValueAuto;
}

PageScaleConstraints ViewportArguments::resolve(const FloatSize& initialViewportSize) const
{
    float resultWidth = ValueAuto;
    float resultMaxWidth = resolveViewportLength(maxWidth, initialViewportSize, Horizontal);
    float resultMinWidth = resolveViewportLength(minWidth, initialViewportSize, Horizontal);
    float resultHeight = ValueAuto;
    float resultMaxHeight = resolveViewportLength(maxHeight, initialViewportSize, Vertical);
    float resultMinHeight = resolveViewportLength(minHeight, initialViewportSize, Vertical);

    float resultZoom = zoom;
    float resultMinZoom = minZoom;
    float resultMaxZoom = maxZoom;
    float resultUserZoom = userZoom;

    // 1. Resolve min-zoom and max-zoom values.
    if (resultMinZoom != ViewportArguments::ValueAuto && resultMaxZoom != ViewportArguments::ValueAuto)
        resultMaxZoom = max(resultMinZoom, resultMaxZoom);

    // 2. Constrain zoom value to the [min-zoom, max-zoom] range.
    if (resultZoom != ViewportArguments::ValueAuto)
        resultZoom = compareIgnoringAuto(resultMinZoom, compareIgnoringAuto(resultMaxZoom, resultZoom, min), max);

    float extendZoom = compareIgnoringAuto(resultZoom, resultMaxZoom, min);

    // 3. Resolve non-"auto" lengths to pixel lengths.
    if (extendZoom == ViewportArguments::ValueAuto) {
        if (resultMaxWidth == ViewportArguments::ValueExtendToZoom)
            resultMaxWidth = ViewportArguments::ValueAuto;

        if (resultMaxHeight == ViewportArguments::ValueExtendToZoom)
            resultMaxHeight = ViewportArguments::ValueAuto;

        if (resultMinWidth == ViewportArguments::ValueExtendToZoom)
            resultMinWidth = resultMaxWidth;

        if (resultMinHeight == ViewportArguments::ValueExtendToZoom)
            resultMinHeight = resultMaxHeight;
    } else {
        float extendWidth = initialViewportSize.width() / extendZoom;
        float extendHeight = initialViewportSize.height() / extendZoom;

        if (resultMaxWidth == ViewportArguments::ValueExtendToZoom)
            resultMaxWidth = extendWidth;

        if (resultMaxHeight == ViewportArguments::ValueExtendToZoom)
            resultMaxHeight = extendHeight;

        if (resultMinWidth == ViewportArguments::ValueExtendToZoom)
            resultMinWidth = compareIgnoringAuto(extendWidth, resultMaxWidth, max);

        if (resultMinHeight == ViewportArguments::ValueExtendToZoom)
            resultMinHeight = compareIgnoringAuto(extendHeight, resultMaxHeight, max);
    }

    // 4. Resolve initial width from min/max descriptors.
    if (resultMinWidth != ViewportArguments::ValueAuto || resultMaxWidth != ViewportArguments::ValueAuto)
        resultWidth = compareIgnoringAuto(resultMinWidth, compareIgnoringAuto(resultMaxWidth, initialViewportSize.width(), min), max);

    // 5. Resolve initial height from min/max descriptors.
    if (resultMinHeight != ViewportArguments::ValueAuto || resultMaxHeight != ViewportArguments::ValueAuto)
        resultHeight = compareIgnoringAuto(resultMinHeight, compareIgnoringAuto(resultMaxHeight, initialViewportSize.height(), min), max);

    // 6-7. Resolve width value.
    if (resultWidth == ViewportArguments::ValueAuto) {
        if (resultHeight == ViewportArguments::ValueAuto || !initialViewportSize.height())
            resultWidth = initialViewportSize.width();
        else
            resultWidth = resultHeight * (initialViewportSize.width() / initialViewportSize.height());
    }

    // 8. Resolve height value.
    if (resultHeight == ViewportArguments::ValueAuto) {
        if (!initialViewportSize.width())
            resultHeight = initialViewportSize.height();
        else
            resultHeight = resultWidth * initialViewportSize.height() / initialViewportSize.width();
    }

    // Resolve initial-scale value.
    if (resultZoom == ViewportArguments::ValueAuto) {
        if (resultWidth != ViewportArguments::ValueAuto && resultWidth > 0)
            resultZoom = initialViewportSize.width() / resultWidth;
        if (resultHeight != ViewportArguments::ValueAuto && resultHeight > 0) {
            // if 'auto', the initial-scale will be negative here and thus ignored.
            resultZoom = max<float>(resultZoom, initialViewportSize.height() / resultHeight);
        }
    }

    // If user-scalable = no, lock the min/max scale to the computed initial
    // scale.
    if (!resultUserZoom)
        resultMinZoom = resultMaxZoom = resultZoom;

    // Only set initialScale to a value if it was explicitly set.
    if (zoom == ViewportArguments::ValueAuto)
        resultZoom = ViewportArguments::ValueAuto;

    PageScaleConstraints result;
    result.minimumScale = resultMinZoom;
    result.maximumScale = resultMaxZoom;
    result.initialScale = resultZoom;
    result.layoutSize.setWidth(resultWidth);
    result.layoutSize.setHeight(resultHeight);
    return result;
}

static float numericPrefix(const String& keyString, const String& valueString, Document* document, bool* ok = 0)
{
    size_t parsedLength;
    float value;
    if (valueString.is8Bit())
        value = charactersToFloat(valueString.characters8(), valueString.length(), parsedLength);
    else
        value = charactersToFloat(valueString.characters16(), valueString.length(), parsedLength);
    if (!parsedLength) {
        reportViewportWarning(document, UnrecognizedViewportArgumentValueError, valueString, keyString);
        if (ok)
            *ok = false;
        return 0;
    }
    if (parsedLength < valueString.length())
        reportViewportWarning(document, TruncatedViewportArgumentValueError, valueString, keyString);
    if (ok)
        *ok = true;
    return value;
}

static Length findSizeValue(const String& keyString, const String& valueString, Document* document)
{
    // 1) Non-negative number values are translated to px lengths.
    // 2) Negative number values are translated to auto.
    // 3) device-width and device-height are used as keywords.
    // 4) Other keywords and unknown values translate to 0.0.

    if (equalIgnoringCase(valueString, "device-width"))
        return Length(100, ViewportPercentageWidth);
    if (equalIgnoringCase(valueString, "device-height"))
        return Length(100, ViewportPercentageHeight);

    float value = numericPrefix(keyString, valueString, document);

    if (value < 0)
        return Length(); // auto

    if (!static_cast<int>(value) && document->page() && document->page()->settings().viewportMetaZeroValuesQuirk()) {
        if (keyString == "width")
            return Length(100, ViewportPercentageWidth);
        if (keyString == "height")
            return Length(100, ViewportPercentageHeight);
    }

    return Length(clampLengthValue(value), Fixed);
}

static float findScaleValue(const String& keyString, const String& valueString, Document* document)
{
    // 1) Non-negative number values are translated to <number> values.
    // 2) Negative number values are translated to auto.
    // 3) yes is translated to 1.0.
    // 4) device-width and device-height are translated to 10.0.
    // 5) no and unknown values are translated to 0.0

    if (equalIgnoringCase(valueString, "yes"))
        return 1;
    if (equalIgnoringCase(valueString, "no"))
        return 0;
    if (equalIgnoringCase(valueString, "device-width"))
        return 10;
    if (equalIgnoringCase(valueString, "device-height"))
        return 10;

    float value = numericPrefix(keyString, valueString, document);

    if (value < 0)
        return ViewportArguments::ValueAuto;

    if (value > 10.0)
        reportViewportWarning(document, MaximumScaleTooLargeError, String(), String());

    if (!static_cast<int>(value) && document->page() && document->page()->settings().viewportMetaZeroValuesQuirk() && (keyString == "minimum-scale" || keyString == "maximum-scale"))
        return ViewportArguments::ValueAuto;

    return clampScaleValue(value);
}

static float findUserScalableValue(const String& keyString, const String& valueString, Document* document)
{
    // yes and no are used as keywords.
    // Numbers >= 1, numbers <= -1, device-width and device-height are mapped to yes.
    // Numbers in the range <-1, 1>, and unknown values, are mapped to no.

    if (equalIgnoringCase(valueString, "yes"))
        return 1;
    if (equalIgnoringCase(valueString, "no"))
        return 0;
    if (equalIgnoringCase(valueString, "device-width"))
        return 1;
    if (equalIgnoringCase(valueString, "device-height"))
        return 1;

    float value = numericPrefix(keyString, valueString, document);

    if (fabs(value) < 1)
        return 0;

    return 1;
}

static float findTargetDensityDPIValue(const String& keyString, const String& valueString, Document* document)
{
    if (equalIgnoringCase(valueString, "device-dpi"))
        return ViewportArguments::ValueDeviceDPI;
    if (equalIgnoringCase(valueString, "low-dpi"))
        return ViewportArguments::ValueLowDPI;
    if (equalIgnoringCase(valueString, "medium-dpi"))
        return ViewportArguments::ValueMediumDPI;
    if (equalIgnoringCase(valueString, "high-dpi"))
        return ViewportArguments::ValueHighDPI;

    bool ok;
    float value = numericPrefix(keyString, valueString, document, &ok);
    if (!ok || value < 70 || value > 400)
        return ViewportArguments::ValueAuto;

    return value;
}

void setViewportFeature(const String& keyString, const String& valueString, Document* document, void* data)
{
    ViewportArguments* arguments = static_cast<ViewportArguments*>(data);

    if (keyString == "width") {
        const Length& width = findSizeValue(keyString, valueString, document);
        if (!width.isAuto()) {
            arguments->minWidth = Length(ExtendToZoom);
            arguments->maxWidth = width;
        }
    } else if (keyString == "height") {
        const Length& height = findSizeValue(keyString, valueString, document);
        if (!height.isAuto()) {
            arguments->minHeight = Length(ExtendToZoom);
            arguments->maxHeight = height;
        }
    } else if (keyString == "initial-scale") {
        arguments->zoom = findScaleValue(keyString, valueString, document);
    } else if (keyString == "minimum-scale") {
        arguments->minZoom = findScaleValue(keyString, valueString, document);
    } else if (keyString == "maximum-scale") {
        arguments->maxZoom = findScaleValue(keyString, valueString, document);
    } else if (keyString == "user-scalable") {
        arguments->userZoom = findUserScalableValue(keyString, valueString, document);
    } else if (keyString == "target-densitydpi") {
        arguments->deprecatedTargetDensityDPI = findTargetDensityDPIValue(keyString, valueString, document);
        reportViewportWarning(document, TargetDensityDpiUnsupported, String(), String());
    } else {
        reportViewportWarning(document, UnrecognizedViewportArgumentKeyError, keyString, String());
    }
}

static const char* viewportErrorMessageTemplate(ViewportErrorCode errorCode)
{
    static const char* const errors[] = {
        "Note that ';' is not a key-value pair separator. The list should be comma-separated.",
        "The key \"%replacement1\" is not recognized and ignored.",
        "The value \"%replacement1\" for key \"%replacement2\" is invalid, and has been ignored.",
        "The value \"%replacement1\" for key \"%replacement2\" was truncated to its numeric prefix.",
        "The value for key \"maximum-scale\" is out of bounds and the value has been clamped.",
        "The key \"target-densitydpi\" is not supported.",
    };

    return errors[errorCode];
}

static MessageLevel viewportErrorMessageLevel(ViewportErrorCode errorCode)
{
    switch (errorCode) {
    case InvalidKeyValuePairSeparatorError:
    case TruncatedViewportArgumentValueError:
    case TargetDensityDpiUnsupported:
        return WarningMessageLevel;
    case UnrecognizedViewportArgumentKeyError:
    case UnrecognizedViewportArgumentValueError:
    case MaximumScaleTooLargeError:
        return ErrorMessageLevel;
    }

    ASSERT_NOT_REACHED();
    return ErrorMessageLevel;
}

void reportViewportWarning(Document* document, ViewportErrorCode errorCode, const String& replacement1, const String& replacement2)
{
    Frame* frame = document->frame();
    if (!frame)
        return;

    String message = viewportErrorMessageTemplate(errorCode);
    if (!replacement1.isNull())
        message.replace("%replacement1", replacement1);
    if (!replacement2.isNull())
        message.replace("%replacement2", replacement2);

    // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
    document->addConsoleMessage(RenderingMessageSource, viewportErrorMessageLevel(errorCode), message);
}

} // namespace WebCore
