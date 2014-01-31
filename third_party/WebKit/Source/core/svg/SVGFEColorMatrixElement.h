/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGFEColorMatrixElement_h
#define SVGFEColorMatrixElement_h

#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedNumberList.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/filters/FEColorMatrix.h"

namespace WebCore {

template<>
struct SVGPropertyTraits<ColorMatrixType> {
    static unsigned highestEnumValue() { return FECOLORMATRIX_TYPE_LUMINANCETOALPHA; }

    static String toString(ColorMatrixType type)
    {
        switch (type) {
        case FECOLORMATRIX_TYPE_UNKNOWN:
            return emptyString();
        case FECOLORMATRIX_TYPE_MATRIX:
            return "matrix";
        case FECOLORMATRIX_TYPE_SATURATE:
            return "saturate";
        case FECOLORMATRIX_TYPE_HUEROTATE:
            return "hueRotate";
        case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
            return "luminanceToAlpha";
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static ColorMatrixType fromString(const String& value)
    {
        if (value == "matrix")
            return FECOLORMATRIX_TYPE_MATRIX;
        if (value == "saturate")
            return FECOLORMATRIX_TYPE_SATURATE;
        if (value == "hueRotate")
            return FECOLORMATRIX_TYPE_HUEROTATE;
        if (value == "luminanceToAlpha")
            return FECOLORMATRIX_TYPE_LUMINANCETOALPHA;
        return FECOLORMATRIX_TYPE_UNKNOWN;
    }
};

class SVGFEColorMatrixElement FINAL : public SVGFilterPrimitiveStandardAttributes {
public:
    static PassRefPtr<SVGFEColorMatrixElement> create(Document&);

    SVGAnimatedNumberList* values() { return m_values.get(); }
    SVGAnimatedString* in1() { return m_in1.get(); }

private:
    explicit SVGFEColorMatrixElement(Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;
    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*, Filter*) OVERRIDE;

    RefPtr<SVGAnimatedNumberList> m_values;
    RefPtr<SVGAnimatedString> m_in1;
    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGFEColorMatrixElement)
        DECLARE_ANIMATED_ENUMERATION(Type, type, ColorMatrixType)
    END_DECLARE_ANIMATED_PROPERTIES
};

} // namespace WebCore

#endif
