/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#ifndef SVGAnimatedType_h
#define SVGAnimatedType_h

#include "core/svg/SVGAngle.h"
#include "core/svg/properties/NewSVGAnimatedProperty.h"
#include "core/svg/properties/SVGPropertyInfo.h"

namespace WebCore {

class SVGPathByteStream;

class SVGAnimatedType FINAL {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ~SVGAnimatedType();

    static PassOwnPtr<SVGAnimatedType> createPath(PassOwnPtr<SVGPathByteStream>);
    // Temporary compatibility layer. This shouldn't be needed after all properties are switched to NewSVGAnimatedProperty impl.
    static PassOwnPtr<SVGAnimatedType> createNewProperty(PassRefPtr<NewSVGPropertyBase>);
    static bool supportsAnimVal(AnimatedPropertyType);

    AnimatedPropertyType type() const { return m_type; }

    SVGPathByteStream* path()
    {
        ASSERT(m_type == AnimatedPath);
        return m_data.path;
    }

    RefPtr<NewSVGPropertyBase>& newProperty()
    {
        ASSERT(m_newProperty);
        return m_newProperty;
    }

    String valueAsString();
    bool setValueAsString(const QualifiedName&, const String&);

private:
    SVGAnimatedType(AnimatedPropertyType);

    AnimatedPropertyType m_type;

    union DataUnion {
        DataUnion()
        {
        }

        SVGPathByteStream* path;
    } m_data;
    RefPtr<NewSVGPropertyBase> m_newProperty;
};

} // namespace WebCore

#endif // SVGAnimatedType_h
