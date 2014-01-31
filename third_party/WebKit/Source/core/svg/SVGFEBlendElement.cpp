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

#include "config.h"

#include "core/svg/SVGFEBlendElement.h"

#include "SVGNames.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_ENUMERATION(SVGFEBlendElement, SVGNames::modeAttr, Mode, mode, BlendModeType)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFEBlendElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(mode)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFEBlendElement::SVGFEBlendElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feBlendTag, document)
    , m_in1(SVGAnimatedString::create(this, SVGNames::inAttr, SVGString::create()))
    , m_in2(SVGAnimatedString::create(this, SVGNames::in2Attr, SVGString::create()))
    , m_mode(FEBLEND_MODE_NORMAL)
{
    ScriptWrappable::init(this);
    addToPropertyMap(m_in1);
    addToPropertyMap(m_in2);
    registerAnimatedPropertiesForSVGFEBlendElement();
}

PassRefPtr<SVGFEBlendElement> SVGFEBlendElement::create(Document& document)
{
    return adoptRef(new SVGFEBlendElement(document));
}

bool SVGFEBlendElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::modeAttr);
        supportedAttributes.add(SVGNames::inAttr);
        supportedAttributes.add(SVGNames::in2Attr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFEBlendElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
        return;
    }

    if (name == SVGNames::modeAttr) {
        BlendModeType propertyValue = SVGPropertyTraits<BlendModeType>::fromString(value);
        if (propertyValue > 0)
            setModeBaseValue(propertyValue);
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::inAttr)
        m_in1->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::in2Attr)
        m_in2->setBaseValueAsString(value, parseError);
    else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

bool SVGFEBlendElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FEBlend* blend = static_cast<FEBlend*>(effect);
    if (attrName == SVGNames::modeAttr)
        return blend->setBlendMode(modeCurrentValue());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEBlendElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::modeAttr) {
        primitiveAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::inAttr || attrName == SVGNames::in2Attr) {
        invalidate();
        return;
    }

    ASSERT_NOT_REACHED();
}

PassRefPtr<FilterEffect> SVGFEBlendElement::build(SVGFilterBuilder* filterBuilder, Filter* filter)
{
    FilterEffect* input1 = filterBuilder->getEffectById(AtomicString(m_in1->currentValue()->value()));
    FilterEffect* input2 = filterBuilder->getEffectById(AtomicString(m_in2->currentValue()->value()));

    if (!input1 || !input2)
        return 0;

    RefPtr<FilterEffect> effect = FEBlend::create(filter, modeCurrentValue());
    FilterEffectVector& inputEffects = effect->inputEffects();
    inputEffects.reserveCapacity(2);
    inputEffects.append(input1);
    inputEffects.append(input2);
    return effect.release();
}

}
