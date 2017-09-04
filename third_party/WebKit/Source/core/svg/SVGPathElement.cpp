/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGPathElement.h"

#include "core/css/StyleChangeReason.h"
#include "core/layout/svg/LayoutSVGPath.h"
#include "core/svg/SVGMPathElement.h"
#include "core/svg/SVGPathQuery.h"
#include "core/svg/SVGPathUtilities.h"
#include "core/svg/SVGPointTearOff.h"

namespace blink {

inline SVGPathElement::SVGPathElement(Document& document)
    : SVGGeometryElement(SVGNames::pathTag, document),
      path_(SVGAnimatedPath::Create(this, SVGNames::dAttr, CSSPropertyD)) {
  AddToPropertyMap(path_);
}

DEFINE_TRACE(SVGPathElement) {
  visitor->Trace(path_);
  SVGGeometryElement::Trace(visitor);
}

DEFINE_NODE_FACTORY(SVGPathElement)

Path SVGPathElement::AttributePath() const {
  return path_->CurrentValue()->GetStylePath()->GetPath();
}

const StylePath* SVGPathElement::GetStylePath() const {
  if (LayoutObject* layout_object = this->GetLayoutObject()) {
    const StylePath* style_path = layout_object->StyleRef().SvgStyle().D();
    if (style_path)
      return style_path;
    return StylePath::EmptyPath();
  }
  return path_->CurrentValue()->GetStylePath();
}

float SVGPathElement::ComputePathLength() const {
  return GetStylePath()->length();
}

Path SVGPathElement::AsPath() const {
  return GetStylePath()->GetPath();
}

float SVGPathElement::getTotalLength() {
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  return SVGPathQuery(PathByteStream()).GetTotalLength();
}

SVGPointTearOff* SVGPathElement::getPointAtLength(float length) {
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  FloatPoint point = SVGPathQuery(PathByteStream()).GetPointAtLength(length);
  return SVGPointTearOff::CreateDetached(point);
}

void SVGPathElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == SVGNames::dAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    InvalidateSVGPresentationAttributeStyle();
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::FromAttribute(attr_name));

    if (LayoutSVGShape* layout_path = ToLayoutSVGShape(this->GetLayoutObject()))
      layout_path->SetNeedsShapeUpdate();

    InvalidateMPathDependencies();
    if (GetLayoutObject())
      MarkForLayoutAndParentResourceInvalidation(GetLayoutObject());

    return;
  }

  if (attr_name == SVGNames::pathLengthAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    if (GetLayoutObject())
      MarkForLayoutAndParentResourceInvalidation(GetLayoutObject());
    return;
  }

  SVGGeometryElement::SvgAttributeChanged(attr_name);
}

void SVGPathElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  SVGAnimatedPropertyBase* property = PropertyFromAttribute(name);
  if (property == path_) {
    SVGAnimatedPath* path = this->GetPath();
    // If this is a <use> instance, return the referenced path to maximize
    // geometry sharing.
    if (const SVGElement* element = CorrespondingElement())
      path = toSVGPathElement(element)->GetPath();
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            path->CssValue());
    return;
  }
  SVGGeometryElement::CollectStyleForPresentationAttribute(name, value, style);
}

void SVGPathElement::InvalidateMPathDependencies() {
  // <mpath> can only reference <path> but this dependency is not handled in
  // markForLayoutAndParentResourceInvalidation so we update any mpath
  // dependencies manually.
  if (SVGElementSet* dependencies = SetOfIncomingReferences()) {
    for (SVGElement* element : *dependencies) {
      if (isSVGMPathElement(*element))
        toSVGMPathElement(element)->TargetPathChanged();
    }
  }
}

Node::InsertionNotificationRequest SVGPathElement::InsertedInto(
    ContainerNode* root_parent) {
  SVGGeometryElement::InsertedInto(root_parent);
  InvalidateMPathDependencies();
  return kInsertionDone;
}

void SVGPathElement::RemovedFrom(ContainerNode* root_parent) {
  SVGGeometryElement::RemovedFrom(root_parent);
  InvalidateMPathDependencies();
}

FloatRect SVGPathElement::GetBBox() {
  // We want the exact bounds.
  return SVGPathElement::AsPath().BoundingRect(Path::BoundsType::kExact);
}

}  // namespace blink
