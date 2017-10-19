// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGElementProxy.h"

#include "core/dom/Document.h"
#include "core/dom/IdTargetObserver.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGResourceClient.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/fetch_initiator_type_names.h"

namespace blink {

class SVGElementProxy::IdObserver : public IdTargetObserver {
 public:
  IdObserver(TreeScope& tree_scope, SVGElementProxy& proxy)
      : IdTargetObserver(tree_scope.GetIdTargetObserverRegistry(), proxy.Id()),
        tree_scope_(&tree_scope) {}

  void AddClient(SVGResourceClient* client) { clients_.insert(client); }
  bool RemoveClient(SVGResourceClient* client) {
    return clients_.erase(client);
  }
  bool HasClients() const { return !clients_.IsEmpty(); }

  TreeScope* GetTreeScope() const { return tree_scope_; }
  void TransferClients(IdObserver& observer) {
    for (const auto& client : clients_)
      observer.clients_.insert(client.key, client.value);
    clients_.clear();
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(clients_);
    visitor->Trace(tree_scope_);
    IdTargetObserver::Trace(visitor);
  }

  void ContentChanged() {
    DCHECK(Lifecycle().GetState() <= DocumentLifecycle::kCompositingClean ||
           Lifecycle().GetState() >= DocumentLifecycle::kPaintClean);
    HeapVector<Member<SVGResourceClient>> clients;
    CopyToVector(clients_, clients);
    for (SVGResourceClient* client : clients)
      client->ResourceContentChanged();
  }

 private:
  const DocumentLifecycle& Lifecycle() const {
    return tree_scope_->GetDocument().Lifecycle();
  }
  void IdTargetChanged() override {
    DCHECK(Lifecycle().StateAllowsTreeMutations());
    HeapVector<Member<SVGResourceClient>> clients;
    CopyToVector(clients_, clients);
    for (SVGResourceClient* client : clients)
      client->ResourceElementChanged();
  }
  HeapHashCountedSet<Member<SVGResourceClient>> clients_;
  Member<TreeScope> tree_scope_;
};

SVGElementProxy::SVGElementProxy(const AtomicString& id)
    : id_(id), is_local_(true) {}

SVGElementProxy::SVGElementProxy(const String& url, const AtomicString& id)
    : id_(id), url_(url), is_local_(false) {}

SVGElementProxy::~SVGElementProxy() {}

void SVGElementProxy::AddClient(SVGResourceClient* client) {
  // An empty id will never be a valid element reference.
  if (id_.IsEmpty())
    return;
  if (!is_local_) {
    if (document_)
      document_->AddClient(client);
    return;
  }
  TreeScope* client_scope = client->GetTreeScope();
  if (!client_scope)
    return;
  // Ensure sure we have an observer registered for this tree scope.
  auto& scope_observer =
      observers_.insert(client_scope, nullptr).stored_value->value;
  if (!scope_observer)
    scope_observer = new IdObserver(*client_scope, *this);

  auto& observer = clients_.insert(client, nullptr).stored_value->value;
  if (!observer)
    observer = scope_observer;

  DCHECK(observer && scope_observer);

  // If the client moved to a different scope, we need to unregister the old
  // observer and transfer any clients from it before replacing it. Thus any
  // clients that remain to be removed will be transferred to the new observer,
  // and hence removed from it instead.
  if (observer != scope_observer) {
    observer->Unregister();
    observer->TransferClients(*scope_observer);
    observer = scope_observer;
  }
  observer->AddClient(client);
}

void SVGElementProxy::RemoveClient(SVGResourceClient* client) {
  // An empty id will never be a valid element reference.
  if (id_.IsEmpty())
    return;
  if (!is_local_) {
    if (document_)
      document_->RemoveClient(client);
    return;
  }
  auto entry = clients_.find(client);
  if (entry == clients_.end())
    return;
  IdObserver* observer = entry->value;
  DCHECK(observer);
  // If the client is not the last client in the scope, then no further action
  // needs to be taken.
  if (!observer->RemoveClient(client))
    return;
  // Unregister and drop the scope association, then drop the client.
  if (!observer->HasClients()) {
    observer->Unregister();
    observers_.erase(observer->GetTreeScope());
  }
  clients_.erase(entry);
}

void SVGElementProxy::Resolve(Document& document) {
  if (is_local_ || id_.IsEmpty() || url_.IsEmpty())
    return;
  ResourceLoaderOptions options;
  options.initiator_info.name = FetchInitiatorTypeNames::css;
  FetchParameters params(ResourceRequest(url_), options);
  document_ = DocumentResource::FetchSVGDocument(params, document.Fetcher());
  url_ = String();
}

TreeScope* SVGElementProxy::TreeScopeForLookup(TreeScope& tree_scope) const {
  if (is_local_)
    return &tree_scope;
  if (!document_)
    return nullptr;
  return document_->GetDocument();
}

SVGElement* SVGElementProxy::FindElement(TreeScope& tree_scope) {
  // An empty id will never be a valid element reference.
  if (id_.IsEmpty())
    return nullptr;
  TreeScope* lookup_scope = TreeScopeForLookup(tree_scope);
  if (!lookup_scope)
    return nullptr;
  if (Element* target_element = lookup_scope->getElementById(id_)) {
    SVGElementProxySet* proxy_set =
        target_element->IsSVGElement()
            ? ToSVGElement(target_element)->ElementProxySet()
            : nullptr;
    if (proxy_set) {
      proxy_set->Add(*this);
      return ToSVGElement(target_element);
    }
  }
  return nullptr;
}

void SVGElementProxy::ContentChanged(TreeScope& tree_scope) {
  if (auto* observer = observers_.at(&tree_scope))
    observer->ContentChanged();
}

void SVGElementProxy::Trace(blink::Visitor* visitor) {
  visitor->Trace(clients_);
  visitor->Trace(observers_);
  visitor->Trace(document_);
}

void SVGElementProxySet::Add(SVGElementProxy& element_proxy) {
  element_proxies_.insert(&element_proxy);
}

bool SVGElementProxySet::IsEmpty() const {
  return element_proxies_.IsEmpty();
}

void SVGElementProxySet::NotifyContentChanged(TreeScope& tree_scope) {
  for (SVGElementProxy* proxy : element_proxies_)
    proxy->ContentChanged(tree_scope);
}

void SVGElementProxySet::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_proxies_);
}

}  // namespace blink
