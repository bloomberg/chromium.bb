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

#include "core/svg/SVGRectElement.h"

#include "core/css/StyleChangeReason.h"
#include "core/layout/svg/LayoutSVGRect.h"
#include "core/svg/SVGLength.h"

namespace blink {

inline SVGRectElement::SVGRectElement(Document& document)
    : SVGGeometryElement(SVGNames::rectTag, document),
      x_(SVGAnimatedLength::Create(this,
                                   SVGNames::xAttr,
                                   SVGLength::Create(SVGLengthMode::kWidth),
                                   CSSPropertyX)),
      y_(SVGAnimatedLength::Create(this,
                                   SVGNames::yAttr,
                                   SVGLength::Create(SVGLengthMode::kHeight),
                                   CSSPropertyY)),
      width_(SVGAnimatedLength::Create(this,
                                       SVGNames::widthAttr,
                                       SVGLength::Create(SVGLengthMode::kWidth),
                                       CSSPropertyWidth)),
      height_(
          SVGAnimatedLength::Create(this,
                                    SVGNames::heightAttr,
                                    SVGLength::Create(SVGLengthMode::kHeight),
                                    CSSPropertyHeight)),
      rx_(SVGAnimatedLength::Create(this,
                                    SVGNames::rxAttr,
                                    SVGLength::Create(SVGLengthMode::kWidth),
                                    CSSPropertyRx)),
      ry_(SVGAnimatedLength::Create(this,
                                    SVGNames::ryAttr,
                                    SVGLength::Create(SVGLengthMode::kHeight),
                                    CSSPropertyRy)) {
  AddToPropertyMap(x_);
  AddToPropertyMap(y_);
  AddToPropertyMap(width_);
  AddToPropertyMap(height_);
  AddToPropertyMap(rx_);
  AddToPropertyMap(ry_);
}

DEFINE_TRACE(SVGRectElement) {
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
  const SVGComputedStyle& svg_style = style.SvgStyle();

  float width = length_context.ValueForLength(style.Width(), style,
                                              SVGLengthMode::kWidth);
  if (width < 0)
    return path;
  float height = length_context.ValueForLength(style.Height(), style,
                                               SVGLengthMode::kHeight);
  if (height < 0)
    return path;
  if (!width && !height)
    return path;

  float x = length_context.ValueForLength(svg_style.X(), style,
                                          SVGLengthMode::kWidth);
  float y = length_context.ValueForLength(svg_style.Y(), style,
                                          SVGLengthMode::kHeight);
  float rx = length_context.ValueForLength(svg_style.Rx(), style,
                                           SVGLengthMode::kWidth);
  float ry = length_context.ValueForLength(svg_style.Ry(), style,
                                           SVGLengthMode::kHeight);
  bool has_rx = rx > 0;
  bool has_ry = ry > 0;
  if (has_rx || has_ry) {
    if (svg_style.Rx().IsAuto())
      rx = ry;
    else if (svg_style.Ry().IsAuto())
      ry = rx;

    path.AddRoundedRect(FloatRect(x, y, width, height), FloatSize(rx, ry));
    return path;
  }

  path.AddRect(FloatRect(x, y, width, height));
  return path;
}

void SVGRectElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
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
    SVGElement::InvalidationGuard invalidation_guard(this);

    InvalidateSVGPresentationAttributeStyle();
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::FromAttribute(attr_name));
    UpdateRelativeLengthsInformation();

    LayoutSVGShape* layout_object = ToLayoutSVGShape(this->GetLayoutObject());
    if (!layout_object)
      return;

    layout_object->SetNeedsShapeUpdate();
    MarkForLayoutAndParentResourceInvalidation(layout_object);

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
