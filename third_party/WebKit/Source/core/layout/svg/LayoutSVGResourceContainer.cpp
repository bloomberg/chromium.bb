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

#include "core/layout/svg/LayoutSVGResourceContainer.h"

#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/svg/SVGResource.h"
#include "core/svg/SVGTreeScopeResources.h"
#include "platform/wtf/AutoReset.h"

namespace blink {

namespace {

LocalSVGResource* ResourceForContainer(
    const LayoutSVGResourceContainer& resource_container) {
  const SVGElement& element = *resource_container.GetElement();
  return element.GetTreeScope()
      .EnsureSVGTreeScopedResources()
      .ExistingResourceForId(element.GetIdAttribute());
}

}  // namespace

LayoutSVGResourceContainer::LayoutSVGResourceContainer(SVGElement* node)
    : LayoutSVGHiddenContainer(node),
      is_in_layout_(false),
      completed_invalidations_mask_(0),
      is_invalidating_(false) {}

LayoutSVGResourceContainer::~LayoutSVGResourceContainer() = default;

void LayoutSVGResourceContainer::UpdateLayout() {
  // FIXME: Investigate a way to detect and break resource layout dependency
  // cycles early. Then we can remove this method altogether, and fall back onto
  // LayoutSVGHiddenContainer::layout().
  DCHECK(NeedsLayout());
  if (is_in_layout_)
    return;

  AutoReset<bool> in_layout_change(&is_in_layout_, true);

  LayoutSVGHiddenContainer::UpdateLayout();

  ClearInvalidationMask();
}

void LayoutSVGResourceContainer::WillBeDestroyed() {
  LayoutSVGHiddenContainer::WillBeDestroyed();
  // The resource is being torn down. If we have any clients, move those to be
  // pending on the resource (if one exists.)
  if (LocalSVGResource* resource = ResourceForContainer(*this))
    MakeClientsPending(*resource);
}

void LayoutSVGResourceContainer::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style) {
  LayoutSVGHiddenContainer::StyleDidChange(diff, old_style);
  // The resource has (read: may have) been attached. Notify any pending
  // clients that they can now try to add themselves as clients to the
  // resource.
  if (LocalSVGResource* resource = ResourceForContainer(*this)) {
    if (resource->Target() == GetElement())
      resource->NotifyPendingClients();
  }
}

void LayoutSVGResourceContainer::MakeClientsPending(
    LocalSVGResource& resource) {
  RemoveAllClientsFromCache();

  for (auto* client : clients_) {
    // Unlink the resource from the client's SVGResources.
    SVGResources* resources =
        SVGResourcesCache::CachedResourcesForLayoutObject(*client);
    // Or else the client wouldn't be in the list in the first place.
    DCHECK(resources);
    resources->ResourceDestroyed(this);

    resource.AddWatch(ToSVGElement(*client->GetNode()));
  }
  clients_.clear();
}

void LayoutSVGResourceContainer::MarkAllClientsForInvalidation(
    InvalidationModeMask invalidation_mask) {
  if (is_invalidating_)
    return;
  LocalSVGResource* resource = ResourceForContainer(*this);
  if (clients_.IsEmpty() && (!resource || !resource->HasClients()))
    return;
  // Remove modes for which invalidations have already been
  // performed. If no modes remain we are done.
  invalidation_mask &= ~completed_invalidations_mask_;
  if (invalidation_mask == 0)
    return;
  completed_invalidations_mask_ |= invalidation_mask;

  is_invalidating_ = true;
  bool needs_layout = invalidation_mask & kLayoutInvalidation;
  bool mark_for_invalidation = invalidation_mask & ~kParentOnlyInvalidation;

  // Invalidate clients registered on the this object (via SVGResources).
  for (auto* client : clients_) {
    DCHECK(client->IsSVG());
    if (client->IsSVGResourceContainer()) {
      ToLayoutSVGResourceContainer(client)->RemoveAllClientsFromCache(
          mark_for_invalidation);
      continue;
    }

    if (mark_for_invalidation)
      MarkClientForInvalidation(*client, invalidation_mask);

    MarkForLayoutAndParentResourceInvalidation(*client, needs_layout);
  }

  // Invalidate clients registered via an SVGResource.
  if (resource)
    resource->NotifyContentChanged();

  is_invalidating_ = false;
}

