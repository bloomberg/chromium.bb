/*
 * Copyright (C) 2006 Oliver Hunt <oliver@nerget.com>
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

#include "core/svg/SVGFEDisplacementMapElement.h"

#include "SVGNames.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_ENUMERATION(SVGFEDisplacementMapElement, SVGNames::xChannelSelectorAttr, XChannelSelector, xChannelSelector, ChannelSelectorType)
DEFINE_ANIMATED_ENUMERATION(SVGFEDisplacementMapElement, SVGNames::yChannelSelectorAttr, YChannelSelector, yChannelSelector, ChannelSelectorType)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFEDisplacementMapElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(xChannelSelector)
    REGISTER_LOCAL_ANIMATED_PROPERTY(yChannelSelector)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFEDisplacementMapElement::SVGFEDisplacementMapElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feDisplacementMapTag, document)
    , m_scale(SVGAnimatedNumber::create(this, SVGNames::scaleAttr, SVGNumber::create(0)))
    , m_in1(SVGAnimatedString::create(this, SVGNames::inAttr, SVGString::create()))
    , m_in2(SVGAnimatedString::create(this, SVGNames::in2Attr, SVGString::create()))
    , m_xChannelSelector(CHANNEL_A)
    , m_yChannelSelector(CHANNEL_A)
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_scale);
    addToPropertyMap(m_in1);
    addToPropertyMap(m_in2);
    registerAnimatedPropertiesForSVGFEDisplacementMapElement();
}

PassRefPtr<SVGFEDisplacementMapElement> SVGFEDisplacementMapElement::create(Document& document)
{
    return adoptRef(new SVGFEDisplacementMapElement(document));
}

bool SVGFEDisplacementMapElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::inAttr);
        supportedAttributes.add(SVGNames::in2Attr);
        supportedAttributes.add(SVGNames::xChannelSelectorAttr);
        supportedAttributes.add(SVGNames::yChannelSelectorAttr);
        supportedAttributes.add(SVGNames::scaleAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFEDisplacementMapElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
        return;
    }

    if (name == SVGNames::xChannelSelectorAttr) {
        ChannelSelectorType propertyValue = SVGPropertyTraits<ChannelSelectorType>::fromString(value);
        if (propertyValue > 0)
            setXChannelSelectorBaseValue(propertyValue);
        return;
    }

    if (name == SVGNames::yChannelSelectorAttr) {
        ChannelSelectorType propertyValue = SVGPropertyTraits<ChannelSelectorType>::fromString(value);
        if (propertyValue > 0)
            setYChannelSelectorBaseValue(propertyValue);
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::inAttr)
        m_in1->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::in2Attr)
        m_in2->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::scaleAttr)
        m_scale->setBaseValueAsString(value, parseError);
    else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

bool SVGFEDisplacementMapElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FEDisplacementMap* displacementMap = static_cast<FEDisplacementMap*>(effect);
    if (attrName == SVGNames::xChannelSelectorAttr)
        return displacementMap->setXChannelSelector(xChannelSelectorCurrentValue());
    if (attrName == SVGNames::yChannelSelectorAttr)
        return displacementMap->setYChannelSelector(yChannelSelectorCurrentValue());
    if (attrName == SVGNames::scaleAttr)
        return displacementMap->setScale(m_scale->currentValue()->value());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEDisplacementMapElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::xChannelSelectorAttr || attrName == SVGNames::yChannelSelectorAttr || attrName == SVGNames::scaleAttr) {
        primitiveAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::inAttr || attrName == SVGNames::in2Attr) {
        invalidate();
        return;
    }

    ASSERT_NOT_REACHED();
}

PassRefPtr<FilterEffect> SVGFEDisplacementMapElement::build(SVGFilterBuilder* filterBuilder, Filter* filter)
{
    FilterEffect* input1 = filterBuilder->getEffectById(AtomicString(m_in1->currentValue()->value()));
    FilterEffect* input2 = filterBuilder->getEffectById(AtomicString(m_in2->currentValue()->value()));

    if (!input1 || !input2)
        return 0;

    RefPtr<FilterEffect> effect = FEDisplacementMap::create(filter, xChannelSelectorCurrentValue(), yChannelSelectorCurrentValue(), m_scale->currentValue()->value());
    FilterEffectVector& inputEffects = effect->inputEffects();
    inputEffects.reserveCapacity(2);
    inputEffects.append(input1);
    inputEffects.append(input2);
    return effect.release();
}

}
