/*
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

#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"

#include <memory>
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_paint_server.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cycle_solver.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"

namespace blink {

SVGResourcesCache::SVGResourcesCache() = default;

SVGResourcesCache::~SVGResourcesCache() = default;

SVGResources* SVGResourcesCache::AddResourcesFromLayoutObject(
    LayoutObject& object,
    const ComputedStyle& style) {
  DCHECK(!cache_.Contains(&object));

  // Build a list of all resources associated with the passed LayoutObject.
  std::unique_ptr<SVGResources> new_resources =
      SVGResources::BuildResources(object, style);
  if (!new_resources)
    return nullptr;

  // Put object in cache.
  SVGResources* resources =
      cache_.Set(&object, std::move(new_resources)).stored_value->value.get();

  // Run cycle-detection _afterwards_, so self-references can be caught as well.
  HashSet<LayoutSVGResourceContainer*> resource_set;
  resources->BuildSetOfResources(resource_set);

  SVGResourcesCycleSolver solver;
  for (auto* resource_container : resource_set) {
    if (resource_container->FindCycle(solver))
      resources->ClearReferencesTo(resource_container);
  }
  return resources;
}

bool SVGResourcesCache::RemoveResourcesFromLayoutObject(LayoutObject& object) {
  std::unique_ptr<SVGResources> resources = cache_.Take(&object);
  return !!resources;
}

SVGResourcesCache::ResourceUpdateInfo
SVGResourcesCache::UpdateResourcesFromLayoutObject(
    LayoutObject& object,
    const ComputedStyle& new_style) {
  std::unique_ptr<SVGResources> old_resources = cache_.Take(&object);
  SVGResources* new_resources = AddResourcesFromLayoutObject(object, new_style);
  return {
      old_resources || new_resources,
      SVGResources::DifferenceNeedsLayout(old_resources.get(), new_resources)};
}

static inline SVGResourcesCache& ResourcesCache(Document& document) {
  return document.AccessSVGExtensions().ResourcesCache();
}

SVGResources* SVGResourcesCache::CachedResourcesForLayoutObject(
    const LayoutObject& layout_object) {
  return ResourcesCache(layout_object.GetDocument()).cache_.at(&layout_object);
}

void SVGResourcesCache::ClientLayoutChanged(LayoutObject& object) {
  SVGResources* resources = CachedResourcesForLayoutObject(object);
  if (!resources)
    return;
  // Invalidate the resources if either the LayoutObject itself changed,
  // or we have filter resources, which could depend on the layout of children.
  if (!object.SelfNeedsLayout() && !resources->Filter())
    return;
  SVGElementResourceClient* client = SVGResources::GetClient(object);
  InvalidationModeMask invalidation_flags =
      resources->RemoveClientFromCacheAffectingObjectBounds(*client);
  if (LayoutSVGResourcePaintServer* fill = resources->Fill()) {
    fill->RemoveClientFromCache(*client);
    invalidation_flags |= SVGResourceClient::kPaintInvalidation;
  }
  if (LayoutSVGResourcePaintServer* stroke = resources->Stroke()) {
    stroke->RemoveClientFromCache(*client);
    invalidation_flags |= SVGResourceClient::kPaintInvalidation;
  }
  if (invalidation_flags) {
    LayoutSVGResourceContainer::MarkClientForInvalidation(object,
                                                          invalidation_flags);
  }
}

static inline bool LayoutObjectCanHaveResources(
    const LayoutObject& layout_object) {
  return layout_object.GetNode() && layout_object.GetNode()->IsSVGElement() &&
         !layout_object.IsSVGInlineText();
}

static inline bool IsLayoutObjectOfResourceContainer(
    const LayoutObject& layout_object) {
  const LayoutObject* current = &layout_object;
  while (current) {
    if (current->IsSVGResourceContainer())
      return true;
    current = current->Parent();
  }
  return false;
}

void SVGResourcesCache::ClientStyleChanged(LayoutObject& layout_object,
                                           StyleDifference diff,
                                           const ComputedStyle& new_style) {
  DCHECK(layout_object.GetNode());
  DCHECK(layout_object.GetNode()->IsSVGElement());

  if (!diff.HasDifference() || !layout_object.Parent())
    return;

  // LayoutObjects for SVGFE*Element should not be calling this function.
  DCHECK(!layout_object.IsSVGFilterPrimitive());

  // Dynamic changes of CSS properties like 'clip-path' may require us to
  // recompute the associated resources for a LayoutObject.
  // TODO(fs): Avoid passing in a useless StyleDifference, but instead compare
  // oldStyle/newStyle to see which resources changed to be able to selectively
  // rebuild individual resources, instead of all of them.
  bool needs_layout = false;
  if (LayoutObjectCanHaveResources(layout_object)) {
    SVGResourcesCache& cache = ResourcesCache(layout_object.GetDocument());
    auto update_info =
        cache.UpdateResourcesFromLayoutObject(layout_object, new_style);
    if (update_info) {
      layout_object.SetNeedsPaintPropertyUpdate();
      // Since the visual rect has the bounds of the clip-path, mask and filter
      // baked in, and the visual rect is updated during layout, we need to
      // trigger layout if the style change could somehow have affected the
      // bounds that form the visual rect.
      needs_layout = update_info.needs_layout;
    }
  }

  // If this layoutObject is the child of ResourceContainer and it require
  // repainting that changes of CSS properties such as 'visibility',
  // request repainting.
  needs_layout |= diff.NeedsPaintInvalidation() &&
                  IsLayoutObjectOfResourceContainer(layout_object);

  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
      layout_object, needs_layout);
}

void SVGResourcesCache::ResourceReferenceChanged(LayoutObject& layout_object) {
  DCHECK(layout_object.IsSVG());
  DCHECK(layout_object.GetNode());
  DCHECK(layout_object.GetNode()->IsSVGElement());

  if (!layout_object.Parent())
    return;

  // Only LayoutObjects that can actually have resources should be pending and
  // hence be able to call this method.
  DCHECK(LayoutObjectCanHaveResources(layout_object));

  SVGResourcesCache& cache = ResourcesCache(layout_object.GetDocument());
  if (cache.UpdateResourcesFromLayoutObject(layout_object,
                                            layout_object.StyleRef())) {
    layout_object.SetNeedsPaintPropertyUpdate();
  }

  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
      layout_object, true);
}

void SVGResourcesCache::ClientWasAddedToTree(LayoutObject& layout_object) {
  if (!layout_object.GetNode())
    return;
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
      layout_object, false);

  if (!LayoutObjectCanHaveResources(layout_object))
    return;
  SVGResourcesCache& cache = ResourcesCache(layout_object.GetDocument());
  if (cache.AddResourcesFromLayoutObject(layout_object,
                                         layout_object.StyleRef()))
    layout_object.SetNeedsPaintPropertyUpdate();
}

void SVGResourcesCache::ClientWillBeRemovedFromTree(
    LayoutObject& layout_object) {
  if (!layout_object.GetNode())
    return;
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
      layout_object, false);

  if (!LayoutObjectCanHaveResources(layout_object))
    return;
  SVGResourcesCache& cache = ResourcesCache(layout_object.GetDocument());
  if (cache.RemoveResourcesFromLayoutObject(layout_object))
    layout_object.SetNeedsPaintPropertyUpdate();
}

void SVGResourcesCache::ClientDestroyed(LayoutObject& layout_object) {
  SVGResourcesCache& cache = ResourcesCache(layout_object.GetDocument());
  cache.RemoveResourcesFromLayoutObject(layout_object);
}

SVGResourcesCache::TemporaryStyleScope::TemporaryStyleScope(
    LayoutObject& layout_object,
    const ComputedStyle& style,
    const ComputedStyle& temporary_style)
    : layout_object_(layout_object),
      original_style_(style),
      temporary_style_(temporary_style),
      styles_are_equal_(style == temporary_style) {
  if (styles_are_equal_)
    return;
  DCHECK(LayoutObjectCanHaveResources(layout_object_));
  auto& element = To<SVGElement>(*layout_object_.GetNode());
  SVGResources::UpdatePaints(element, nullptr, temporary_style_);
  SwitchTo(temporary_style);
}

SVGResourcesCache::TemporaryStyleScope::~TemporaryStyleScope() {
  if (styles_are_equal_)
    return;
  auto& element = To<SVGElement>(*layout_object_.GetNode());
  SVGResources::ClearPaints(element, &temporary_style_);
  SwitchTo(original_style_);
}

void SVGResourcesCache::TemporaryStyleScope::SwitchTo(
    const ComputedStyle& style) {
  DCHECK(!styles_are_equal_);
  SVGResourcesCache& cache = ResourcesCache(layout_object_.GetDocument());
  cache.UpdateResourcesFromLayoutObject(layout_object_, style);
}

}  // namespace blink
