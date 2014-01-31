/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#ifndef SVGFEMorphologyElement_h
#define SVGFEMorphologyElement_h

#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedNumberOptionalNumber.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/filters/FEMorphology.h"

namespace WebCore {

template<>
struct SVGPropertyTraits<MorphologyOperatorType> {
    static unsigned highestEnumValue() { return FEMORPHOLOGY_OPERATOR_DILATE; }

    static String toString(MorphologyOperatorType type)
    {
        switch (type) {
        case FEMORPHOLOGY_OPERATOR_UNKNOWN:
            return emptyString();
        case FEMORPHOLOGY_OPERATOR_ERODE:
            return "erode";
        case FEMORPHOLOGY_OPERATOR_DILATE:
            return "dilate";
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static MorphologyOperatorType fromString(const String& value)
    {
        if (value == "erode")
            return FEMORPHOLOGY_OPERATOR_ERODE;
        if (value == "dilate")
            return FEMORPHOLOGY_OPERATOR_DILATE;
        return FEMORPHOLOGY_OPERATOR_UNKNOWN;
    }
};

class SVGFEMorphologyElement FINAL : public SVGFilterPrimitiveStandardAttributes {
public:
    static PassRefPtr<SVGFEMorphologyElement> create(Document&);

    void setRadius(float radiusX, float radiusY);

    SVGAnimatedNumber* radiusX() { return m_radius->firstNumber(); }
    SVGAnimatedNumber* radiusY() { return m_radius->secondNumber(); }
    SVGAnimatedString* in1() { return m_in1.get(); }

private:
    explicit SVGFEMorphologyElement(Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;
    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*, Filter*) OVERRIDE;

    RefPtr<SVGAnimatedNumberOptionalNumber> m_radius;
    RefPtr<SVGAnimatedString> m_in1;
    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGFEMorphologyElement)
        DECLARE_ANIMATED_ENUMERATION(SVGOperator, svgOperator, MorphologyOperatorType)
    END_DECLARE_ANIMATED_PROPERTIES
};

} // namespace WebCore

#endif
