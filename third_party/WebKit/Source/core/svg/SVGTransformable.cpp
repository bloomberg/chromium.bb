/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "core/svg/SVGTransformable.h"

#include "core/platform/graphics/transforms/AffineTransform.h"
#include "core/svg/SVGParserUtilities.h"
#include "core/svg/SVGTransformList.h"

namespace WebCore {

template<typename CharType>
static int parseTransformParamList(const CharType*& ptr, const CharType* end, float* values, int required, int optional)
{
    int optionalParams = 0, requiredParams = 0;

    if (!skipOptionalSVGSpaces(ptr, end) || *ptr != '(')
        return -1;

    ptr++;

    skipOptionalSVGSpaces(ptr, end);

    while (requiredParams < required) {
        if (ptr >= end || !parseNumber(ptr, end, values[requiredParams], false))
            return -1;
        requiredParams++;
        if (requiredParams < required)
            skipOptionalSVGSpacesOrDelimiter(ptr, end);
    }
    if (!skipOptionalSVGSpaces(ptr, end))
        return -1;

    bool delimParsed = skipOptionalSVGSpacesOrDelimiter(ptr, end);

    if (ptr >= end)
        return -1;

    if (*ptr == ')') { // skip optionals
        ptr++;
        if (delimParsed)
            return -1;
    } else {
        while (optionalParams < optional) {
            if (ptr >= end || !parseNumber(ptr, end, values[requiredParams + optionalParams], false))
                return -1;
            optionalParams++;
            if (optionalParams < optional)
                skipOptionalSVGSpacesOrDelimiter(ptr, end);
        }

        if (!skipOptionalSVGSpaces(ptr, end))
            return -1;

        delimParsed = skipOptionalSVGSpacesOrDelimiter(ptr, end);

        if (ptr >= end || *ptr != ')' || delimParsed)
            return -1;
        ptr++;
    }

    return requiredParams + optionalParams;
}

// These should be kept in sync with enum SVGTransformType
static const int requiredValuesForType[] =  {0, 6, 1, 1, 1, 1, 1};
static const int optionalValuesForType[] =  {0, 0, 1, 1, 2, 0, 0};

// This destructor is needed in order to link correctly with Intel ICC.
SVGTransformable::~SVGTransformable()
{
}

template<typename CharType>
static bool parseTransformValueInternal(unsigned type, const CharType*& ptr, const CharType* end, SVGTransform& transform)
{
    if (type == SVGTransform::SVG_TRANSFORM_UNKNOWN)
        return false;

    int valueCount = 0;
    float values[] = {0, 0, 0, 0, 0, 0};
    if ((valueCount = parseTransformParamList(ptr, end, values, requiredValuesForType[type], optionalValuesForType[type])) < 0)
        return false;

    switch (type) {
    case SVGTransform::SVG_TRANSFORM_SKEWX:
        transform.setSkewX(values[0]);
        break;
    case SVGTransform::SVG_TRANSFORM_SKEWY:
        transform.setSkewY(values[0]);
        break;
    case SVGTransform::SVG_TRANSFORM_SCALE:
        if (valueCount == 1) // Spec: if only one param given, assume uniform scaling
            transform.setScale(values[0], values[0]);
        else
            transform.setScale(values[0], values[1]);
        break;
    case SVGTransform::SVG_TRANSFORM_TRANSLATE:
        if (valueCount == 1) // Spec: if only one param given, assume 2nd param to be 0
            transform.setTranslate(values[0], 0);
        else
            transform.setTranslate(values[0], values[1]);
        break;
    case SVGTransform::SVG_TRANSFORM_ROTATE:
        if (valueCount == 1)
            transform.setRotate(values[0], 0, 0);
        else
            transform.setRotate(values[0], values[1], values[2]);
        break;
    case SVGTransform::SVG_TRANSFORM_MATRIX:
        transform.setMatrix(AffineTransform(values[0], values[1], values[2], values[3], values[4], values[5]));
        break;
    }

    return true;
}

bool SVGTransformable::parseTransformValue(unsigned type, const LChar*& ptr, const LChar* end, SVGTransform& transform)
{
    return parseTransformValueInternal(type, ptr, end, transform);
}

bool SVGTransformable::parseTransformValue(unsigned type, const UChar*& ptr, const UChar* end, SVGTransform& transform)
{
    return parseTransformValueInternal(type, ptr, end, transform);
}

static const LChar skewXDesc[] =  {'s', 'k', 'e', 'w', 'X'};
static const LChar skewYDesc[] =  {'s', 'k', 'e', 'w', 'Y'};
static const LChar scaleDesc[] =  {'s', 'c', 'a', 'l', 'e'};
static const LChar translateDesc[] =  {'t', 'r', 'a', 'n', 's', 'l', 'a', 't', 'e'};
static const LChar rotateDesc[] =  {'r', 'o', 't', 'a', 't', 'e'};
static const LChar matrixDesc[] =  {'m', 'a', 't', 'r', 'i', 'x'};

template<typename CharType>
static inline bool parseAndSkipType(const CharType*& ptr, const CharType* end, unsigned short& type)
{
    if (ptr >= end)
        return false;

    if (*ptr == 's') {
        if (skipString(ptr, end, skewXDesc, WTF_ARRAY_LENGTH(skewXDesc)))
            type = SVGTransform::SVG_TRANSFORM_SKEWX;
        else if (skipString(ptr, end, skewYDesc, WTF_ARRAY_LENGTH(skewYDesc)))
            type = SVGTransform::SVG_TRANSFORM_SKEWY;
        else if (skipString(ptr, end, scaleDesc, WTF_ARRAY_LENGTH(scaleDesc)))
            type = SVGTransform::SVG_TRANSFORM_SCALE;
        else
            return false;
    } else if (skipString(ptr, end, translateDesc, WTF_ARRAY_LENGTH(translateDesc)))
        type = SVGTransform::SVG_TRANSFORM_TRANSLATE;
    else if (skipString(ptr, end, rotateDesc, WTF_ARRAY_LENGTH(rotateDesc)))
        type = SVGTransform::SVG_TRANSFORM_ROTATE;
    else if (skipString(ptr, end, matrixDesc, WTF_ARRAY_LENGTH(matrixDesc)))
        type = SVGTransform::SVG_TRANSFORM_MATRIX;
    else
        return false;

    return true;
}

SVGTransform::SVGTransformType SVGTransformable::parseTransformType(const String& string)
{
    if (string.isEmpty())
        return SVGTransform::SVG_TRANSFORM_UNKNOWN;
    unsigned short type = SVGTransform::SVG_TRANSFORM_UNKNOWN;
    if (string.is8Bit()) {
        const LChar* ptr = string.characters8();
        const LChar* end = ptr + string.length();
        parseAndSkipType(ptr, end, type);
    } else {
        const UChar* ptr = string.characters16();
        const UChar* end = ptr + string.length();
        parseAndSkipType(ptr, end, type);
    }
    return static_cast<SVGTransform::SVGTransformType>(type);
}

template<typename CharType>
bool SVGTransformable::parseTransformAttributeInternal(SVGTransformList& list, const CharType*& ptr, const CharType* end, TransformParsingMode mode)
{
    if (mode == ClearList)
        list.clear();

    bool delimParsed = false;
    while (ptr < end) {
        delimParsed = false;
        unsigned short type = SVGTransform::SVG_TRANSFORM_UNKNOWN;
        skipOptionalSVGSpaces(ptr, end);

        if (!parseAndSkipType(ptr, end, type))
            return false;

        SVGTransform transform;
        if (!parseTransformValue(type, ptr, end, transform))
            return false;

        list.append(transform);
        skipOptionalSVGSpaces(ptr, end);
        if (ptr < end && *ptr == ',') {
            delimParsed = true;
            ++ptr;
        }
        skipOptionalSVGSpaces(ptr, end);
    }

    return !delimParsed;
}

bool SVGTransformable::parseTransformAttribute(SVGTransformList& list, const LChar*& ptr, const LChar* end, TransformParsingMode mode)
{
    return parseTransformAttributeInternal(list, ptr, end, mode);
}

bool SVGTransformable::parseTransformAttribute(SVGTransformList& list, const UChar*& ptr, const UChar* end, TransformParsingMode mode)
{
    return parseTransformAttributeInternal(list, ptr, end, mode);
}

}
