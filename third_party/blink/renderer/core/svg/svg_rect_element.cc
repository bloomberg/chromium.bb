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

#include "third_party/blink/renderer/core/svg/svg_rect_element.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_rect.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"

namespace blink {

inline SVGRectElement::SVGRectElement(Document& document)
    : SVGGeometryElement(SVGNames::rectTag, document),
      x_(SVGAnimatedLength::Create(this,
                                   SVGNames::xAttr,
                                   SVGLengthMode::kWidth,
                                   SVGLength::Initial::kUnitlessZero,
                                   CSSPropertyX)),
      y_(SVGAnimatedLength::Create(this,
                                   SVGNames::yAttr,
                                   SVGLengthMode::kHeight,
                                   SVGLength::Initial::kUnitlessZero,
                                   CSSPropertyY)),
      width_(SVGAnimatedLength::Create(this,
                                       SVGNames::widthAttr,
                                       SVGLengthMode::kWidth,
                                       SVGLength::Initial::kUnitlessZero,
                                       CSSPropertyWidth)),
      height_(SVGAnimatedLength::Create(this,
                                        SVGNames::heightAttr,
                                        SVGLengthMode::kHeight,
                                        SVGLength::Initial::kUnitlessZero,
                                        CSSPropertyHeight)),
      rx_(SVGAnimatedLength::Create(this,
                                    SVGNames::rxAttr,
                                    SVGLengthMode::kWidth,
                                    SVGLength::Initial::kUnitlessZero,
                                    CSSPropertyRx)),
      ry_(SVGAnimatedLength::Create(this,
                                    SVGNames::ryAttr,
                                    SVGLengthMode::kHeight,
                                    SVGLength::Initial::kUnitlessZero,
                                    CSSPropertyRy)) {
  AddToPropertyMap(x_);
  AddToPropertyMap(y_);
  AddToPropertyMap(width_);
  AddToPropertyMap(height_);
  AddToPropertyMap(rx_);
  AddToPropertyMap(ry_);
}

void SVGRectElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  visitor->Trace(rx_);
  visitor->Trace(ry_);
  SVGGeometryElement::Trace(visitor);
}

DEFINE_NODE_FACTORY(SVGRectElement)

Path SVGRectElement::AsPath() const {
  Path path;

  SVGLengthContext length_context(this);
  DCHECK(GetLayoutObject());
  const ComputedStyle& style = GetLayoutObject()->StyleRef();

  FloatSize size(ToFloatSize(
      length_context.ResolveLengthPair(style.Width(), style.Height(), style)));
  if (size.Width() < 0 || size.Height() < 0 ||
      (!size.Width() && !size.Height()))
    return path;

  const SVGComputedStyle& svg_style = style.SvgStyle();
  FloatRect rect(
      length_context.ResolveLengthPair(svg_style.X(), svg_style.Y(), style),
      size);
  FloatPoint radii(
      length_context.ResolveLengthPair(svg_style.Rx(), svg_style.Ry(), style));
  if (radii.X() > 0 || radii.Y() > 0) {
    if (svg_style.Rx().IsAuto())
      radii.SetX(radii.Y());
    else if (svg_style.Ry().IsAuto())
      radii.SetY(radii.X());

    path.AddRoundedRect(rect, ToFloatSize(radii));
  } else {
    path.AddRect(rect);
  }
  return path;
}

void SVGRectElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  SVGAnimatedPropertyBase* property = PropertyFromAttribute(name);
  if (property == x_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            x_->CssValue());
  } else if (property == y_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            y_->CssValue());
  } else if (property == width_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            width_->CssValue());
  } else if (property == height_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            height_->CssValue());
  } else if (property == rx_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            rx_->CssValue());
  } else if (property == ry_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            ry_->CssValue());
  } else {
    SVGGeometryElement::CollectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

void SVGRectElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == SVGNames::xAttr || attr_name == SVGNames::yAttr ||
      attr_name == SVGNames::widthAttr || attr_name == SVGNames::heightAttr ||
      attr_name == SVGNames::rxAttr || attr_name == SVGNames::ryAttr) {
    UpdateRelativeLengthsInformation();
    GeometryPresentationAttributeChanged(attr_name);
    return;
  }

  SVGGeometryElement::SvgAttributeChanged(attr_name);
}

bool SVGRectElement::SelfHasRelativeLengths() const {
  return x_->CurrentValue()->IsRelative() || y_->CurrentValue()->IsRelative() ||
         width_->CurrentValue()->IsRelative() ||
         height_->CurrentValue()->IsRelative() ||
         rx_->CurrentValue()->IsRelative() || ry_->CurrentValue()->IsRelative();
}

LayoutObject* SVGRectElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutSVGRect(this);
}

}  // namespace blink
