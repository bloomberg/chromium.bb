/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "core/layout/svg/LayoutSVGResourceLinearGradient.h"

#include "core/svg/SVGLinearGradientElement.h"

namespace blink {

LayoutSVGResourceLinearGradient::LayoutSVGResourceLinearGradient(
    SVGLinearGradientElement* node)
    : LayoutSVGResourceGradient(node),
      attributes_wrapper_(LinearGradientAttributesWrapper::Create()) {}

LayoutSVGResourceLinearGradient::~LayoutSVGResourceLinearGradient() {}

bool LayoutSVGResourceLinearGradient::CollectGradientAttributes() {
  DCHECK(GetElement());
  attributes_wrapper_->Set(LinearGradientAttributes());
  return toSVGLinearGradientElement(GetElement())
      ->CollectGradientAttributes(MutableAttributes());
}

FloatPoint LayoutSVGResourceLinearGradient::StartPoint(
    const LinearGradientAttributes& attributes) const {
  return SVGLengthContext::ResolvePoint(GetElement(),
                                        attributes.GradientUnits(),
                                        *attributes.X1(), *attributes.Y1());
}

FloatPoint LayoutSVGResourceLinearGradient::EndPoint(
    const LinearGradientAttributes& attributes) const {
  return SVGLengthContext::ResolvePoint(GetElement(),
                                        attributes.GradientUnits(),
                                        *attributes.X2(), *attributes.Y2());
}

RefPtr<Gradient> LayoutSVGResourceLinearGradient::BuildGradient() const {
  const LinearGradientAttributes& attributes = this->Attributes();
  RefPtr<Gradient> gradient = Gradient::CreateLinear(
      StartPoint(attributes), EndPoint(attributes),
      PlatformSpreadMethodFromSVGType(attributes.SpreadMethod()),
      Gradient::ColorInterpolation::kUnpremultiplied);
  gradient->AddColorStops(attributes.Stops());
  return gradient;
}

}  // namespace blink
