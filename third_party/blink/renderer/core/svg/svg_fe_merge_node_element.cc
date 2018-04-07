/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_fe_merge_node_element.h"

#include "third_party/blink/renderer/core/svg/svg_filter_primitive_standard_attributes.h"

namespace blink {

inline SVGFEMergeNodeElement::SVGFEMergeNodeElement(Document& document)
    : SVGElement(SVGNames::feMergeNodeTag, document),
      in1_(SVGAnimatedString::Create(this, SVGNames::inAttr)) {
  AddToPropertyMap(in1_);
}

void SVGFEMergeNodeElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(in1_);
  SVGElement::Trace(visitor);
}

DEFINE_NODE_FACTORY(SVGFEMergeNodeElement)

void SVGFEMergeNodeElement::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  if (attr_name == SVGNames::inAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    InvalidateFilterPrimitiveParent(*this);
    return;
  }

  SVGElement::SvgAttributeChanged(attr_name);
}

}  // namespace blink
