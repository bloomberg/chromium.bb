/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "core/svg/SVGGradientElement.h"

#include "core/css/StyleChangeReason.h"
#include "core/dom/Attribute.h"
#include "core/dom/ElementTraversal.h"
#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/svg/GradientAttributes.h"
#include "core/svg/SVGStopElement.h"
#include "core/svg/SVGTransformList.h"

namespace blink {

template <>
const SVGEnumerationStringEntries&
GetStaticStringEntries<SVGSpreadMethodType>() {
  DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
  if (entries.IsEmpty()) {
    entries.push_back(std::make_pair(kSVGSpreadMethodPad, "pad"));
    entries.push_back(std::make_pair(kSVGSpreadMethodReflect, "reflect"));
    entries.push_back(std::make_pair(kSVGSpreadMethodRepeat, "repeat"));
  }
  return entries;
}

SVGGradientElement::SVGGradientElement(const QualifiedName& tag_name,
                                       Document& document)
    : SVGElement(tag_name, document),
      SVGURIReference(this),
      gradient_transform_(
          SVGAnimatedTransformList::Create(this,
                                           SVGNames::gradientTransformAttr,
                                           CSSPropertyTransform)),
      spread_method_(SVGAnimatedEnumeration<SVGSpreadMethodType>::Create(
          this,
          SVGNames::spreadMethodAttr,
          kSVGSpreadMethodPad)),
      gradient_units_(SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>::Create(
          this,
          SVGNames::gradientUnitsAttr,
          SVGUnitTypes::kSvgUnitTypeObjectboundingbox)) {
  AddToPropertyMap(gradient_transform_);
  AddToPropertyMap(spread_method_);
  AddToPropertyMap(gradient_units_);
}

DEFINE_TRACE(SVGGradientElement) {
  visitor->Trace(gradient_transform_);
  visitor->Trace(spread_method_);
  visitor->Trace(gradient_units_);
  SVGElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
}

void SVGGradientElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == SVGNames::gradientTransformAttr) {
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyTransform,
        gradient_transform_->CurrentValue()->CssValue());
    return;
  }
  SVGElement::CollectStyleForPresentationAttribute(name, value, style);
}

void SVGGradientElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == SVGNames::gradientTransformAttr) {
    InvalidateSVGPresentationAttributeStyle();
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::FromAttribute(attr_name));
  }

  if (attr_name == SVGNames::gradientUnitsAttr ||
      attr_name == SVGNames::gradientTransformAttr ||
      attr_name == SVGNames::spreadMethodAttr ||
      SVGURIReference::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    LayoutSVGResourceContainer* layout_object =
        ToLayoutSVGResourceContainer(this->GetLayoutObject());
    if (layout_object)
      layout_object->InvalidateCacheAndMarkForLayout();

    return;
  }

  SVGElement::SvgAttributeChanged(attr_name);
}

void SVGGradientElement::ChildrenChanged(const ChildrenChange& change) {
  SVGElement::ChildrenChanged(change);

  if (change.by_parser)
    return;

  if (LayoutObject* object = GetLayoutObject())
    object->SetNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::kChildChanged);
}

void SVGGradientElement::CollectCommonAttributes(
    GradientAttributes& attributes) const {
  if (!attributes.HasSpreadMethod() && spreadMethod()->IsSpecified())
    attributes.SetSpreadMethod(spreadMethod()->CurrentValue()->EnumValue());

  if (!attributes.HasGradientUnits() && gradientUnits()->IsSpecified())
    attributes.SetGradientUnits(gradientUnits()->CurrentValue()->EnumValue());

  if (!attributes.HasGradientTransform() &&
      HasTransform(SVGElement::kExcludeMotionTransform)) {
    attributes.SetGradientTransform(
        CalculateTransform(SVGElement::kExcludeMotionTransform));
  }

  if (!attributes.HasStops()) {
    const Vector<Gradient::ColorStop>& stops(BuildStops());
    if (!stops.IsEmpty())
      attributes.SetStops(stops);
  }
}

const SVGGradientElement* SVGGradientElement::ReferencedElement() const {
  // Respect xlink:href, take attributes from referenced element.
  Element* referenced_element =
      TargetElementFromIRIString(HrefString(), GetTreeScope());
  if (!referenced_element || !IsSVGGradientElement(*referenced_element))
    return nullptr;
  return ToSVGGradientElement(referenced_element);
}

Vector<Gradient::ColorStop> SVGGradientElement::BuildStops() const {
  Vector<Gradient::ColorStop> stops;

  float previous_offset = 0.0f;
  for (const SVGStopElement& stop :
       Traversal<SVGStopElement>::ChildrenOf(*this)) {
    // Figure out right monotonic offset.
    float offset = stop.offset()->CurrentValue()->Value();
    offset = std::min(std::max(previous_offset, offset), 1.0f);
    previous_offset = offset;

    stops.push_back(
        Gradient::ColorStop(offset, stop.StopColorIncludingOpacity()));
  }
  return stops;
}

}  // namespace blink
