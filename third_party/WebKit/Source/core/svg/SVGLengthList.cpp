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
#include "core/svg/SVGLengthList.h"

#include "bindings/v8/ExceptionState.h"
#include "core/svg/SVGParserUtilities.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

template<typename CharType>
void SVGLengthList::parseInternal(const CharType*& ptr, const CharType* end, SVGLengthMode mode)
{
    TrackExceptionState es;

    while (ptr < end) {
        const CharType* start = ptr;
        while (ptr < end && *ptr != ',' && !isSVGSpace(*ptr))
            ptr++;
        if (ptr == start)
            break;

        SVGLength length(mode);
        String valueString(start, ptr - start);
        if (valueString.isEmpty())
            return;
        length.setValueAsString(valueString, es);
        if (es.hadException())
            return;
        append(length);
        skipOptionalSVGSpacesOrDelimiter(ptr, end);
    }
}

void SVGLengthList::parse(const String& value, SVGLengthMode mode)
{
    clear();
    if (value.isEmpty())
        return;
    if (value.is8Bit()) {
        const LChar* ptr = value.characters8();
        const LChar* end = ptr + value.length();
        parseInternal(ptr, end, mode);
    } else {
        const UChar* ptr = value.characters16();
        const UChar* end = ptr + value.length();
        parseInternal(ptr, end, mode);
    }
}

String SVGLengthList::valueAsString() const
{
    StringBuilder builder;

    unsigned size = this->size();
    for (unsigned i = 0; i < size; ++i) {
        if (i > 0)
            builder.append(' ');

        builder.append(at(i).valueAsString());
    }

    return builder.toString();
}

}
