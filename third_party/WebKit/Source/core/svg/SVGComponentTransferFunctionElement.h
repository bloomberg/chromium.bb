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

#ifndef SVGComponentTransferFunctionElement_h
#define SVGComponentTransferFunctionElement_h

#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGAnimatedNumberList.h"
#include "core/svg/SVGElement.h"
#include "platform/graphics/filters/FEComponentTransfer.h"

namespace WebCore {

template<>
struct SVGPropertyTraits<ComponentTransferType> {
    static unsigned highestEnumValue() { return FECOMPONENTTRANSFER_TYPE_GAMMA; }

    static String toString(ComponentTransferType type)
    {
        switch (type) {
        case FECOMPONENTTRANSFER_TYPE_UNKNOWN:
            return emptyString();
        case FECOMPONENTTRANSFER_TYPE_IDENTITY:
            return "identity";
        case FECOMPONENTTRANSFER_TYPE_TABLE:
            return "table";
        case FECOMPONENTTRANSFER_TYPE_DISCRETE:
            return "discrete";
        case FECOMPONENTTRANSFER_TYPE_LINEAR:
            return "linear";
        case FECOMPONENTTRANSFER_TYPE_GAMMA:
            return "gamma";
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static ComponentTransferType fromString(const String& value)
    {
        if (value == "identity")
            return FECOMPONENTTRANSFER_TYPE_IDENTITY;
        if (value == "table")
            return FECOMPONENTTRANSFER_TYPE_TABLE;
        if (value == "discrete")
            return FECOMPONENTTRANSFER_TYPE_DISCRETE;
        if (value == "linear")
            return FECOMPONENTTRANSFER_TYPE_LINEAR;
        if (value == "gamma")
            return FECOMPONENTTRANSFER_TYPE_GAMMA;
        return FECOMPONENTTRANSFER_TYPE_UNKNOWN;
    }
};

class SVGComponentTransferFunctionElement : public SVGElement {
public:
    ComponentTransferFunction transferFunction() const;

    SVGAnimatedNumberList* tableValues() { return m_tableValues.get(); }
    SVGAnimatedNumber* slope() { return m_slope.get(); }
    SVGAnimatedNumber* intercept() { return m_intercept.get(); }
    SVGAnimatedNumber* amplitude() { return m_amplitude.get(); }
    SVGAnimatedNumber* exponent() { return m_exponent.get(); }
    SVGAnimatedNumber* offset() { return m_offset.get(); }

protected:
    SVGComponentTransferFunctionElement(const QualifiedName&, Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE FINAL;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE FINAL;

    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE FINAL { return false; }

private:
    RefPtr<SVGAnimatedNumberList> m_tableValues;
    RefPtr<SVGAnimatedNumber> m_slope;
    RefPtr<SVGAnimatedNumber> m_intercept;
    RefPtr<SVGAnimatedNumber> m_amplitude;
    RefPtr<SVGAnimatedNumber> m_exponent;
    RefPtr<SVGAnimatedNumber> m_offset;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGComponentTransferFunctionElement)
        DECLARE_ANIMATED_ENUMERATION(Type, type, ComponentTransferType)
    END_DECLARE_ANIMATED_PROPERTIES
};

} // namespace WebCore

#endif
