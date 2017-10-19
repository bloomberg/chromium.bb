/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "core/layout/svg/LayoutSVGResourceFilter.h"

#include "core/svg/SVGFilterElement.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"

namespace blink {

void FilterData::Trace(blink::Visitor* visitor) {
  visitor->Trace(last_effect);
  visitor->Trace(node_map);
}

void FilterData::Dispose() {
  node_map = nullptr;
  if (last_effect)
    last_effect->DisposeImageFiltersRecursive();
  last_effect = nullptr;
}

LayoutSVGResourceFilter::LayoutSVGResourceFilter(SVGFilterElement* node)
    : LayoutSVGResourceContainer(node) {}

LayoutSVGResourceFilter::~LayoutSVGResourceFilter() {}

void LayoutSVGResourceFilter::DisposeFilterMap() {
  for (auto& entry : filter_)
    entry.value->Dispose();
  filter_.clear();
}

void LayoutSVGResourceFilter::WillBeDestroyed() {
  DisposeFilterMap();
  LayoutSVGResourceContainer::WillBeDestroyed();
}

bool LayoutSVGResourceFilter::IsChildAllowed(LayoutObject* child,
                                             const ComputedStyle&) const {
  return child->IsSVGResourceFilterPrimitive();
}

void LayoutSVGResourceFilter::RemoveAllClientsFromCache(
    bool mark_for_invalidation) {
  // LayoutSVGResourceFilter::removeClientFromCache will be called for
  // all clients through markAllClientsForInvalidation so no explicit
  // display item invalidation is needed here.
  DisposeFilterMap();
  MarkAllClientsForInvalidation(mark_for_invalidation
                                    ? kLayoutAndBoundariesInvalidation
                                    : kParentOnlyInvalidation);
}

void LayoutSVGResourceFilter::RemoveClientFromCache(
    LayoutObject* client,
    bool mark_for_invalidation) {
  DCHECK(client);

  auto entry = filter_.find(client);
  bool filter_cached = entry != filter_.end();
  if (filter_cached) {
    entry->value->Dispose();
    filter_.erase(entry);
  }

  // If the filter has a cached subtree, invalidate the associated display item.
  if (mark_for_invalidation && filter_cached)
    MarkClientForInvalidation(client, kPaintInvalidation);

  MarkClientForInvalidation(client, mark_for_invalidation
                                        ? kBoundariesInvalidation
                                        : kParentOnlyInvalidation);
}

FloatRect LayoutSVGResourceFilter::ResourceBoundingBox(
    const LayoutObject* object) {
  if (SVGFilterElement* element = ToSVGFilterElement(this->GetElement()))
    return SVGLengthContext::ResolveRectangle<SVGFilterElement>(
        element, element->filterUnits()->CurrentValue()->EnumValue(),
        object->ObjectBoundingBox());

  return FloatRect();
}

SVGUnitTypes::SVGUnitType LayoutSVGResourceFilter::FilterUnits() const {
  return ToSVGFilterElement(GetElement())
      ->filterUnits()
      ->CurrentValue()
      ->EnumValue();
}

SVGUnitTypes::SVGUnitType LayoutSVGResourceFilter::PrimitiveUnits() const {
  return ToSVGFilterElement(GetElement())
      ->primitiveUnits()
      ->CurrentValue()
      ->EnumValue();
}

void LayoutSVGResourceFilter::PrimitiveAttributeChanged(
    SVGFilterPrimitiveStandardAttributes& primitive,
    const QualifiedName& attribute) {
  LayoutObject* object = primitive.GetLayoutObject();

  for (auto& filter : filter_) {
    FilterData* filter_data = filter.value.Get();
    if (filter_data->state_ != FilterData::kReadyToPaint)
      continue;

    SVGFilterGraphNodeMap* node_map = filter_data->node_map.Get();
    FilterEffect* effect = node_map->EffectByRenderer(object);
    if (!effect)
      continue;
    // Since all effects shares the same attribute value, all
    // or none of them will be changed.
    if (!primitive.SetFilterEffectAttribute(effect, attribute))
      return;
    node_map->InvalidateDependentEffects(effect);

    // Issue paint invalidations for the image on the screen.
    MarkClientForInvalidation(filter.key, kPaintInvalidation);
  }
  NotifyContentChanged();
}

}  // namespace blink
