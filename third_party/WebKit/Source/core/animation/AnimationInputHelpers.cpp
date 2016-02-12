// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/AnimationInputHelpers.h"

#include "core/SVGNames.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/CSSToStyleMap.h"
#include "core/svg/SVGElement.h"
#include "core/svg/animation/SVGSMILElement.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

const char kSVGPrefix[] = "svg-";
const unsigned kSVGPrefixLength = sizeof(kSVGPrefix) - 1;

static bool isSVGPrefixed(const String& property)
{
    return property.startsWith(kSVGPrefix);
}

static String removeSVGPrefix(const String& property)
{
    ASSERT(isSVGPrefixed(property));
    return property.substring(kSVGPrefixLength);
}

CSSPropertyID AnimationInputHelpers::keyframeAttributeToCSSProperty(const String& property)
{
    // Disallow prefixed properties.
    if (property[0] == '-' || isASCIIUpper(property[0]))
        return CSSPropertyInvalid;
    if (property == "cssFloat")
        return CSSPropertyFloat;
    StringBuilder builder;
    for (size_t i = 0; i < property.length(); ++i) {
        if (isASCIIUpper(property[i]))
            builder.append('-');
        builder.append(property[i]);
    }
    return cssPropertyID(builder.toString());
}

CSSPropertyID AnimationInputHelpers::keyframeAttributeToPresentationAttribute(const String& property, const Element& element)
{
    if (!RuntimeEnabledFeatures::webAnimationsSVGEnabled() || !element.isSVGElement() || !isSVGPrefixed(property))
        return CSSPropertyInvalid;

    String unprefixedProperty = removeSVGPrefix(property);
    if (SVGElement::isAnimatableCSSProperty(QualifiedName(nullAtom, AtomicString(unprefixedProperty), nullAtom)))
        return cssPropertyID(unprefixedProperty);

    return CSSPropertyInvalid;
}

using AttributeNameMap = HashMap<QualifiedName, const QualifiedName*>;

