// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGResource.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/IdTargetObserver.h"
#include "core/dom/TreeScope.h"
#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/loader/resource/DocumentResource.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGResourceClient.h"
#include "core/svg/SVGURIReference.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/fetch_initiator_type_names.h"

namespace blink {

SVGResource::SVGResource() = default;

SVGResource::~SVGResource() = default;

void SVGResource::Trace(Visitor* visitor) {
  visitor->Trace(target_);
  visitor->Trace(clients_);
}

void SVGResource::AddClient(SVGResourceClient& client) {
  clients_.insert(&client);
}

void SVGResource::RemoveClient(SVGResourceClient& client) {
  clients_.erase(&client);
}

void SVGResource::NotifyElementChanged() {
  HeapVector<Member<SVGResourceClient>> clients;
  CopyToVector(clients_, clients);

  for (SVGResourceClient* client : clients)
    client->ResourceElementChanged();
}

LayoutSVGResourceContainer* SVGResource::ResourceContainer() const {
  if (!target_)
    return nullptr;
  LayoutObject* layout_object = target_->GetLayoutObject();
  if (!layout_object || !layout_object->IsSVGResourceContainer())
    return nullptr;
  return ToLayoutSVGResourceContainer(layout_object);
}

LocalSVGResource::LocalSVGResource(TreeScope& tree_scope,
                                   const AtomicString& id)
    : tree_scope_(tree_scope) {
  target_ = SVGURIReference::ObserveTarget(
      id_observer_, tree_scope, id,
      WTF::BindRepeating(&LocalSVGResource::TargetChanged,
                         WrapWeakPersistent(this), id));
}

void LocalSVGResource::AddWatch(SVGElement& element) {
  pending_clients_.insert(&element);
  element.SetHasPendingResources();
}

void LocalSVGResource::RemoveWatch(SVGElement& element) {
  pending_clients_.erase(&element);
}

bool LocalSVGResource::IsEmpty() const {
  LayoutSVGResourceContainer* container = ResourceContainer();
  return !HasClients() && (!container || !container->HasClients()) &&
         pending_clients_.IsEmpty();
}

void LocalSVGResource::Unregister() {
  SVGURIReference::UnobserveTarget(id_observer_);
}

void LocalSVGResource::NotifyPendingClients() {
  HeapHashSet<Member<SVGElement>> pending_clients;
  pending_clients.swap(pending_clients_);

  for (SVGElement* client_element : pending_clients) {
    if (LayoutObject* layout_object = client_element->GetLayoutObject())
      SVGResourcesCache::ResourceReferenceChanged(*layout_object);
  }
}

void LocalSVGResource::NotifyContentChanged(
    InvalidationModeMask invalidation_mask) {
  HeapVector<Member<SVGResourceClient>> clients;
  CopyToVector(clients_, clients);

  for (SVGResourceClient* client : clients)
    client->ResourceContentChanged(invalidation_mask);
}

void LocalSVGResource::TargetChanged(const AtomicString& id) {
  Element* new_target = tree_scope_->getElementById(id);
  if (new_target == target_)
    return;
  // Detach clients from the old resource, moving them to the pending list
  // and then notify pending clients.
  if (LayoutSVGResourceContainer* old_resource = ResourceContainer())
    old_resource->MakeClientsPending(*this);
  target_ = new_target;
  NotifyElementChanged();
  NotifyPendingClients();
}

void LocalSVGResource::Trace(Visitor* visitor) {
  visitor->Trace(tree_scope_);
  visitor->Trace(id_observer_);
  visitor->Trace(pending_clients_);
  SVGResource::Trace(visitor);
}

ExternalSVGResource::ExternalSVGResource(const KURL& url) : url_(url) {}

void ExternalSVGResource::Load(const Document& document) {
  if (resource_document_)
    return;
  ResourceLoaderOptions options;
  options.initiator_info.name = FetchInitiatorTypeNames::css;
  FetchParameters params(ResourceRequest(url_), options);
  resource_document_ =
      DocumentResource::FetchSVGDocument(params, document.Fetcher(), this);
  target_ = ResolveTarget();
}

void ExternalSVGResource::NotifyFinished(Resource*) {
  Element* new_target = ResolveTarget();
  if (new_target == target_)
    return;
  target_ = new_target;
  NotifyElementChanged();
}

String ExternalSVGResource::DebugName() const {
  return "ExternalSVGResource";
}

Element* ExternalSVGResource::ResolveTarget() {
  if (!resource_document_)
    return nullptr;
  if (!url_.HasFragmentIdentifier())
    return nullptr;
  Document* external_document = resource_document_->GetDocument();
  if (!external_document)
    return nullptr;
  return external_document->getElementById(
      AtomicString(url_.FragmentIdentifier()));
}

void ExternalSVGResource::Trace(Visitor* visitor) {
  visitor->Trace(resource_document_);
  SVGResource::Trace(visitor);
  ResourceClient::Trace(visitor);
}

}  // namespace blink
