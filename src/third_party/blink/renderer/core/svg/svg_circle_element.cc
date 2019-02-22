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

#include "third_party/blink/renderer/core/svg/svg_circle_element.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_ellipse.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"

namespace blink {

inline SVGCircleElement::SVGCircleElement(Document& document)
    : SVGGeometryElement(SVGNames::circleTag, document),
      cx_(SVGAnimatedLength::Create(this,
                                    SVGNames::cxAttr,
                                    SVGLengthMode::kWidth,
                                    SVGLength::Initial::kUnitlessZero,
                                    CSSPropertyCx)),
      cy_(SVGAnimatedLength::Create(this,
                                    SVGNames::cyAttr,
                                    SVGLengthMode::kHeight,
                                    SVGLength::Initial::kUnitlessZero,
                                    CSSPropertyCy)),
      r_(SVGAnimatedLength::Create(this,
                                   SVGNames::rAttr,
                                   SVGLengthMode::kOther,
                                   SVGLength::Initial::kUnitlessZero,
                                   CSSPropertyR)) {
  AddToPropertyMap(cx_);
  AddToPropertyMap(cy_);
  AddToPropertyMap(r_);
}

void SVGCircleElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(cx_);
  visitor->Trace(cy_);
  visitor->Trace(r_);
  SVGGeometryElement::Trace(visitor);
}

DEFINE_NODE_FACTORY(SVGCircleElement)

Path SVGCircleElement::AsPath() const {
  Path path;

  SVGLengthContext length_context(this);
  DCHECK(GetLayoutObject());
  const ComputedStyle& style = GetLayoutObject()->StyleRef();
  const SVGComputedStyle& svg_style = style.SvgStyle();

  float r = length_context.ValueForLength(svg_style.R(), style,
                                          SVGLengthMode::kOther);
  if (r > 0) {
    FloatPoint center(length_context.ResolveLengthPair(svg_style.Cx(),
                                                       svg_style.Cy(), style));
    FloatSize radii(r, r);
    path.AddEllipse(FloatRect(center - radii, radii.ScaledBy(2)));
  }
  return path;
}

void SVGCircleElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  SVGAnimatedPropertyBase* property = PropertyFromAttribute(name);
  if (property == cx_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            cx_->CssValue());
  } else if (property == cy_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            cy_->CssValue());
  } else if (property == r_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            r_->CssValue());
  } else {
    SVGGeometryElement::CollectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

void SVGCircleElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == SVGNames::rAttr || attr_name == SVGNames::cxAttr ||
      attr_name == SVGNames::cyAttr) {
    UpdateRelativeLengthsInformation();
    GeometryPresentationAttributeChanged(attr_name);
    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(attr_name);
}

bool SVGCircleElement::SelfHasRelativeLengths() const {
  return cx_->CurrentValue()->IsRelative() ||
         cy_->CurrentValue()->IsRelative() || r_->CurrentValue()->IsRelative();
}

LayoutObject* SVGCircleElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutSVGEllipse(this);
}

}  // namespace blink
