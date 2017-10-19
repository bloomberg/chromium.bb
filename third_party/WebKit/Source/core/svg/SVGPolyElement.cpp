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

#include "core/svg/SVGPolyElement.h"

#include "core/layout/svg/LayoutSVGShape.h"
#include "core/svg/SVGAnimatedPointList.h"
#include "core/svg/SVGParserUtilities.h"

namespace blink {

SVGPolyElement::SVGPolyElement(const QualifiedName& tag_name,
                               Document& document)
    : SVGGeometryElement(tag_name, document),
      points_(SVGAnimatedPointList::Create(this,
                                           SVGNames::pointsAttr,
                                           SVGPointList::Create())) {
  AddToPropertyMap(points_);
}

void SVGPolyElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(points_);
  SVGGeometryElement::Trace(visitor);
}

Path SVGPolyElement::AsPathFromPoints() const {
  Path path;

  SVGPointList* points_value = Points()->CurrentValue();
  if (points_value->IsEmpty())
    return path;

  SVGPointList::ConstIterator it = points_value->begin();
  SVGPointList::ConstIterator it_end = points_value->end();
  DCHECK(it != it_end);
  path.MoveTo(it->Value());
  ++it;

  for (; it != it_end; ++it)
    path.AddLineTo(it->Value());

  return path;
}

void SVGPolyElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == SVGNames::pointsAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    LayoutSVGShape* layout_object = ToLayoutSVGShape(this->GetLayoutObject());
    if (!layout_object)
      return;

    layout_object->SetNeedsShapeUpdate();
    MarkForLayoutAndParentResourceInvalidation(layout_object);
    return;
  }

  SVGGeometryElement::SvgAttributeChanged(attr_name);
}

}  // namespace blink
