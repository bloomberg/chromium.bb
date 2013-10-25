/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGRect_h
#define SVGRect_h

#include "core/svg/properties/SVGPropertyTraits.h"
#include "platform/geometry/FloatRect.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

class SVGRect : public FloatRect {
public:
    struct InvalidSVGRectTag { };

    SVGRect()
        : m_isValid(true) { }
    SVGRect(InvalidSVGRectTag)
        : m_isValid(false) { }
    SVGRect(const FloatRect& rect)
        : FloatRect(rect), m_isValid(true) { }
    SVGRect(const FloatPoint& location, const FloatSize& size)
        : FloatRect(location, size), m_isValid(true) { }
    SVGRect(float x, float y, float width, float height)
        : FloatRect(x, y, width, height), m_isValid(true) { }
    SVGRect(const IntRect& intRect)
        : FloatRect(intRect), m_isValid(true) { }
    SVGRect(const LayoutRect& layoutRect)
        : FloatRect(layoutRect), m_isValid(true) { }
    SVGRect(const SkRect& skRect)
        : FloatRect(skRect), m_isValid(true) { }

    bool isValid() const { return m_isValid; }

private:
    bool m_isValid;
};

template<>
struct SVGPropertyTraits<SVGRect> {
    static SVGRect initialValue() { return SVGRect(SVGRect::InvalidSVGRectTag()); }
    static String toString(const SVGRect& type)
    {
        StringBuilder builder;
        builder.append(String::number(type.x()));
        builder.append(' ');
        builder.append(String::number(type.y()));
        builder.append(' ');
        builder.append(String::number(type.width()));
        builder.append(' ');
        builder.append(String::number(type.height()));
        return builder.toString();
    }
};

} // namespace WebCore

#endif // SVGRect_h
