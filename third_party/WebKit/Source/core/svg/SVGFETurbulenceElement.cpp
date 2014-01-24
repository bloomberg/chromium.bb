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

#include "core/svg/SVGFETurbulenceElement.h"

#include "SVGNames.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGParserUtilities.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_INTEGER(SVGFETurbulenceElement, SVGNames::numOctavesAttr, NumOctaves, numOctaves)
DEFINE_ANIMATED_ENUMERATION(SVGFETurbulenceElement, SVGNames::stitchTilesAttr, StitchTiles, stitchTiles, SVGStitchOptions)
DEFINE_ANIMATED_ENUMERATION(SVGFETurbulenceElement, SVGNames::typeAttr, Type, type, TurbulenceType)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFETurbulenceElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(numOctaves)
    REGISTER_LOCAL_ANIMATED_PROPERTY(stitchTiles)
    REGISTER_LOCAL_ANIMATED_PROPERTY(type)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFETurbulenceElement::SVGFETurbulenceElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feTurbulenceTag, document)
    , m_baseFrequency(SVGAnimatedNumberOptionalNumber::create(this, SVGNames::baseFrequencyAttr))
    , m_seed(SVGAnimatedNumber::create(this, SVGNames::seedAttr, SVGNumber::create(0)))
    , m_numOctaves(1)
    , m_stitchTiles(SVG_STITCHTYPE_NOSTITCH)
    , m_type(FETURBULENCE_TYPE_TURBULENCE)
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_baseFrequency);
    addToPropertyMap(m_seed);
    registerAnimatedPropertiesForSVGFETurbulenceElement();
}

PassRefPtr<SVGFETurbulenceElement> SVGFETurbulenceElement::create(Document& document)
{
    return adoptRef(new SVGFETurbulenceElement(document));
}

bool SVGFETurbulenceElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::baseFrequencyAttr);
        supportedAttributes.add(SVGNames::numOctavesAttr);
        supportedAttributes.add(SVGNames::seedAttr);
        supportedAttributes.add(SVGNames::stitchTilesAttr);
        supportedAttributes.add(SVGNames::typeAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFETurbulenceElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
        return;
    }

    if (name == SVGNames::typeAttr) {
        TurbulenceType propertyValue = SVGPropertyTraits<TurbulenceType>::fromString(value);
        if (propertyValue > 0)
            setTypeBaseValue(propertyValue);
        return;
    }

    if (name == SVGNames::stitchTilesAttr) {
        SVGStitchOptions propertyValue = SVGPropertyTraits<SVGStitchOptions>::fromString(value);
        if (propertyValue > 0)
            setStitchTilesBaseValue(propertyValue);
        return;
    }

    if (name == SVGNames::numOctavesAttr) {
        setNumOctavesBaseValue(value.string().toUIntStrict());
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::baseFrequencyAttr)
        m_baseFrequency->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::seedAttr)
        m_seed->setBaseValueAsString(value, parseError);
    else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

bool SVGFETurbulenceElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FETurbulence* turbulence = static_cast<FETurbulence*>(effect);
    if (attrName == SVGNames::typeAttr)
        return turbulence->setType(typeCurrentValue());
    if (attrName == SVGNames::stitchTilesAttr)
        return turbulence->setStitchTiles(stitchTilesCurrentValue());
    if (attrName == SVGNames::baseFrequencyAttr) {
        bool baseFrequencyXChanged = turbulence->setBaseFrequencyX(baseFrequencyX()->currentValue()->value());
        bool baseFrequencyYChanged = turbulence->setBaseFrequencyY(baseFrequencyY()->currentValue()->value());
        return (baseFrequencyXChanged || baseFrequencyYChanged);
    }
    if (attrName == SVGNames::seedAttr)
        return turbulence->setSeed(m_seed->currentValue()->value());
    if (attrName == SVGNames::numOctavesAttr)
        return turbulence->setNumOctaves(numOctavesCurrentValue());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFETurbulenceElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::baseFrequencyAttr
        || attrName == SVGNames::numOctavesAttr
        || attrName == SVGNames::seedAttr
        || attrName == SVGNames::stitchTilesAttr
        || attrName == SVGNames::typeAttr) {
        primitiveAttributeChanged(attrName);
        return;
    }

    ASSERT_NOT_REACHED();
}

PassRefPtr<FilterEffect> SVGFETurbulenceElement::build(SVGFilterBuilder*, Filter* filter)
{
    if (baseFrequencyX()->currentValue()->value() < 0 || baseFrequencyY()->currentValue()->value() < 0)
        return 0;
    return FETurbulence::create(filter, typeCurrentValue(), baseFrequencyX()->currentValue()->value(), baseFrequencyY()->currentValue()->value(), numOctavesCurrentValue(), m_seed->currentValue()->value(), stitchTilesCurrentValue() == SVG_STITCHTYPE_STITCH);
}

}
