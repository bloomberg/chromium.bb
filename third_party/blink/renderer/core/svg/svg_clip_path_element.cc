/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_clip_path_element.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"

namespace blink {

inline SVGClipPathElement::SVGClipPathElement(Document& document)
    : SVGGraphicsElement(SVGNames::clipPathTag, document),
      clip_path_units_(
          SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>::Create(
              this,
              SVGNames::clipPathUnitsAttr,
              SVGUnitTypes::kSvgUnitTypeUserspaceonuse)) {
  AddToPropertyMap(clip_path_units_);
}

void SVGClipPathElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(clip_path_units_);
  SVGGraphicsElement::Trace(visitor);
}

DEFINE_NODE_FACTORY(SVGClipPathElement)

void SVGClipPathElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == SVGNames::clipPathUnitsAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    LayoutSVGResourceContainer* layout_object =
        ToLayoutSVGResourceContainer(this->GetLayoutObject());
    if (layout_object)
      layout_object->InvalidateCacheAndMarkForLayout();
    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(attr_name);
}

void SVGClipPathElement::ChildrenChanged(const ChildrenChange& change) {
  SVGGraphicsElement::ChildrenChanged(change);

  if (change.by_parser)
    return;

  if (LayoutObject* object = GetLayoutObject())
    object->SetNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::kChildChanged);
}

LayoutObject* SVGClipPathElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutSVGResourceClipper(this);
}

}  // namespace blink
