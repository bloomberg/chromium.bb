/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGFitToViewBox.h"

#include "SVGNames.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/transforms/AffineTransform.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGParserUtilities.h"
#include "core/svg/SVGPreserveAspectRatio.h"
#include "wtf/text/StringImpl.h"

namespace WebCore {

template<typename CharType>
static bool parseViewBoxInternal(Document* document, const CharType*& ptr, const CharType* end, FloatRect& viewBox, bool validate)
{
    const CharType* start = ptr;

    skipOptionalSVGSpaces(ptr, end);

    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    bool valid = parseNumber(ptr, end, x) && parseNumber(ptr, end, y) && parseNumber(ptr, end, width) && parseNumber(ptr, end, height, false);
    if (!validate) {
        viewBox = FloatRect(x, y, width, height);
        return true;
    }
    if (!valid) {
        document->accessSVGExtensions()->reportWarning("Problem parsing viewBox=\"" + String(start, end - start) + "\"");
        return false;
    }

    if (width < 0.0) { // check that width is positive
        document->accessSVGExtensions()->reportError("A negative value for ViewBox width is not allowed");
        return false;
    }
    if (height < 0.0) { // check that height is positive
        document->accessSVGExtensions()->reportError("A negative value for ViewBox height is not allowed");
        return false;
    }
    skipOptionalSVGSpaces(ptr, end);
    if (ptr < end) { // nothing should come after the last, fourth number
        document->accessSVGExtensions()->reportWarning("Problem parsing viewBox=\"" + String(start, end - start) + "\"");
        return false;
    }

    viewBox = FloatRect(x, y, width, height);
    return true;
}

bool SVGFitToViewBox::parseViewBox(Document* document, const LChar*& ptr, const LChar* end, FloatRect& viewBox, bool validate)
{
    return parseViewBoxInternal(document, ptr, end, viewBox, validate);
}

bool SVGFitToViewBox::parseViewBox(Document* document, const UChar*& ptr, const UChar* end, FloatRect& viewBox, bool validate)
{
    return parseViewBoxInternal(document, ptr, end, viewBox, validate);
}

bool SVGFitToViewBox::parseViewBox(Document* document, const String& string, FloatRect& viewBox)
{
    if (string.isEmpty()) {
        const LChar* ptr = 0;
        return parseViewBoxInternal<LChar>(document, ptr, ptr, viewBox, true);
    }
    if (string.is8Bit()) {
        const LChar* ptr = string.characters8();
        const LChar* end = ptr + string.length();
        return parseViewBox(document, ptr, end, viewBox, true);
    }
    const UChar* ptr = string.characters16();
    const UChar* end = ptr + string.length();
    return parseViewBox(document, ptr, end, viewBox, true);
}

AffineTransform SVGFitToViewBox::viewBoxToViewTransform(const FloatRect& viewBoxRect, const SVGPreserveAspectRatio& preserveAspectRatio, float viewWidth, float viewHeight)
{
    if (!viewBoxRect.width() || !viewBoxRect.height())
        return AffineTransform();

    return preserveAspectRatio.getCTM(viewBoxRect.x(), viewBoxRect.y(), viewBoxRect.width(), viewBoxRect.height(), viewWidth, viewHeight);
}

bool SVGFitToViewBox::isKnownAttribute(const QualifiedName& attrName)
{
    return attrName == SVGNames::viewBoxAttr || attrName == SVGNames::preserveAspectRatioAttr;
}

void SVGFitToViewBox::addSupportedAttributes(HashSet<QualifiedName>& supportedAttributes)
{
    supportedAttributes.add(SVGNames::viewBoxAttr);
    supportedAttributes.add(SVGNames::preserveAspectRatioAttr);
}

}
