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

#include "core/layout/svg/SVGResourcesCache.h"

#include <memory>
#include "core/html_names.h"
#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCycleSolver.h"
#include "core/svg/SVGDocumentExtensions.h"

namespace blink {

SVGResourcesCache::SVGResourcesCache() {}

SVGResourcesCache::~SVGResourcesCache() {}

void SVGResourcesCache::AddResourcesFromLayoutObject(
    LayoutObject* object,
    const ComputedStyle& style) {
  DCHECK(object);
  DCHECK(!cache_.Contains(object));

  // Build a list of all resources associated with the passed LayoutObject.
  std::unique_ptr<SVGResources> new_resources =
      SVGResources::BuildResources(object, style);
  if (!new_resources)
    return;

  // The new resource may cause new paint property nodes.
  object->SetNeedsPaintPropertyUpdate();

  // Put object in cache.
  SVGResources* resources =
      cache_.Set(object, std::move(new_resources)).stored_value->value.get();

  // Run cycle-detection _afterwards_, so self-references can be caught as well.
  SVGResourcesCycleSolver solver(object, resources);
  solver.ResolveCycles();

  // Walk resources and register the layout object as a client of each resource.
  HashSet<LayoutSVGResourceContainer*> resource_set;
  resources->BuildSetOfResources(resource_set);

  for (auto* resource_container : resource_set)
    resource_container->AddClient(object);
}

void SVGResourcesCache::RemoveResourcesFromLayoutObject(LayoutObject* object) {
  std::unique_ptr<SVGResources> resources = cache_.Take(object);
  if (!resources)
    return;

  // Removal of the resource may cause removal of paint property nodes.
  object->SetNeedsPaintPropertyUpdate();

  // Walk resources and unregister the layout object as a client of each
  // resource.
  HashSet<LayoutSVGResourceContainer*> resource_set;
  resources->BuildSetOfResources(resource_set);

  for (auto* resource_container : resource_set)
    resource_container->RemoveClient(object);
}

static inline SVGResourcesCache& ResourcesCache(Document& document) {
  return document.AccessSVGExtensions().ResourcesCache();
}

SVGResources* SVGResourcesCache::CachedResourcesForLayoutObject(
    const LayoutObject* layout_object) {
  DCHECK(layout_object);
  return ResourcesCache(layout_object->GetDocument()).cache_.at(layout_object);
}

void SVGResourcesCache::ClientLayoutChanged(LayoutObject* object) {
  SVGResources* resources = CachedResourcesForLayoutObject(object);
  if (!resources)
    return;

  // Invalidate the resources if either the LayoutObject itself changed,
  // or we have filter resources, which could depend on the layout of children.
  if (object->SelfNeedsLayout() || resources->Filter())
    resources->RemoveClientFromCache(object);
}

static inline bool LayoutObjectCanHaveResources(LayoutObject* layout_object) {
  DCHECK(layout_object);
  return layout_object->GetNode() && layout_object->GetNode()->IsSVGElement() &&
         !layout_object->IsSVGInlineText();
}

static inline bool IsLayoutObjectOfResourceContainer(
    LayoutObject* layout_object) {
  LayoutObject* current = layout_object;
  while (current) {
    if (current->IsSVGResourceContainer()) {
      return true;
    }
    current = current->Parent();
  }
  return false;
}

void SVGResourcesCache::ClientStyleChanged(LayoutObject* layout_object,
                                           StyleDifference diff,
                                           const ComputedStyle& new_style) {
  DCHECK(layout_object);
  DCHECK(layout_object->GetNode());
  DCHECK(layout_object->GetNode()->IsSVGElement());

  if (!diff.HasDifference() || !layout_object->Parent())
    return;

  // In this case the proper SVGFE*Element will decide whether the modified CSS
  // properties require
  // a relayout or paintInvalidation.
  if (layout_object->IsSVGResourceFilterPrimitive() && !diff.NeedsLayout())
    return;

  // Dynamic changes of CSS properties like 'clip-path' may require us to
  // recompute the associated resources for a LayoutObject.
  // TODO(fs): Avoid passing in a useless StyleDifference, but instead compare
  // oldStyle/newStyle to see which resources changed to be able to selectively
  // rebuild individual resources, instead of all of them.
  if (LayoutObjectCanHaveResources(layout_object)) {
    SVGResourcesCache& cache = ResourcesCache(layout_object->GetDocument());
    cache.RemoveResourcesFromLayoutObject(layout_object);
    cache.AddResourcesFromLayoutObject(layout_object, new_style);
  }

  // If this layoutObject is the child of ResourceContainer and it require
  // repainting that changes of CSS properties such as 'visibility',
  // request repainting.
  bool needs_layout = diff.NeedsFullPaintInvalidation() &&
                      IsLayoutObjectOfResourceContainer(layout_object);

  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
      layout_object, needs_layout);
}

void SVGResourcesCache::ClientWasAddedToTree(LayoutObject* layout_object,
                                             const ComputedStyle& new_style) {
  if (!layout_object->GetNode())
    return;
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
      layout_object, false);

  if (!LayoutObjectCanHaveResources(layout_object))
    return;
  SVGResourcesCache& cache = ResourcesCache(layout_object->GetDocument());
  cache.AddResourcesFromLayoutObject(layout_object, new_style);
}

void SVGResourcesCache::ClientWillBeRemovedFromTree(
    LayoutObject* layout_object) {
  if (!layout_object->GetNode())
    return;
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
      layout_object, false);

  if (!LayoutObjectCanHaveResources(layout_object))
    return;
  SVGResourcesCache& cache = ResourcesCache(layout_object->GetDocument());
  cache.RemoveResourcesFromLayoutObject(layout_object);
}

void SVGResourcesCache::ClientDestroyed(LayoutObject* layout_object) {
  DCHECK(layout_object);

  SVGResources* resources = CachedResourcesForLayoutObject(layout_object);
  if (resources)
    resources->RemoveClientFromCache(layout_object);
  SVGResourcesCache& cache = ResourcesCache(layout_object->GetDocument());
  cache.RemoveResourcesFromLayoutObject(layout_object);
}

SVGResourcesCache::TemporaryStyleScope::TemporaryStyleScope(
    LayoutObject& layout_object,
    const ComputedStyle& style,
    const ComputedStyle& temporary_style)
    : layout_object_(layout_object),
      original_style_(style),
      styles_are_equal_(style == temporary_style) {
  SwitchTo(temporary_style);
}

void SVGResourcesCache::TemporaryStyleScope::SwitchTo(
    const ComputedStyle& style) {
  DCHECK(LayoutObjectCanHaveResources(&layout_object_));
  if (styles_are_equal_)
    return;
  SVGResourcesCache& cache = ResourcesCache(layout_object_.GetDocument());
  cache.RemoveResourcesFromLayoutObject(&layout_object_);
  cache.AddResourcesFromLayoutObject(&layout_object_, style);
}

}  // namespace blink
