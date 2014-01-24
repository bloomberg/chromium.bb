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

#include "config.h"

#include "core/svg/SVGFEDropShadowElement.h"

#include "SVGNames.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/SVGRenderStyle.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGParserUtilities.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGFEDropShadowElement, SVGNames::inAttr, In1, in1)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFEDropShadowElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(in1)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFEDropShadowElement::SVGFEDropShadowElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feDropShadowTag, document)
    , m_dx(SVGAnimatedNumber::create(this, SVGNames::dxAttr, SVGNumber::create(2)))
    , m_dy(SVGAnimatedNumber::create(this, SVGNames::dyAttr, SVGNumber::create(2)))
    , m_stdDeviation(SVGAnimatedNumberOptionalNumber::create(this, SVGNames::stdDeviationAttr, 2, 2))
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_dx);
    addToPropertyMap(m_dy);
    addToPropertyMap(m_stdDeviation);
    registerAnimatedPropertiesForSVGFEDropShadowElement();
}

PassRefPtr<SVGFEDropShadowElement> SVGFEDropShadowElement::create(Document& document)
{
    return adoptRef(new SVGFEDropShadowElement(document));
}

void SVGFEDropShadowElement::setStdDeviation(float x, float y)
{
    stdDeviationX()->baseValue()->setValue(x);
    stdDeviationY()->baseValue()->setValue(y);
    invalidate();
}

bool SVGFEDropShadowElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::inAttr);
        supportedAttributes.add(SVGNames::dxAttr);
        supportedAttributes.add(SVGNames::dyAttr);
        supportedAttributes.add(SVGNames::stdDeviationAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFEDropShadowElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
        return;
    }

    if (name == SVGNames::inAttr) {
        setIn1BaseValue(value);
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::dxAttr)
        m_dx->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::dyAttr)
        m_dy->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::stdDeviationAttr)
        m_stdDeviation->setBaseValueAsString(value, parseError);
    else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGFEDropShadowElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::inAttr
        || attrName == SVGNames::stdDeviationAttr
        || attrName == SVGNames::dxAttr
        || attrName == SVGNames::dyAttr) {
        invalidate();
        return;
    }

    ASSERT_NOT_REACHED();
}

PassRefPtr<FilterEffect> SVGFEDropShadowElement::build(SVGFilterBuilder* filterBuilder, Filter* filter)
{
    RenderObject* renderer = this->renderer();
    if (!renderer)
        return 0;

    if (stdDeviationX()->currentValue()->value() < 0 || stdDeviationY()->currentValue()->value() < 0)
        return 0;

    ASSERT(renderer->style());
    const SVGRenderStyle* svgStyle = renderer->style()->svgStyle();

    Color color = svgStyle->floodColor();
    float opacity = svgStyle->floodOpacity();

    FilterEffect* input1 = filterBuilder->getEffectById(AtomicString(in1CurrentValue()));
    if (!input1)
        return 0;

    RefPtr<FilterEffect> effect = FEDropShadow::create(filter, stdDeviationX()->currentValue()->value(), stdDeviationY()->currentValue()->value(), m_dx->currentValue()->value(), m_dy->currentValue()->value(), color, opacity);
    effect->inputEffects().append(input1);
    return effect.release();
}

}
