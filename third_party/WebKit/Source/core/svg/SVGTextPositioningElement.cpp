/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGTextPositioningElement.h"

#include "core/layout/svg/LayoutSVGText.h"
#include "core/svg/SVGLengthList.h"
#include "core/svg/SVGNumberList.h"
#include "core/svg_names.h"

namespace blink {

SVGTextPositioningElement::SVGTextPositioningElement(
    const QualifiedName& tag_name,
    Document& document)
    : SVGTextContentElement(tag_name, document),
      x_(SVGAnimatedLengthList::Create(
          this,
          SVGNames::xAttr,
          SVGLengthList::Create(SVGLengthMode::kWidth))),
      y_(SVGAnimatedLengthList::Create(
          this,
          SVGNames::yAttr,
          SVGLengthList::Create(SVGLengthMode::kHeight))),
      dx_(SVGAnimatedLengthList::Create(
          this,
          SVGNames::dxAttr,
          SVGLengthList::Create(SVGLengthMode::kWidth))),
      dy_(SVGAnimatedLengthList::Create(
          this,
          SVGNames::dyAttr,
          SVGLengthList::Create(SVGLengthMode::kHeight))),
      rotate_(SVGAnimatedNumberList::Create(this, SVGNames::rotateAttr)) {
  AddToPropertyMap(x_);
  AddToPropertyMap(y_);
  AddToPropertyMap(dx_);
  AddToPropertyMap(dy_);
  AddToPropertyMap(rotate_);
}

void SVGTextPositioningElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(dx_);
  visitor->Trace(dy_);
  visitor->Trace(rotate_);
  SVGTextContentElement::Trace(visitor);
}

void SVGTextPositioningElement::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  bool update_relative_lengths =
      attr_name == SVGNames::xAttr || attr_name == SVGNames::yAttr ||
      attr_name == SVGNames::dxAttr || attr_name == SVGNames::dyAttr;

  if (update_relative_lengths)
    UpdateRelativeLengthsInformation();

  if (update_relative_lengths || attr_name == SVGNames::rotateAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    LayoutObject* layout_object = this->GetLayoutObject();
    if (!layout_object)
      return;

    if (LayoutSVGText* text_layout_object =
            LayoutSVGText::LocateLayoutSVGTextAncestor(layout_object))
      text_layout_object->SetNeedsPositioningValuesUpdate();
    MarkForLayoutAndParentResourceInvalidation(layout_object);
    return;
  }

  SVGTextContentElement::SvgAttributeChanged(attr_name);
}

}  // namespace blink
