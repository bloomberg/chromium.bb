/*
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2010 Zoltan Herczeg
 * Copyright (C) 2011 Renata Hodovan (reni@webkit.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/svg/LayoutSVGResourceFilterPrimitive.h"

#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"

namespace blink {

void LayoutSVGResourceFilterPrimitive::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style) {
  LayoutSVGHiddenContainer::StyleDidChange(diff, old_style);

  if (!old_style)
    return;
  DCHECK(GetElement());
  SVGFilterPrimitiveStandardAttributes& element =
      ToSVGFilterPrimitiveStandardAttributes(*GetElement());
  const SVGComputedStyle& new_style = StyleRef().SvgStyle();
  if (IsSVGFEFloodElement(element) || IsSVGFEDropShadowElement(element)) {
    if (new_style.FloodColor() != old_style->SvgStyle().FloodColor())
      element.PrimitiveAttributeChanged(SVGNames::flood_colorAttr);
    if (new_style.FloodOpacity() != old_style->SvgStyle().FloodOpacity())
      element.PrimitiveAttributeChanged(SVGNames::flood_opacityAttr);
  } else if (IsSVGFEDiffuseLightingElement(element) ||
             IsSVGFESpecularLightingElement(element)) {
    if (new_style.LightingColor() != old_style->SvgStyle().LightingColor())
      element.PrimitiveAttributeChanged(SVGNames::lighting_colorAttr);
  }
  if (new_style.ColorInterpolationFilters() !=
      old_style->SvgStyle().ColorInterpolationFilters()) {
    element.PrimitiveAttributeChanged(
        SVGNames::color_interpolation_filtersAttr);
  }
}

}  // namespace blink
