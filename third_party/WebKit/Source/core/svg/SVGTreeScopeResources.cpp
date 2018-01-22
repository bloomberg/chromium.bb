// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGTreeScopeResources.h"

#include "core/dom/Element.h"
#include "core/dom/TreeScope.h"
#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

SVGTreeScopeResources::Resource::Resource(TreeScope& tree_scope,
                                          const AtomicString& id)
    : IdTargetObserver(tree_scope.GetIdTargetObserverRegistry(), id),
      tree_scope_(tree_scope),
      target_(tree_scope.getElementById(id)) {}

SVGTreeScopeResources::Resource::~Resource() = default;

void SVGTreeScopeResources::Resource::Trace(Visitor* visitor) {
  visitor->Trace(tree_scope_);
  visitor->Trace(target_);
  visitor->Trace(pending_clients_);
  IdTargetObserver::Trace(visitor);
}

void SVGTreeScopeResources::Resource::AddWatch(SVGElement& element) {
  pending_clients_.insert(&element);
  element.SetHasPendingResources();
}

void SVGTreeScopeResources::Resource::RemoveWatch(SVGElement& element) {
  pending_clients_.erase(&element);
}

bool SVGTreeScopeResources::Resource::IsEmpty() const {
  LayoutSVGResourceContainer* container = ResourceContainer();
  return (!container || !container->HasClients()) && pending_clients_.IsEmpty();
}

void SVGTreeScopeResources::Resource::NotifyResourceClients() {
  HeapHashSet<Member<SVGElement>> pending_clients;
  pending_clients.swap(pending_clients_);

  for (SVGElement* client_element : pending_clients) {
    if (LayoutObject* layout_object = client_element->GetLayoutObject())
      SVGResourcesCache::ResourceReferenceChanged(*layout_object);
  }
}

LayoutSVGResourceContainer* SVGTreeScopeResources::Resource::ResourceContainer()
    const {
  if (!target_)
    return nullptr;
  LayoutObject* layout_object = target_->GetLayoutObject();
  if (!layout_object || !layout_object->IsSVGResourceContainer())
    return nullptr;
  return ToLayoutSVGResourceContainer(layout_object);
}

void SVGTreeScopeResources::Resource::IdTargetChanged() {
  Element* new_target = tree_scope_->getElementById(Id());
  if (new_target == target_)
    return;
  // Detach clients from the old resource, moving them to the pending list
  // and then notify pending clients.
  if (LayoutSVGResourceContainer* old_resource = ResourceContainer())
    old_resource->MakeClientsPending(*this);
  target_ = new_target;
  NotifyResourceClients();
}

SVGTreeScopeResources::SVGTreeScopeResources(TreeScope* tree_scope)
    : tree_scope_(tree_scope) {}

SVGTreeScopeResources::~SVGTreeScopeResources() = default;

SVGTreeScopeResources::Resource* SVGTreeScopeResources::ResourceForId(
    const AtomicString& id) {
  if (id.IsEmpty())
    return nullptr;
  auto& entry = resources_.insert(id, nullptr).stored_value->value;
  if (!entry)
    entry = new Resource(*tree_scope_, id);
  return entry;
}

SVGTreeScopeResources::Resource* SVGTreeScopeResources::ExistingResourceForId(
    const AtomicString& id) const {
  if (id.IsEmpty())
    return nullptr;
  return resources_.at(id);
}

void SVGTreeScopeResources::RemoveUnreferencedResources() {
  if (resources_.IsEmpty())
    return;
  // Remove resources that are no longer referenced.
  Vector<AtomicString> to_be_removed;
  for (const auto& entry : resources_) {
    Resource* resource = entry.value.Get();
    DCHECK(resource);
    if (resource->IsEmpty()) {
      resource->Unregister();
      to_be_removed.push_back(entry.key);
    }
  }
  resources_.RemoveAll(to_be_removed);
}

void SVGTreeScopeResources::RemoveWatchesForElement(SVGElement& element) {
  if (resources_.IsEmpty() || !element.HasPendingResources())
    return;
  // Remove the element from pending resources.
  Vector<AtomicString> to_be_removed;
  for (const auto& entry : resources_) {
    Resource* resource = entry.value.Get();
    DCHECK(resource);
    resource->RemoveWatch(element);
    if (resource->IsEmpty()) {
      resource->Unregister();
      to_be_removed.push_back(entry.key);
    }
  }
  resources_.RemoveAll(to_be_removed);

  element.ClearHasPendingResources();
}

void SVGTreeScopeResources::Trace(blink::Visitor* visitor) {
  visitor->Trace(resources_);
  visitor->Trace(tree_scope_);
}

}  // namespace blink
