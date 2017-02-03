// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGInterpolationTypesMap.h"

#include "core/HTMLNames.h"
#include "core/animation/SVGAngleInterpolationType.h"
#include "core/animation/SVGIntegerInterpolationType.h"
#include "core/animation/SVGIntegerOptionalIntegerInterpolationType.h"
#include "core/animation/SVGLengthInterpolationType.h"
#include "core/animation/SVGLengthListInterpolationType.h"
#include "core/animation/SVGNumberInterpolationType.h"
#include "core/animation/SVGNumberListInterpolationType.h"
#include "core/animation/SVGNumberOptionalNumberInterpolationType.h"
#include "core/animation/SVGPathInterpolationType.h"
#include "core/animation/SVGPointListInterpolationType.h"
#include "core/animation/SVGRectInterpolationType.h"
#include "core/animation/SVGTransformListInterpolationType.h"
#include "core/animation/SVGValueInterpolationType.h"
#include "core/css/CSSPropertyMetadata.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

const InterpolationTypes& SVGInterpolationTypesMap::get(
    const PropertyHandle& property) const {
  using ApplicableTypesMap =
      HashMap<PropertyHandle, std::unique_ptr<const InterpolationTypes>>;
  DEFINE_STATIC_LOCAL(ApplicableTypesMap, applicableTypesMap, ());
  auto entry = applicableTypesMap.find(property);
  if (entry != applicableTypesMap.end())
    return *entry->value.get();

  std::unique_ptr<InterpolationTypes> applicableTypes =
      WTF::makeUnique<InterpolationTypes>();

  const QualifiedName& attribute = property.svgAttribute();
  if (attribute == SVGNames::orientAttr) {
    applicableTypes->push_back(
        WTF::makeUnique<SVGAngleInterpolationType>(attribute));
  } else if (attribute == SVGNames::numOctavesAttr ||
             attribute == SVGNames::targetXAttr ||
             attribute == SVGNames::targetYAttr) {
    applicableTypes->push_back(
        WTF::makeUnique<SVGIntegerInterpolationType>(attribute));
  } else if (attribute == SVGNames::orderAttr) {
    applicableTypes->push_back(
        WTF::makeUnique<SVGIntegerOptionalIntegerInterpolationType>(attribute));
  } else if (attribute == SVGNames::cxAttr || attribute == SVGNames::cyAttr ||
             attribute == SVGNames::fxAttr || attribute == SVGNames::fyAttr ||
             attribute == SVGNames::heightAttr ||
             attribute == SVGNames::markerHeightAttr ||
             attribute == SVGNames::markerWidthAttr ||
             attribute == SVGNames::rAttr || attribute == SVGNames::refXAttr ||
             attribute == SVGNames::refYAttr || attribute == SVGNames::rxAttr ||
             attribute == SVGNames::ryAttr ||
             attribute == SVGNames::startOffsetAttr ||
             attribute == SVGNames::textLengthAttr ||
             attribute == SVGNames::widthAttr ||
             attribute == SVGNames::x1Attr || attribute == SVGNames::x2Attr ||
             attribute == SVGNames::y1Attr || attribute == SVGNames::y2Attr) {
    applicableTypes->push_back(
        WTF::makeUnique<SVGLengthInterpolationType>(attribute));
  } else if (attribute == SVGNames::dxAttr || attribute == SVGNames::dyAttr) {
    applicableTypes->push_back(
        WTF::makeUnique<SVGNumberInterpolationType>(attribute));
    applicableTypes->push_back(
        WTF::makeUnique<SVGLengthListInterpolationType>(attribute));
  } else if (attribute == SVGNames::xAttr || attribute == SVGNames::yAttr) {
    applicableTypes->push_back(
        WTF::makeUnique<SVGLengthInterpolationType>(attribute));
    applicableTypes->push_back(
        WTF::makeUnique<SVGLengthListInterpolationType>(attribute));
  } else if (attribute == SVGNames::amplitudeAttr ||
             attribute == SVGNames::azimuthAttr ||
             attribute == SVGNames::biasAttr ||
             attribute == SVGNames::diffuseConstantAttr ||
             attribute == SVGNames::divisorAttr ||
             attribute == SVGNames::elevationAttr ||
             attribute == SVGNames::exponentAttr ||
             attribute == SVGNames::interceptAttr ||
             attribute == SVGNames::k1Attr || attribute == SVGNames::k2Attr ||
             attribute == SVGNames::k3Attr || attribute == SVGNames::k4Attr ||
             attribute == SVGNames::limitingConeAngleAttr ||
             attribute == SVGNames::offsetAttr ||
             attribute == SVGNames::pathLengthAttr ||
             attribute == SVGNames::pointsAtXAttr ||
             attribute == SVGNames::pointsAtYAttr ||
             attribute == SVGNames::pointsAtZAttr ||
             attribute == SVGNames::scaleAttr ||
             attribute == SVGNames::seedAttr ||
             attribute == SVGNames::slopeAttr ||
             attribute == SVGNames::specularConstantAttr ||
             attribute == SVGNames::specularExponentAttr ||
             attribute == SVGNames::surfaceScaleAttr ||
             attribute == SVGNames::zAttr) {
    applicableTypes->push_back(
        WTF::makeUnique<SVGNumberInterpolationType>(attribute));
  } else if (attribute == SVGNames::kernelMatrixAttr ||
             attribute == SVGNames::rotateAttr ||
             attribute == SVGNames::tableValuesAttr ||
             attribute == SVGNames::valuesAttr) {
    applicableTypes->push_back(
        WTF::makeUnique<SVGNumberListInterpolationType>(attribute));
  } else if (attribute == SVGNames::baseFrequencyAttr ||
             attribute == SVGNames::kernelUnitLengthAttr ||
             attribute == SVGNames::radiusAttr ||
             attribute == SVGNames::stdDeviationAttr) {
    applicableTypes->push_back(
        WTF::makeUnique<SVGNumberOptionalNumberInterpolationType>(attribute));
  } else if (attribute == SVGNames::dAttr) {
    applicableTypes->push_back(
        WTF::makeUnique<SVGPathInterpolationType>(attribute));
  } else if (attribute == SVGNames::pointsAttr) {
    applicableTypes->push_back(
        WTF::makeUnique<SVGPointListInterpolationType>(attribute));
  } else if (attribute == SVGNames::viewBoxAttr) {
    applicableTypes->push_back(
        WTF::makeUnique<SVGRectInterpolationType>(attribute));
  } else if (attribute == SVGNames::gradientTransformAttr ||
             attribute == SVGNames::patternTransformAttr ||
             attribute == SVGNames::transformAttr) {
    applicableTypes->push_back(
        WTF::makeUnique<SVGTransformListInterpolationType>(attribute));
  } else if (attribute == HTMLNames::classAttr ||
             attribute == SVGNames::clipPathUnitsAttr ||
             attribute == SVGNames::edgeModeAttr ||
             attribute == SVGNames::filterUnitsAttr ||
             attribute == SVGNames::gradientUnitsAttr ||
             attribute == SVGNames::hrefAttr || attribute == SVGNames::inAttr ||
             attribute == SVGNames::in2Attr ||
             attribute == SVGNames::lengthAdjustAttr ||
             attribute == SVGNames::markerUnitsAttr ||
             attribute == SVGNames::maskContentUnitsAttr ||
             attribute == SVGNames::maskUnitsAttr ||
             attribute == SVGNames::methodAttr ||
             attribute == SVGNames::modeAttr ||
             attribute == SVGNames::operatorAttr ||
             attribute == SVGNames::patternContentUnitsAttr ||
             attribute == SVGNames::patternUnitsAttr ||
             attribute == SVGNames::preserveAlphaAttr ||
             attribute == SVGNames::preserveAspectRatioAttr ||
             attribute == SVGNames::primitiveUnitsAttr ||
             attribute == SVGNames::resultAttr ||
             attribute == SVGNames::spacingAttr ||
             attribute == SVGNames::spreadMethodAttr ||
             attribute == SVGNames::stitchTilesAttr ||
             attribute == SVGNames::targetAttr ||
             attribute == SVGNames::typeAttr ||
             attribute == SVGNames::xChannelSelectorAttr ||
             attribute == SVGNames::yChannelSelectorAttr) {
    // Use default SVGValueInterpolationType.
  } else {
    NOTREACHED();
  }

  applicableTypes->push_back(
      WTF::makeUnique<SVGValueInterpolationType>(attribute));

  auto addResult =
      applicableTypesMap.insert(property, std::move(applicableTypes));
  return *addResult.storedValue->value.get();
}

}  // namespace blink
