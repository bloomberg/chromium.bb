/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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
#include "core/svg/SVGStringList.h"

#include "core/svg/SVGElement.h"
#include "core/svg/SVGParserUtilities.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

void SVGStringList::commitChange(SVGElement* contextElement)
{
    ASSERT(contextElement);
    contextElement->invalidateSVGAttributes();
    contextElement->svgAttributeChanged(m_attributeName);
}

void SVGStringList::reset(const String& string)
{
    parse(string, ' ');

    // Add empty string, if list is empty.
    if (isEmpty())
        append(emptyString());
}

template<typename CharType>
void SVGStringList::parseInternal(const CharType*& ptr, const CharType* end, UChar delimiter)
{
    while (ptr < end) {
        const CharType* start = ptr;
        while (ptr < end && *ptr != delimiter && !isSVGSpace(*ptr))
            ptr++;
        if (ptr == start)
            break;
        append(String(start, ptr - start));
        skipOptionalSVGSpacesOrDelimiter(ptr, end, delimiter);
    }
}

void SVGStringList::parse(const String& data, UChar delimiter)
{
    // FIXME: Add more error checking and reporting.
    clear();
    if (data.isEmpty())
        return;
    if (data.is8Bit()) {
        const LChar* ptr = data.characters8();
        const LChar* end = ptr + data.length();
        parseInternal(ptr, end, delimiter);
    } else {
        const UChar* ptr = data.characters16();
        const UChar* end = ptr + data.length();
        parseInternal(ptr, end, delimiter);
    }
}

String SVGStringList::valueAsString() const
{
    StringBuilder builder;

    unsigned size = this->size();
    for (unsigned i = 0; i < size; ++i) {
        if (i > 0)
            builder.append(' ');

        builder.append(at(i));
    }

    return builder.toString();
}

}
