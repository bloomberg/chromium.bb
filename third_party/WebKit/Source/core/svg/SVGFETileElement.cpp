/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGFETileElement.h"

#include "core/svg/graphics/filters/SVGFilterBuilder.h"
#include "core/svg_names.h"
#include "platform/graphics/filters/FETile.h"

namespace blink {

inline SVGFETileElement::SVGFETileElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feTileTag, document),
      in1_(SVGAnimatedString::Create(this, SVGNames::inAttr)) {
  AddToPropertyMap(in1_);
}

DEFINE_TRACE(SVGFETileElement) {
  visitor->Trace(in1_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

DEFINE_NODE_FACTORY(SVGFETileElement)

void SVGFETileElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == SVGNames::inAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    Invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(attr_name);
}

FilterEffect* SVGFETileElement::Build(SVGFilterBuilder* filter_builder,
                                      Filter* filter) {
  FilterEffect* input1 = filter_builder->GetEffectById(
      AtomicString(in1_->CurrentValue()->Value()));
  DCHECK(input1);

  FilterEffect* effect = FETile::Create(filter);
  effect->InputEffects().push_back(input1);
  return effect;
}

}  // namespace blink
