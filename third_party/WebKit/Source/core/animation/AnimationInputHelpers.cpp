// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/AnimationInputHelpers.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSVariableParser.h"
#include "core/css/resolver/CSSToStyleMap.h"
#include "core/frame/Deprecation.h"
#include "core/svg/SVGElement.h"
#include "core/svg/animation/SVGSMILElement.h"
#include "core/svg_names.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

const char kSVGPrefix[] = "svg-";
const unsigned kSVGPrefixLength = sizeof(kSVGPrefix) - 1;

static bool IsSVGPrefixed(const String& property) {
  return property.StartsWith(kSVGPrefix);
}

static String RemoveSVGPrefix(const String& property) {
  DCHECK(IsSVGPrefixed(property));
  return property.Substring(kSVGPrefixLength);
}

CSSPropertyID AnimationInputHelpers::KeyframeAttributeToCSSProperty(
    const String& property,
    const Document& document) {
  if (CSSVariableParser::IsValidVariableName(property))
    return CSSPropertyVariable;

  // Disallow prefixed properties.
  if (property[0] == '-')
    return CSSPropertyInvalid;
  if (IsASCIIUpper(property[0]))
    return CSSPropertyInvalid;
  if (property == "cssFloat")
    return CSSPropertyFloat;
  if (property == "cssOffset")
    return CSSPropertyOffset;

  StringBuilder builder;
  for (size_t i = 0; i < property.length(); ++i) {
    // Disallow hyphenated properties.
    if (property[i] == '-')
      return CSSPropertyInvalid;
    if (IsASCIIUpper(property[i]))
      builder.Append('-');
    builder.Append(property[i]);
  }
  return cssPropertyID(builder.ToString());
}

CSSPropertyID AnimationInputHelpers::KeyframeAttributeToPresentationAttribute(
    const String& property,
    const Element& element) {
  if (!RuntimeEnabledFeatures::WebAnimationsSVGEnabled() ||
      !element.IsSVGElement() || !IsSVGPrefixed(property))
    return CSSPropertyInvalid;

  String unprefixed_property = RemoveSVGPrefix(property);
  if (SVGElement::IsAnimatableCSSProperty(QualifiedName(
          g_null_atom, AtomicString(unprefixed_property), g_null_atom)))
    return cssPropertyID(unprefixed_property);

  return CSSPropertyInvalid;
}

using AttributeNameMap = HashMap<QualifiedName, const QualifiedName*>;

const AttributeNameMap& GetSupportedAttributes() {
  DEFINE_STATIC_LOCAL(AttributeNameMap, supported_attributes, ());
  if (supported_attributes.IsEmpty()) {
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
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(attributes); i++) {
      DCHECK(!SVGElement::IsAnimatableCSSProperty(*attributes[i]));
      supported_attributes.Set(*attributes[i], attributes[i]);
    }
  }
  return supported_attributes;
}

QualifiedName SvgAttributeName(const String& property) {
  DCHECK(!IsSVGPrefixed(property));
  return QualifiedName(g_null_atom, AtomicString(property), g_null_atom);
}

const QualifiedName* AnimationInputHelpers::KeyframeAttributeToSVGAttribute(
    const String& property,
    Element& element) {
  if (!RuntimeEnabledFeatures::WebAnimationsSVGEnabled() ||
      !element.IsSVGElement() || !IsSVGPrefixed(property))
    return nullptr;

  SVGElement& svg_element = ToSVGElement(element);
  if (IsSVGSMILElement(svg_element))
    return nullptr;

  String unprefixed_property = RemoveSVGPrefix(property);
  QualifiedName attribute_name = SvgAttributeName(unprefixed_property);
  const AttributeNameMap& supported_attributes = GetSupportedAttributes();
  auto iter = supported_attributes.find(attribute_name);
  if (iter == supported_attributes.end() ||
      !svg_element.PropertyFromAttribute(*iter->value))
    return nullptr;

  return iter->value;
}

RefPtr<TimingFunction> AnimationInputHelpers::ParseTimingFunction(
    const String& string,
    Document* document,
    ExceptionState& exception_state) {
  if (string.IsEmpty()) {
    exception_state.ThrowTypeError("Easing may not be the empty string");
    return nullptr;
  }

  const CSSValue* value =
      CSSParser::ParseSingleValue(CSSPropertyTransitionTimingFunction, string);
  if (!value || !value->IsValueList()) {
    DCHECK(!value || value->IsCSSWideKeyword());
    if (document) {
      if (string.StartsWith("function")) {
        // Due to a bug in old versions of the web-animations-next
        // polyfill, in some circumstances the string passed in here
        // may be a Javascript function instead of the allowed values
        // from the spec
        // (http://w3c.github.io/web-animations/#dom-animationeffecttimingreadonly-easing)
        // This bug was fixed in
        // https://github.com/web-animations/web-animations-next/pull/423
        // and we want to track how often it is still being hit. The
        // linear case is special because 'linear' is the default value
        // for easing. See http://crbug.com/601672
        if (string == "function (a){return a}") {
          UseCounter::Count(*document,
                            WebFeature::kWebAnimationsEasingAsFunctionLinear);
        } else {
          UseCounter::Count(*document,
                            WebFeature::kWebAnimationsEasingAsFunctionOther);
        }
      }
    }
    exception_state.ThrowTypeError("'" + string +
                                   "' is not a valid value for easing");
    return nullptr;
  }
  const CSSValueList* value_list = ToCSSValueList(value);
  if (value_list->length() > 1) {
    exception_state.ThrowTypeError("Easing may not be set to a list of values");
    return nullptr;
  }
  return CSSToStyleMap::MapAnimationTimingFunction(value_list->Item(0), true,
                                                   document);
}

}  // namespace blink
