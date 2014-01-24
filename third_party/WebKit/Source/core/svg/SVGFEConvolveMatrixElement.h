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

#ifndef SVGFEConvolveMatrixElement_h
#define SVGFEConvolveMatrixElement_h

#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedInteger.h"
#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGAnimatedNumberList.h"
#include "core/svg/SVGAnimatedNumberOptionalNumber.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/filters/FEConvolveMatrix.h"

namespace WebCore {

template<>
struct SVGPropertyTraits<EdgeModeType> {
    static unsigned highestEnumValue() { return EDGEMODE_NONE; }

    static String toString(EdgeModeType type)
    {
        switch (type) {
        case EDGEMODE_UNKNOWN:
            return emptyString();
        case EDGEMODE_DUPLICATE:
            return "duplicate";
        case EDGEMODE_WRAP:
            return "wrap";
        case EDGEMODE_NONE:
            return "none";
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static EdgeModeType fromString(const String& value)
    {
        if (value == "duplicate")
            return EDGEMODE_DUPLICATE;
        if (value == "wrap")
            return EDGEMODE_WRAP;
        if (value == "none")
            return EDGEMODE_NONE;
        return EDGEMODE_UNKNOWN;
    }
};

class SVGFEConvolveMatrixElement FINAL : public SVGFilterPrimitiveStandardAttributes {
public:
    static PassRefPtr<SVGFEConvolveMatrixElement> create(Document&);

    void setOrder(float orderX, float orderY);
    void setKernelUnitLength(float kernelUnitLengthX, float kernelUnitLengthY);

    SVGAnimatedBoolean* preserveAlpha() { return m_preserveAlpha.get(); }
    SVGAnimatedNumber* divisor() { return m_divisor.get(); }
    SVGAnimatedNumber* bias() { return m_bias.get(); }
    SVGAnimatedNumber* kernelUnitLengthX() { return m_kernelUnitLength->firstNumber(); }
    SVGAnimatedNumber* kernelUnitLengthY() { return m_kernelUnitLength->secondNumber(); }
    SVGAnimatedNumberList* kernelMatrix() { return m_kernelMatrix.get(); }

private:
    explicit SVGFEConvolveMatrixElement(Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;
    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*, Filter*) OVERRIDE;

    static const AtomicString& orderXIdentifier();
    static const AtomicString& orderYIdentifier();

    RefPtr<SVGAnimatedBoolean> m_preserveAlpha;
    RefPtr<SVGAnimatedNumber> m_divisor;
    RefPtr<SVGAnimatedNumber> m_bias;
    RefPtr<SVGAnimatedNumberOptionalNumber> m_kernelUnitLength;
    RefPtr<SVGAnimatedNumberList> m_kernelMatrix;
    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGFEConvolveMatrixElement)
        DECLARE_ANIMATED_STRING(In1, in1)
        DECLARE_ANIMATED_INTEGER(OrderX, orderX)
        DECLARE_ANIMATED_INTEGER(OrderY, orderY)
        DECLARE_ANIMATED_INTEGER(TargetX, targetX)
        DECLARE_ANIMATED_INTEGER(TargetY, targetY)
        DECLARE_ANIMATED_ENUMERATION(EdgeMode, edgeMode, EdgeModeType)
    END_DECLARE_ANIMATED_PROPERTIES
};

} // namespace WebCore

#endif
