/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "core/svg/SVGFEMergeElement.h"

#include "core/dom/ElementTraversal.h"
#include "core/svg/SVGFEMergeNodeElement.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"
#include "core/svg_names.h"
#include "platform/graphics/filters/FEMerge.h"

namespace blink {

inline SVGFEMergeElement::SVGFEMergeElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feMergeTag, document) {}

DEFINE_NODE_FACTORY(SVGFEMergeElement)

FilterEffect* SVGFEMergeElement::Build(SVGFilterBuilder* filter_builder,
                                       Filter* filter) {
  FilterEffect* effect = FEMerge::Create(filter);
  FilterEffectVector& merge_inputs = effect->InputEffects();
  for (SVGFEMergeNodeElement& merge_node :
       Traversal<SVGFEMergeNodeElement>::ChildrenOf(*this)) {
    FilterEffect* merge_effect = filter_builder->GetEffectById(
        AtomicString(merge_node.in1()->CurrentValue()->Value()));
    DCHECK(merge_effect);
    merge_inputs.push_back(merge_effect);
  }
  return effect;
}

bool SVGFEMergeElement::TaintsOrigin(bool inputs_taint_origin) const {
  return inputs_taint_origin;
}

}  // namespace blink
