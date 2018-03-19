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

#include "core/layout/svg/SVGResourcesCycleSolver.h"

#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"

namespace blink {

SVGResourcesCycleSolver::SVGResourcesCycleSolver(LayoutObject& layout_object)
    : layout_object_(layout_object) {
  if (layout_object.IsSVGResourceContainer())
    active_resources_.insert(ToLayoutSVGResourceContainer(&layout_object));
}

SVGResourcesCycleSolver::~SVGResourcesCycleSolver() = default;

struct ActiveFrame {
  typedef SVGResourcesCycleSolver::ResourceSet ResourceSet;

  ActiveFrame(ResourceSet& active_set, LayoutSVGResourceContainer* resource)
      : active_set_(active_set), resource_(resource) {
    active_set_.insert(resource_);
  }
  ~ActiveFrame() { active_set_.erase(resource_); }

  ResourceSet& active_set_;
  LayoutSVGResourceContainer* resource_;
};

bool SVGResourcesCycleSolver::TraverseResourceContainer(
    LayoutSVGResourceContainer* resource) {
  // If we've traversed this sub-graph before and no cycles were observed, then
  // reuse that result.
  if (dag_cache_.Contains(resource))
    return false;

  if (active_resources_.Contains(resource))
    return true;

  ActiveFrame frame(active_resources_, resource);

  LayoutObject* node = resource;
  while (node) {
    // Skip subtrees which are themselves resources. (They will be
    // processed - if needed - when they are actually referenced.)
    if (node != resource && node->IsSVGResourceContainer()) {
      node = node->NextInPreOrderAfterChildren(resource);
      continue;
    }
    if (TraverseResources(*node))
      return true;
    node = node->NextInPreOrder(resource);
  }

  // No cycles found in (or from) this resource. Add it to the "DAG cache".
  dag_cache_.insert(resource);
  return false;
}

bool SVGResourcesCycleSolver::TraverseResources(LayoutObject& layout_object) {
  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(layout_object);
  return resources && TraverseResources(resources);
}

bool SVGResourcesCycleSolver::TraverseResources(SVGResources* resources) {
  // Fetch all the referenced resources.
  ResourceSet local_resources;
  resources->BuildSetOfResources(local_resources);

  // This performs a depth-first search for a back-edge in all the
  // (potentially disjoint) graphs formed by the referenced resources.
  for (auto* local_resource : local_resources) {
    if (TraverseResourceContainer(local_resource))
      return true;
  }
  return false;
}

bool SVGResourcesCycleSolver::FindCycle(
    LayoutSVGResourceContainer* start_node) {
  DCHECK(active_resources_.IsEmpty() ||
         (active_resources_.size() == 1 &&
          active_resources_.Contains(
              ToLayoutSVGResourceContainer(&layout_object_))));
  return TraverseResourceContainer(start_node);
}

}  // namespace blink
