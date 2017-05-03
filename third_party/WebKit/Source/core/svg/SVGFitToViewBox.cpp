/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#include "core/svg/SVGFitToViewBox.h"

#include "core/dom/QualifiedName.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGParsingError.h"
#include "platform/geometry/FloatRect.h"
#include "platform/transforms/AffineTransform.h"

namespace blink {

class SVGAnimatedViewBoxRect : public SVGAnimatedRect {
 public:
  static SVGAnimatedRect* Create(SVGElement* context_element) {
    return new SVGAnimatedViewBoxRect(context_element);
  }

  SVGParsingError SetBaseValueAsString(const String&) override;

 protected:
  SVGAnimatedViewBoxRect(SVGElement* context_element)
      : SVGAnimatedRect(context_element, SVGNames::viewBoxAttr) {}
};

SVGParsingError SVGAnimatedViewBoxRect::SetBaseValueAsString(
    const String& value) {
  SVGParsingError parse_status = SVGAnimatedRect::SetBaseValueAsString(value);

  if (parse_status == SVGParseStatus::kNoError &&
      (BaseValue()->Width() < 0 || BaseValue()->Height() < 0)) {
    parse_status = SVGParseStatus::kNegativeValue;
    BaseValue()->SetInvalid();
  }
  return parse_status;
}

SVGFitToViewBox::SVGFitToViewBox(SVGElement* element)
    : view_box_(SVGAnimatedViewBoxRect::Create(element)),
      preserve_aspect_ratio_(SVGAnimatedPreserveAspectRatio::Create(
          element,
          SVGNames::preserveAspectRatioAttr)) {
  DCHECK(element);
  element->AddToPropertyMap(view_box_);
  element->AddToPropertyMap(preserve_aspect_ratio_);
}

DEFINE_TRACE(SVGFitToViewBox) {
  visitor->Trace(view_box_);
  visitor->Trace(preserve_aspect_ratio_);
}

AffineTransform SVGFitToViewBox::ViewBoxToViewTransform(
    const FloatRect& view_box_rect,
    SVGPreserveAspectRatio* preserve_aspect_ratio,
    float view_width,
    float view_height) {
  if (!view_box_rect.Width() || !view_box_rect.Height() || !view_width ||
      !view_height)
    return AffineTransform();

  return preserve_aspect_ratio->ComputeTransform(
      view_box_rect.X(), view_box_rect.Y(), view_box_rect.Width(),
      view_box_rect.Height(), view_width, view_height);
}

bool SVGFitToViewBox::IsKnownAttribute(const QualifiedName& attr_name) {
  return attr_name == SVGNames::viewBoxAttr ||
         attr_name == SVGNames::preserveAspectRatioAttr;
}

}  // namespace blink