void LayoutSVGResourceContainer::MarkClientForInvalidation(
    LayoutObject& client,
    InvalidationModeMask invalidation_mask) {
  if (invalidation_mask & kPaintInvalidation) {
    // Since LayoutSVGInlineTexts don't have SVGResources (they use their
    // parent's), they will not be notified of changes to paint servers. So
    // if the client is one that could have a LayoutSVGInlineText use a
    // paint invalidation reason that will force paint invalidation of the
    // entire <text>/<tspan>/... subtree.
    client.SetShouldDoFullPaintInvalidation(
        PaintInvalidationReason::kSVGResource);
    client.InvalidateClipPathCache();
    // Invalidate paint properties to update effects if any.
    client.SetNeedsPaintPropertyUpdate();
  }

  if (invalidation_mask & kBoundariesInvalidation)
    client.SetNeedsBoundariesUpdate();
}

void LayoutSVGResourceContainer::AddClient(LayoutObject& client) {
  clients_.insert(&client);
  ClearInvalidationMask();
}

bool LayoutSVGResourceContainer::RemoveClient(LayoutObject& client) {
  RemoveClientFromCache(client);
  clients_.erase(&client);
  return clients_.IsEmpty();
}

void LayoutSVGResourceContainer::InvalidateCacheAndMarkForLayout(
    LayoutInvalidationReasonForTracing reason,
    SubtreeLayoutScope* layout_scope) {
  if (SelfNeedsLayout())
    return;

  SetNeedsLayoutAndFullPaintInvalidation(reason, kMarkContainerChain,
                                         layout_scope);

  if (EverHadLayout())
    RemoveAllClientsFromCache();
}

void LayoutSVGResourceContainer::InvalidateCacheAndMarkForLayout(
    SubtreeLayoutScope* layout_scope) {
  InvalidateCacheAndMarkForLayout(
      LayoutInvalidationReason::kSvgResourceInvalidated, layout_scope);
}

static inline void RemoveFromCacheAndInvalidateDependencies(
    LayoutObject& object,
    bool needs_layout) {
  if (SVGResources* resources =
          SVGResourcesCache::CachedResourcesForLayoutObject(object)) {
    if (InvalidationModeMask invalidation_mask =
            resources->RemoveClientFromCacheAffectingObjectBounds(object)) {
      LayoutSVGResourceContainer::MarkClientForInvalidation(object,
                                                            invalidation_mask);
    }
  }

  if (!object.GetNode() || !object.GetNode()->IsSVGElement())
    return;

  ToSVGElement(object.GetNode())
      ->NotifyIncomingReferences([needs_layout](SVGElement& element) {
        LayoutObject* layout_object = element.GetLayoutObject();
        if (!layout_object)
          return;
        LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
            *layout_object, needs_layout);
      });
}

void LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
    LayoutObject& object,
    bool needs_layout) {
  DCHECK(object.GetNode());

  if (needs_layout && !object.DocumentBeingDestroyed())
    object.SetNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::kSvgResourceInvalidated);

  RemoveFromCacheAndInvalidateDependencies(object, needs_layout);

  // Invalidate resources in ancestor chain, if needed.
  LayoutObject* current = object.Parent();
  while (current) {
    RemoveFromCacheAndInvalidateDependencies(*current, needs_layout);

    if (current->IsSVGResourceContainer()) {
      // This will process the rest of the ancestors.
      ToLayoutSVGResourceContainer(current)->RemoveAllClientsFromCache();
      break;
    }

    current = current->Parent();
  }
}

}  // namespace blink
