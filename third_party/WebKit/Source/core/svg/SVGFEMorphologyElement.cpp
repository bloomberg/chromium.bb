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

#include "config.h"

#include "core/svg/SVGFEMorphologyElement.h"

#include "SVGNames.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGParserUtilities.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGFEMorphologyElement, SVGNames::inAttr, In1, in1)
DEFINE_ANIMATED_ENUMERATION(SVGFEMorphologyElement, SVGNames::operatorAttr, SVGOperator, svgOperator, MorphologyOperatorType)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFEMorphologyElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(in1)
    REGISTER_LOCAL_ANIMATED_PROPERTY(svgOperator)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFEMorphologyElement::SVGFEMorphologyElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feMorphologyTag, document)
    , m_radius(SVGAnimatedNumberOptionalNumber::create(this, SVGNames::radiusAttr))
    , m_svgOperator(FEMORPHOLOGY_OPERATOR_ERODE)
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_radius);
    registerAnimatedPropertiesForSVGFEMorphologyElement();
}

PassRefPtr<SVGFEMorphologyElement> SVGFEMorphologyElement::create(Document& document)
{
    return adoptRef(new SVGFEMorphologyElement(document));
}

void SVGFEMorphologyElement::setRadius(float x, float y)
{
    radiusX()->baseValue()->setValue(x);
    radiusY()->baseValue()->setValue(y);
    invalidate();
}

bool SVGFEMorphologyElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::inAttr);
        supportedAttributes.add(SVGNames::operatorAttr);
        supportedAttributes.add(SVGNames::radiusAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFEMorphologyElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
        return;
    }

    if (name == SVGNames::operatorAttr) {
        MorphologyOperatorType propertyValue = SVGPropertyTraits<MorphologyOperatorType>::fromString(value);
        if (propertyValue > 0)
            setSVGOperatorBaseValue(propertyValue);
        return;
    }

    if (name == SVGNames::inAttr) {
        setIn1BaseValue(value);
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::radiusAttr)
        m_radius->setBaseValueAsString(value, parseError);
    else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

bool SVGFEMorphologyElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FEMorphology* morphology = static_cast<FEMorphology*>(effect);
    if (attrName == SVGNames::operatorAttr)
        return morphology->setMorphologyOperator(svgOperatorCurrentValue());
    if (attrName == SVGNames::radiusAttr) {
        // Both setRadius functions should be evaluated separately.
        bool isRadiusXChanged = morphology->setRadiusX(radiusX()->currentValue()->value());
        bool isRadiusYChanged = morphology->setRadiusY(radiusY()->currentValue()->value());
        return isRadiusXChanged || isRadiusYChanged;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEMorphologyElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::operatorAttr || attrName == SVGNames::radiusAttr) {
        primitiveAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::inAttr) {
        invalidate();
        return;
    }

    ASSERT_NOT_REACHED();
}

PassRefPtr<FilterEffect> SVGFEMorphologyElement::build(SVGFilterBuilder* filterBuilder, Filter* filter)
{
    FilterEffect* input1 = filterBuilder->getEffectById(AtomicString(in1CurrentValue()));
    float xRadius = radiusX()->currentValue()->value();
    float yRadius = radiusY()->currentValue()->value();

    if (!input1)
        return 0;

    if (xRadius < 0 || yRadius < 0)
        return 0;

    RefPtr<FilterEffect> effect = FEMorphology::create(filter, svgOperatorCurrentValue(), xRadius, yRadius);
    effect->inputEffects().append(input1);
    return effect.release();
}

} // namespace WebCore
