/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann
 * <zimmermann@kde.org>
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

#include "core/svg/SVGPatternElement.h"

#include "core/css/StyleChangeReason.h"
#include "core/dom/ElementTraversal.h"
#include "core/layout/svg/LayoutSVGResourcePattern.h"
#include "core/svg/PatternAttributes.h"
#include "platform/transforms/AffineTransform.h"

namespace blink {

inline SVGPatternElement::SVGPatternElement(Document& document)
    : SVGElement(SVGNames::patternTag, document),
      SVGURIReference(this),
      SVGTests(this),
      SVGFitToViewBox(this),
      x_(SVGAnimatedLength::Create(this,
                                   SVGNames::xAttr,
                                   SVGLength::Create(SVGLengthMode::kWidth))),
      y_(SVGAnimatedLength::Create(this,
                                   SVGNames::yAttr,
                                   SVGLength::Create(SVGLengthMode::kHeight))),
      width_(
          SVGAnimatedLength::Create(this,
                                    SVGNames::widthAttr,
                                    SVGLength::Create(SVGLengthMode::kWidth))),
      height_(
          SVGAnimatedLength::Create(this,
                                    SVGNames::heightAttr,
                                    SVGLength::Create(SVGLengthMode::kHeight))),
      pattern_transform_(
          SVGAnimatedTransformList::Create(this,
                                           SVGNames::patternTransformAttr,
                                           CSSPropertyTransform)),
      pattern_units_(SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>::Create(
          this,
          SVGNames::patternUnitsAttr,
          SVGUnitTypes::kSvgUnitTypeObjectboundingbox)),
      pattern_content_units_(
          SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>::Create(
              this,
              SVGNames::patternContentUnitsAttr,
              SVGUnitTypes::kSvgUnitTypeUserspaceonuse)) {
  AddToPropertyMap(x_);
  AddToPropertyMap(y_);
  AddToPropertyMap(width_);
  AddToPropertyMap(height_);
  AddToPropertyMap(pattern_transform_);
  AddToPropertyMap(pattern_units_);
  AddToPropertyMap(pattern_content_units_);
}

void SVGPatternElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  visitor->Trace(pattern_transform_);
  visitor->Trace(pattern_units_);
  visitor->Trace(pattern_content_units_);
  SVGElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
  SVGTests::Trace(visitor);
  SVGFitToViewBox::Trace(visitor);
}

DEFINE_NODE_FACTORY(SVGPatternElement)

void SVGPatternElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == SVGNames::patternTransformAttr) {
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyTransform,
        pattern_transform_->CurrentValue()->CssValue());
    return;
  }
  SVGElement::CollectStyleForPresentationAttribute(name, value, style);
}

void SVGPatternElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  bool is_length_attr =
      attr_name == SVGNames::xAttr || attr_name == SVGNames::yAttr ||
      attr_name == SVGNames::widthAttr || attr_name == SVGNames::heightAttr;

  if (attr_name == SVGNames::patternTransformAttr) {
    InvalidateSVGPresentationAttributeStyle();
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::FromAttribute(attr_name));
  }

  if (is_length_attr || attr_name == SVGNames::patternUnitsAttr ||
      attr_name == SVGNames::patternContentUnitsAttr ||
      attr_name == SVGNames::patternTransformAttr ||
      SVGFitToViewBox::IsKnownAttribute(attr_name) ||
      SVGURIReference::IsKnownAttribute(attr_name) ||
      SVGTests::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    if (is_length_attr)
      UpdateRelativeLengthsInformation();

    LayoutSVGResourceContainer* layout_object =
        ToLayoutSVGResourceContainer(this->GetLayoutObject());
    if (layout_object)
      layout_object->InvalidateCacheAndMarkForLayout();

    return;
  }

  SVGElement::SvgAttributeChanged(attr_name);
}

void SVGPatternElement::ChildrenChanged(const ChildrenChange& change) {
  SVGElement::ChildrenChanged(change);

  if (change.by_parser)
    return;

  if (LayoutObject* object = GetLayoutObject())
    object->SetNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::kChildChanged);
}

LayoutObject* SVGPatternElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutSVGResourcePattern(this);
}

static void SetPatternAttributes(const SVGPatternElement* element,
                                 PatternAttributes& attributes) {
  if (!attributes.HasX() && element->x()->IsSpecified())
    attributes.SetX(element->x()->CurrentValue());

  if (!attributes.HasY() && element->y()->IsSpecified())
    attributes.SetY(element->y()->CurrentValue());

  if (!attributes.HasWidth() && element->width()->IsSpecified())
    attributes.SetWidth(element->width()->CurrentValue());

  if (!attributes.HasHeight() && element->height()->IsSpecified())
    attributes.SetHeight(element->height()->CurrentValue());

  if (!attributes.HasViewBox() && element->HasValidViewBox())
    attributes.SetViewBox(element->viewBox()->CurrentValue()->Value());

  if (!attributes.HasPreserveAspectRatio() &&
      element->preserveAspectRatio()->IsSpecified())
    attributes.SetPreserveAspectRatio(
        element->preserveAspectRatio()->CurrentValue());

  if (!attributes.HasPatternUnits() && element->patternUnits()->IsSpecified())
    attributes.SetPatternUnits(
        element->patternUnits()->CurrentValue()->EnumValue());

  if (!attributes.HasPatternContentUnits() &&
      element->patternContentUnits()->IsSpecified())
    attributes.SetPatternContentUnits(
        element->patternContentUnits()->CurrentValue()->EnumValue());

  if (!attributes.HasPatternTransform() &&
      element->HasTransform(SVGElement::kExcludeMotionTransform)) {
    attributes.SetPatternTransform(
        element->CalculateTransform(SVGElement::kExcludeMotionTransform));
  }

  if (!attributes.HasPatternContentElement() &&
      ElementTraversal::FirstWithin(*element))
    attributes.SetPatternContentElement(element);
}

void SVGPatternElement::CollectPatternAttributes(
    PatternAttributes& attributes) const {
  HeapHashSet<Member<const SVGPatternElement>> processed_patterns;
  const SVGPatternElement* current = this;

  while (true) {
    SetPatternAttributes(current, attributes);
    processed_patterns.insert(current);

    // Respect xlink:href, take attributes from referenced element
    Node* ref_node = SVGURIReference::TargetElementFromIRIString(
        current->HrefString(), GetTreeScope());

    // Only consider attached SVG pattern elements.
    if (!IsSVGPatternElement(ref_node) || !ref_node->GetLayoutObject())
      break;

    current = ToSVGPatternElement(ref_node);

    // Cycle detection
    if (processed_patterns.Contains(current))
      break;
  }
}

AffineTransform SVGPatternElement::LocalCoordinateSpaceTransform(
    CTMScope) const {
  return CalculateTransform(SVGElement::kExcludeMotionTransform);
}

bool SVGPatternElement::SelfHasRelativeLengths() const {
  return x_->CurrentValue()->IsRelative() || y_->CurrentValue()->IsRelative() ||
         width_->CurrentValue()->IsRelative() ||
         height_->CurrentValue()->IsRelative();
}

}  // namespace blink