const AttributeNameMap& getSupportedAttributes()
{
    DEFINE_STATIC_LOCAL(AttributeNameMap, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        // Fill the set for the first use.
        // Animatable attributes from http://www.w3.org/TR/SVG/attindex.html
        const QualifiedName* attributes[] = {
            &HTMLNames::classAttr,
            &SVGNames::amplitudeAttr,
            &SVGNames::azimuthAttr,
            &SVGNames::baseFrequencyAttr,
            &SVGNames::biasAttr,
            &SVGNames::clipPathUnitsAttr,
            &SVGNames::cxAttr,
            &SVGNames::cyAttr,
            &SVGNames::dAttr,
            &SVGNames::diffuseConstantAttr,
            &SVGNames::divisorAttr,
            &SVGNames::dxAttr,
            &SVGNames::dyAttr,
            &SVGNames::edgeModeAttr,
            &SVGNames::elevationAttr,
            &SVGNames::exponentAttr,
            &SVGNames::filterUnitsAttr,
            &SVGNames::fxAttr,
            &SVGNames::fyAttr,
            &SVGNames::gradientTransformAttr,
            &SVGNames::gradientUnitsAttr,
            &SVGNames::heightAttr,
            &SVGNames::hrefAttr,
            &SVGNames::in2Attr,
            &SVGNames::inAttr,
            &SVGNames::interceptAttr,
            &SVGNames::k1Attr,
            &SVGNames::k2Attr,
            &SVGNames::k3Attr,
            &SVGNames::k4Attr,
            &SVGNames::kernelMatrixAttr,
            &SVGNames::kernelUnitLengthAttr,
            &SVGNames::lengthAdjustAttr,
            &SVGNames::limitingConeAngleAttr,
            &SVGNames::markerHeightAttr,
            &SVGNames::markerUnitsAttr,
            &SVGNames::markerWidthAttr,
            &SVGNames::maskContentUnitsAttr,
            &SVGNames::maskUnitsAttr,
            &SVGNames::methodAttr,
            &SVGNames::modeAttr,
            &SVGNames::numOctavesAttr,
            &SVGNames::offsetAttr,
            &SVGNames::opacityAttr,
            &SVGNames::operatorAttr,
            &SVGNames::orderAttr,
            &SVGNames::orientAttr,
            &SVGNames::pathLengthAttr,
            &SVGNames::patternContentUnitsAttr,
            &SVGNames::patternTransformAttr,
            &SVGNames::patternUnitsAttr,
            &SVGNames::pointsAtXAttr,
            &SVGNames::pointsAtYAttr,
            &SVGNames::pointsAtZAttr,
            &SVGNames::pointsAttr,
            &SVGNames::preserveAlphaAttr,
            &SVGNames::preserveAspectRatioAttr,
            &SVGNames::primitiveUnitsAttr,
            &SVGNames::rAttr,
            &SVGNames::radiusAttr,
            &SVGNames::refXAttr,
            &SVGNames::refYAttr,
            &SVGNames::resultAttr,
            &SVGNames::rotateAttr,
            &SVGNames::rxAttr,
            &SVGNames::ryAttr,
            &SVGNames::scaleAttr,
            &SVGNames::seedAttr,
            &SVGNames::slopeAttr,
            &SVGNames::spacingAttr,
            &SVGNames::specularConstantAttr,
            &SVGNames::specularExponentAttr,
            &SVGNames::spreadMethodAttr,
            &SVGNames::startOffsetAttr,
            &SVGNames::stdDeviationAttr,
            &SVGNames::stitchTilesAttr,
            &SVGNames::surfaceScaleAttr,
            &SVGNames::tableValuesAttr,
            &SVGNames::targetAttr,
            &SVGNames::targetXAttr,
            &SVGNames::targetYAttr,
            &SVGNames::textLengthAttr,
            &SVGNames::transformAttr,
            &SVGNames::typeAttr,
            &SVGNames::valuesAttr,
            &SVGNames::viewBoxAttr,
            &SVGNames::widthAttr,
            &SVGNames::x1Attr,
            &SVGNames::x2Attr,
            &SVGNames::xAttr,
            &SVGNames::xChannelSelectorAttr,
            &SVGNames::y1Attr,
            &SVGNames::y2Attr,
            &SVGNames::yAttr,
            &SVGNames::yChannelSelectorAttr,
            &SVGNames::zAttr,
        };
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(attributes); i++)
            supportedAttributes.set(*attributes[i], attributes[i]);
    }
    return supportedAttributes;
}

QualifiedName svgAttributeName(const String& property)
{
    ASSERT(!isSVGPrefixed(property));
    return QualifiedName(nullAtom, AtomicString(property), nullAtom);
}

const QualifiedName* AnimationInputHelpers::keyframeAttributeToSVGAttribute(const String& property, Element& element)
{
    if (!RuntimeEnabledFeatures::webAnimationsSVGEnabled() || !element.isSVGElement() || !isSVGPrefixed(property))
        return nullptr;

    SVGElement& svgElement = toSVGElement(element);
    if (isSVGSMILElement(svgElement))
        return nullptr;

    String unprefixedProperty = removeSVGPrefix(property);
    QualifiedName attributeName = svgAttributeName(unprefixedProperty);
    const AttributeNameMap& supportedAttributes = getSupportedAttributes();
    auto iter = supportedAttributes.find(attributeName);
    if (iter == supportedAttributes.end() || !svgElement.propertyFromAttribute(*iter->value))
        return nullptr;

    return iter->value;
}

PassRefPtr<TimingFunction> AnimationInputHelpers::parseTimingFunction(const String& string)
{
    if (string.isEmpty())
        return nullptr;

    RefPtrWillBeRawPtr<CSSValue> value = CSSParser::parseSingleValue(CSSPropertyTransitionTimingFunction, string);
    if (!value || !value->isValueList()) {
        ASSERT(!value || value->isCSSWideKeyword());
        return nullptr;
    }
    CSSValueList* valueList = toCSSValueList(value.get());
    if (valueList->length() > 1)
        return nullptr;
    return CSSToStyleMap::mapAnimationTimingFunction(*valueList->item(0), true);
}

} // namespace blink
