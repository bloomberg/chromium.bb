// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGResource.h"

#include "core/dom/Element.h"
#include "core/dom/TreeScope.h"
#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/svg/SVGElement.h"

namespace blink {

SVGResource::SVGResource(TreeScope& tree_scope, const AtomicString& id)
    : IdTargetObserver(tree_scope.GetIdTargetObserverRegistry(), id),
      tree_scope_(tree_scope),
      target_(tree_scope.getElementById(id)) {}

SVGResource::~SVGResource() = default;

void SVGResource::Trace(Visitor* visitor) {
  visitor->Trace(tree_scope_);
  visitor->Trace(target_);
  visitor->Trace(pending_clients_);
  IdTargetObserver::Trace(visitor);
}

void SVGResource::AddWatch(SVGElement& element) {
  pending_clients_.insert(&element);
  element.SetHasPendingResources();
}

void SVGResource::RemoveWatch(SVGElement& element) {
  pending_clients_.erase(&element);
}

bool SVGResource::IsEmpty() const {
  LayoutSVGResourceContainer* container = ResourceContainer();
  return (!container || !container->HasClients()) && pending_clients_.IsEmpty();
}

void SVGResource::NotifyResourceClients() {
  HeapHashSet<Member<SVGElement>> pending_clients;
  pending_clients.swap(pending_clients_);

  for (SVGElement* client_element : pending_clients) {
    if (LayoutObject* layout_object = client_element->GetLayoutObject())
      SVGResourcesCache::ResourceReferenceChanged(*layout_object);
  }
}

LayoutSVGResourceContainer* SVGResource::ResourceContainer() const {
  if (!target_)
    return nullptr;
  LayoutObject* layout_object = target_->GetLayoutObject();
  if (!layout_object || !layout_object->IsSVGResourceContainer())
    return nullptr;
  return ToLayoutSVGResourceContainer(layout_object);
}

void SVGResource::IdTargetChanged() {
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

}  // namespace blink
