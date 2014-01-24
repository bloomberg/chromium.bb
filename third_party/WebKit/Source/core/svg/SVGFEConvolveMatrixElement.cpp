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

#include "core/svg/SVGFEConvolveMatrixElement.h"

#include "SVGNames.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGParserUtilities.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntSize.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGFEConvolveMatrixElement, SVGNames::inAttr, In1, in1)
DEFINE_ANIMATED_INTEGER_MULTIPLE_WRAPPERS(SVGFEConvolveMatrixElement, SVGNames::orderAttr, orderXIdentifier(), OrderX, orderX)
DEFINE_ANIMATED_INTEGER_MULTIPLE_WRAPPERS(SVGFEConvolveMatrixElement, SVGNames::orderAttr, orderYIdentifier(), OrderY, orderY)
DEFINE_ANIMATED_INTEGER(SVGFEConvolveMatrixElement, SVGNames::targetXAttr, TargetX, targetX)
DEFINE_ANIMATED_INTEGER(SVGFEConvolveMatrixElement, SVGNames::targetYAttr, TargetY, targetY)
DEFINE_ANIMATED_ENUMERATION(SVGFEConvolveMatrixElement, SVGNames::edgeModeAttr, EdgeMode, edgeMode, EdgeModeType)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFEConvolveMatrixElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(in1)
    REGISTER_LOCAL_ANIMATED_PROPERTY(orderX)
    REGISTER_LOCAL_ANIMATED_PROPERTY(orderY)
    REGISTER_LOCAL_ANIMATED_PROPERTY(targetX)
    REGISTER_LOCAL_ANIMATED_PROPERTY(targetY)
    REGISTER_LOCAL_ANIMATED_PROPERTY(edgeMode)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFEConvolveMatrixElement::SVGFEConvolveMatrixElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feConvolveMatrixTag, document)
    , m_preserveAlpha(SVGAnimatedBoolean::create(this, SVGNames::preserveAlphaAttr, SVGBoolean::create()))
    , m_divisor(SVGAnimatedNumber::create(this, SVGNames::divisorAttr, SVGNumber::create()))
    , m_bias(SVGAnimatedNumber::create(this, SVGNames::biasAttr, SVGNumber::create()))
    , m_kernelUnitLength(SVGAnimatedNumberOptionalNumber::create(this, SVGNames::kernelUnitLengthAttr))
    , m_kernelMatrix(SVGAnimatedNumberList::create(this, SVGNames::kernelMatrixAttr, SVGNumberList::create()))
    , m_edgeMode(EDGEMODE_DUPLICATE)
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_preserveAlpha);
    addToPropertyMap(m_divisor);
    addToPropertyMap(m_bias);
    addToPropertyMap(m_kernelUnitLength);
    addToPropertyMap(m_kernelMatrix);
    registerAnimatedPropertiesForSVGFEConvolveMatrixElement();
}

PassRefPtr<SVGFEConvolveMatrixElement> SVGFEConvolveMatrixElement::create(Document& document)
{
    return adoptRef(new SVGFEConvolveMatrixElement(document));
}

const AtomicString& SVGFEConvolveMatrixElement::orderXIdentifier()
{
    DEFINE_STATIC_LOCAL(AtomicString, s_identifier, ("SVGOrderX", AtomicString::ConstructFromLiteral));
    return s_identifier;
}

const AtomicString& SVGFEConvolveMatrixElement::orderYIdentifier()
{
    DEFINE_STATIC_LOCAL(AtomicString, s_identifier, ("SVGOrderY", AtomicString::ConstructFromLiteral));
    return s_identifier;
}

bool SVGFEConvolveMatrixElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::inAttr);
        supportedAttributes.add(SVGNames::orderAttr);
        supportedAttributes.add(SVGNames::kernelMatrixAttr);
        supportedAttributes.add(SVGNames::edgeModeAttr);
        supportedAttributes.add(SVGNames::divisorAttr);
        supportedAttributes.add(SVGNames::biasAttr);
        supportedAttributes.add(SVGNames::targetXAttr);
        supportedAttributes.add(SVGNames::targetYAttr);
        supportedAttributes.add(SVGNames::kernelUnitLengthAttr);
        supportedAttributes.add(SVGNames::preserveAlphaAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFEConvolveMatrixElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
        return;
    }

    if (name == SVGNames::inAttr) {
        setIn1BaseValue(value);
        return;
    }

    if (name == SVGNames::orderAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y) && x >= 1 && y >= 1) {
            setOrderXBaseValue(x);
            setOrderYBaseValue(y);
        } else {
            document().accessSVGExtensions()->reportWarning(
                "feConvolveMatrix: problem parsing order=\"" + value
                + "\". Filtered element will not be displayed.");
        }
        return;
    }

    if (name == SVGNames::edgeModeAttr) {
        EdgeModeType propertyValue = SVGPropertyTraits<EdgeModeType>::fromString(value);
        if (propertyValue > 0)
            setEdgeModeBaseValue(propertyValue);
        else
            document().accessSVGExtensions()->reportWarning(
                "feConvolveMatrix: problem parsing edgeMode=\"" + value
                + "\". Filtered element will not be displayed.");
        return;
    }

    if (name == SVGNames::targetXAttr) {
        setTargetXBaseValue(value.string().toUIntStrict());
        return;
    }

    if (name == SVGNames::targetYAttr) {
        setTargetYBaseValue(value.string().toUIntStrict());
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::divisorAttr)
        m_divisor->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::biasAttr)
        m_bias->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::kernelUnitLengthAttr)
        m_kernelUnitLength->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::kernelMatrixAttr)
        m_kernelMatrix->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::preserveAlphaAttr)
        m_preserveAlpha->setBaseValueAsString(value, parseError);
    else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

bool SVGFEConvolveMatrixElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FEConvolveMatrix* convolveMatrix = static_cast<FEConvolveMatrix*>(effect);
    if (attrName == SVGNames::edgeModeAttr)
        return convolveMatrix->setEdgeMode(edgeModeCurrentValue());
    if (attrName == SVGNames::divisorAttr)
        return convolveMatrix->setDivisor(m_divisor->currentValue()->value());
    if (attrName == SVGNames::biasAttr)
        return convolveMatrix->setBias(m_bias->currentValue()->value());
    if (attrName == SVGNames::targetXAttr)
        return convolveMatrix->setTargetOffset(IntPoint(targetXCurrentValue(), targetYCurrentValue()));
    if (attrName == SVGNames::targetYAttr)
        return convolveMatrix->setTargetOffset(IntPoint(targetXCurrentValue(), targetYCurrentValue()));
    if (attrName == SVGNames::kernelUnitLengthAttr)
        return convolveMatrix->setKernelUnitLength(FloatPoint(kernelUnitLengthX()->currentValue()->value(), kernelUnitLengthY()->currentValue()->value()));
    if (attrName == SVGNames::preserveAlphaAttr)
        return convolveMatrix->setPreserveAlpha(m_preserveAlpha->currentValue()->value());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEConvolveMatrixElement::setOrder(float x, float y)
{
    setOrderXBaseValue(x);
    setOrderYBaseValue(y);
    invalidate();
}

void SVGFEConvolveMatrixElement::setKernelUnitLength(float x, float y)
{
    kernelUnitLengthX()->baseValue()->setValue(x);
    kernelUnitLengthY()->baseValue()->setValue(y);
    invalidate();
}

void SVGFEConvolveMatrixElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::edgeModeAttr
        || attrName == SVGNames::divisorAttr
        || attrName == SVGNames::biasAttr
        || attrName == SVGNames::targetXAttr
        || attrName == SVGNames::targetYAttr
        || attrName == SVGNames::kernelUnitLengthAttr
        || attrName == SVGNames::preserveAlphaAttr) {
        primitiveAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::inAttr
        || attrName == SVGNames::orderAttr
        || attrName == SVGNames::kernelMatrixAttr) {
        invalidate();
        return;
    }

    ASSERT_NOT_REACHED();
}

PassRefPtr<FilterEffect> SVGFEConvolveMatrixElement::build(SVGFilterBuilder* filterBuilder, Filter* filter)
{
    FilterEffect* input1 = filterBuilder->getEffectById(AtomicString(in1CurrentValue()));

    if (!input1)
        return 0;

    int orderXValue = orderXCurrentValue();
    int orderYValue = orderYCurrentValue();
    if (!hasAttribute(SVGNames::orderAttr)) {
        orderXValue = 3;
        orderYValue = 3;
    }
    // Spec says order must be > 0. Bail if it is not.
    if (orderXValue < 1 || orderYValue < 1)
        return 0;
    RefPtr<SVGNumberList> kernelMatrix = this->m_kernelMatrix->currentValue();
    size_t kernelMatrixSize = kernelMatrix->numberOfItems();
    // The spec says this is a requirement, and should bail out if fails
    if (orderXValue * orderYValue != static_cast<int>(kernelMatrixSize))
        return 0;

    int targetXValue = targetXCurrentValue();
    int targetYValue = targetYCurrentValue();
    if (hasAttribute(SVGNames::targetXAttr) && (targetXValue < 0 || targetXValue >= orderXValue))
        return 0;
    // The spec says the default value is: targetX = floor ( orderX / 2 ))
    if (!hasAttribute(SVGNames::targetXAttr))
        targetXValue = static_cast<int>(floorf(orderXValue / 2));
    if (hasAttribute(SVGNames::targetYAttr) && (targetYValue < 0 || targetYValue >= orderYValue))
        return 0;
    // The spec says the default value is: targetY = floor ( orderY / 2 ))
    if (!hasAttribute(SVGNames::targetYAttr))
        targetYValue = static_cast<int>(floorf(orderYValue / 2));

    // Spec says default kernelUnitLength is 1.0, and a specified length cannot be 0.
    // FIXME: Why is this cast from float -> int -> float?
    int kernelUnitLengthXValue = kernelUnitLengthX()->currentValue()->value();
    int kernelUnitLengthYValue = kernelUnitLengthY()->currentValue()->value();
    if (!hasAttribute(SVGNames::kernelUnitLengthAttr)) {
        kernelUnitLengthXValue = 1;
        kernelUnitLengthYValue = 1;
    }
    if (kernelUnitLengthXValue <= 0 || kernelUnitLengthYValue <= 0)
        return 0;

    float divisorValue = m_divisor->currentValue()->value();
    if (hasAttribute(SVGNames::divisorAttr) && !divisorValue)
        return 0;
    if (!hasAttribute(SVGNames::divisorAttr)) {
        for (size_t i = 0; i < kernelMatrixSize; ++i)
            divisorValue += kernelMatrix->at(i)->value();
        if (!divisorValue)
            divisorValue = 1;
    }

    RefPtr<FilterEffect> effect = FEConvolveMatrix::create(filter,
                    IntSize(orderXValue, orderYValue), divisorValue,
                    m_bias->currentValue()->value(), IntPoint(targetXValue, targetYValue), edgeModeCurrentValue(),
                    FloatPoint(kernelUnitLengthXValue, kernelUnitLengthYValue), m_preserveAlpha->currentValue()->value(), m_kernelMatrix->currentValue()->toFloatVector());
    effect->inputEffects().append(input1);
    return effect.release();
}

} // namespace WebCore
