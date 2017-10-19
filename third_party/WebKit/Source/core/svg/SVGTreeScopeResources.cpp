// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGTreeScopeResources.h"

#include "core/dom/Element.h"
#include "core/dom/TreeScope.h"
#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/svg/SVGUseElement.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

SVGTreeScopeResources::SVGTreeScopeResources(TreeScope* tree_scope)
    : tree_scope_(tree_scope) {}

SVGTreeScopeResources::~SVGTreeScopeResources() = default;

static LayoutSVGResourceContainer* LookupResource(TreeScope& tree_scope,
                                                  const AtomicString& id) {
  Element* element = tree_scope.getElementById(id);
  if (!element)
    return nullptr;
  LayoutObject* layout_object = element->GetLayoutObject();
  if (!layout_object || !layout_object->IsSVGResourceContainer())
    return nullptr;
  return ToLayoutSVGResourceContainer(layout_object);
}

void SVGTreeScopeResources::UpdateResource(
    const AtomicString& id,
    LayoutSVGResourceContainer* resource) {
  DCHECK(resource);
  if (resource->IsRegistered() || id.IsEmpty())
    return;
  // Lookup the current resource. (Could differ from what's in the map if an
  // element was just added/removed.)
  LayoutSVGResourceContainer* current_resource =
      LookupResource(*tree_scope_, id);
  // Lookup the currently registered resource.
  auto it = resources_.find(id);
  if (it != resources_.end()) {
    // Is the local map up-to-date already?
    if (it->value == current_resource)
      return;
    UnregisterResource(it);
  }
  if (current_resource)
    RegisterResource(id, current_resource);
}

void SVGTreeScopeResources::UpdateResource(
    const AtomicString& old_id,
    const AtomicString& new_id,
    LayoutSVGResourceContainer* resource) {
  RemoveResource(old_id, resource);
  UpdateResource(new_id, resource);
}

void SVGTreeScopeResources::RemoveResource(
    const AtomicString& id,
    LayoutSVGResourceContainer* resource) {
  DCHECK(resource);
  if (!resource->IsRegistered() || id.IsEmpty())
    return;
  auto it = resources_.find(id);
  // If this is not the currently registered resource for this id, then do
  // nothing.
  if (it == resources_.end() || it->value != resource)
    return;
  UnregisterResource(it);
  // If the layout tree is being torn down, then don't attempt to update the
  // map, since that layout object is likely to be stale already.
  if (resource->DocumentBeingDestroyed())
    return;
  // Another resource could now be current. Perform a lookup and potentially
  // update the map.
  LayoutSVGResourceContainer* current_resource =
      LookupResource(*tree_scope_, id);
  if (!current_resource)
    return;
  // Since this is a removal, don't allow re-adding the resource.
  if (current_resource == resource)
    return;
  RegisterResource(id, current_resource);
}

void SVGTreeScopeResources::RegisterResource(
    const AtomicString& id,
    LayoutSVGResourceContainer* resource) {
  DCHECK(!id.IsEmpty());
  DCHECK(resource);
  DCHECK(!resource->IsRegistered());

  resources_.Set(id, resource);
  resource->SetRegistered(true);

  NotifyPendingClients(id);
}

void SVGTreeScopeResources::UnregisterResource(ResourceMap::iterator it) {
  LayoutSVGResourceContainer* resource = it->value;
  DCHECK(resource);
  DCHECK(resource->IsRegistered());

  resource->DetachAllClients(it->key);

  resource->SetRegistered(false);
  resources_.erase(it);
}

LayoutSVGResourceContainer* SVGTreeScopeResources::ResourceById(
    const AtomicString& id) const {
  if (id.IsEmpty())
    return nullptr;
  return resources_.at(id);
}

void SVGTreeScopeResources::AddPendingResource(const AtomicString& id,
                                               Element& element) {
  DCHECK(element.isConnected());

  if (id.IsEmpty())
    return;
  auto result = pending_resources_.insert(id, nullptr);
  if (result.is_new_entry)
    result.stored_value->value = new SVGPendingElements;
  result.stored_value->value->insert(&element);

  element.SetHasPendingResources();
}

bool SVGTreeScopeResources::IsElementPendingResource(
    Element& element,
    const AtomicString& id) const {
  if (id.IsEmpty())
    return false;
  const SVGPendingElements* pending_elements = pending_resources_.at(id);
  return pending_elements && pending_elements->Contains(&element);
}

void SVGTreeScopeResources::ClearHasPendingResourcesIfPossible(
    Element& element) {
  // This algorithm takes time proportional to the number of pending resources
  // and need not.
  // If performance becomes an issue we can keep a counted set of elements and
  // answer the question efficiently.
  for (const auto& entry : pending_resources_) {
    SVGPendingElements* elements = entry.value.Get();
    DCHECK(elements);
    if (elements->Contains(&element))
      return;
  }
  element.ClearHasPendingResources();
}

void SVGTreeScopeResources::RemoveElementFromPendingResources(
    Element& element) {
  if (pending_resources_.IsEmpty() || !element.HasPendingResources())
    return;
  // Remove the element from pending resources.
  Vector<AtomicString> to_be_removed;
  for (const auto& entry : pending_resources_) {
    SVGPendingElements* elements = entry.value.Get();
    DCHECK(elements);
    DCHECK(!elements->IsEmpty());

    elements->erase(&element);
    if (elements->IsEmpty())
      to_be_removed.push_back(entry.key);
  }
  pending_resources_.RemoveAll(to_be_removed);

  ClearHasPendingResourcesIfPossible(element);
}

void SVGTreeScopeResources::NotifyPendingClients(const AtomicString& id) {
  DCHECK(!id.IsEmpty());
  SVGPendingElements* pending_elements = pending_resources_.Take(id);
  if (!pending_elements)
    return;
  // Update cached resources of pending clients.
  for (Element* client_element : *pending_elements) {
    DCHECK(client_element->HasPendingResources());
    ClearHasPendingResourcesIfPossible(*client_element);

    LayoutObject* layout_object = client_element->GetLayoutObject();
    if (!layout_object)
      continue;
    DCHECK(layout_object->IsSVG());

    StyleDifference diff;
    diff.SetNeedsFullLayout();
    SVGResourcesCache::ClientStyleChanged(layout_object, diff,
                                          layout_object->StyleRef());
    layout_object->SetNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::kSvgResourceInvalidated);
  }
}

void SVGTreeScopeResources::NotifyResourceAvailable(const AtomicString& id) {
  if (id.IsEmpty())
    return;
  // Get pending elements for this id.
  SVGPendingElements* pending_elements = pending_resources_.Take(id);
  if (!pending_elements)
    return;
  // Rebuild pending resources for each client of a pending resource that is
  // being removed.
  for (Element* client_element : *pending_elements) {
    DCHECK(client_element->HasPendingResources());
    if (!client_element->HasPendingResources())
      continue;
    // TODO(fs): Ideally we'd always resolve pending resources async instead of
    // inside insertedInto and svgAttributeChanged. For now we only do it for
    // <use> since that would stamp out DOM.
    if (auto* use = ToSVGUseElementOrNull(client_element))
      use->InvalidateShadowTree();
    else
      client_element->BuildPendingResource();

    ClearHasPendingResourcesIfPossible(*client_element);
  }
}

void SVGTreeScopeResources::Trace(blink::Visitor* visitor) {
  visitor->Trace(pending_resources_);
  visitor->Trace(tree_scope_);
}
}
