/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_foreign_object_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

inline SVGForeignObjectElement::SVGForeignObjectElement(Document& document)
    : SVGGraphicsElement(SVGNames::foreignObjectTag, document),
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
                                    CSSPropertyHeight)) {
  AddToPropertyMap(x_);
  AddToPropertyMap(y_);
  AddToPropertyMap(width_);
  AddToPropertyMap(height_);

  UseCounter::Count(document, WebFeature::kSVGForeignObjectElement);
}

void SVGForeignObjectElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  SVGGraphicsElement::Trace(visitor);
}

DEFINE_NODE_FACTORY(SVGForeignObjectElement)

void SVGForeignObjectElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  SVGAnimatedPropertyBase* property = PropertyFromAttribute(name);
  if (property == width_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            width_->CssValue());
  } else if (property == height_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            height_->CssValue());
  } else if (property == x_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            x_->CssValue());
  } else if (property == y_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            y_->CssValue());
  } else {
    SVGGraphicsElement::CollectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

void SVGForeignObjectElement::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  bool is_width_height_attribute =
      attr_name == SVGNames::widthAttr || attr_name == SVGNames::heightAttr;
  bool is_xy_attribute =
      attr_name == SVGNames::xAttr || attr_name == SVGNames::yAttr;

  if (is_xy_attribute || is_width_height_attribute) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    InvalidateSVGPresentationAttributeStyle();
    SetNeedsStyleRecalc(
        kLocalStyleChange,
        is_width_height_attribute
            ? StyleChangeReasonForTracing::Create(
                  StyleChangeReason::kSVGContainerSizeChange)
            : StyleChangeReasonForTracing::FromAttribute(attr_name));

    UpdateRelativeLengthsInformation();
    if (LayoutObject* layout_object = GetLayoutObject())
      MarkForLayoutAndParentResourceInvalidation(*layout_object);

    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(attr_name);
}

LayoutObject* SVGForeignObjectElement::CreateLayoutObject(
    const ComputedStyle&) {
  return new LayoutSVGForeignObject(this);
}

bool SVGForeignObjectElement::LayoutObjectIsNeeded(
    const ComputedStyle& style) const {
  // Suppress foreignObject layoutObjects in SVG hidden containers.
  // (https://bugs.webkit.org/show_bug.cgi?id=87297)
  // Note that we currently do not support foreignObject instantiation via
  // <use>, hence it is safe to use parentElement() here. If that changes, this
  // method should be updated to use parentOrShadowHostElement() instead.
  Element* ancestor = parentElement();
  while (ancestor && ancestor->IsSVGElement()) {
    if (ancestor->GetLayoutObject() &&
        ancestor->GetLayoutObject()->IsSVGHiddenContainer())
      return false;

    ancestor = ancestor->parentElement();
  }

  return SVGGraphicsElement::LayoutObjectIsNeeded(style);
}

bool SVGForeignObjectElement::SelfHasRelativeLengths() const {
  return x_->CurrentValue()->IsRelative() || y_->CurrentValue()->IsRelative() ||
         width_->CurrentValue()->IsRelative() ||
         height_->CurrentValue()->IsRelative();
}

}  // namespace blink
