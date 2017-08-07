// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorResourceContentLoader.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/resource/CSSStyleSheetResource.h"
#include "core/loader/resource/StyleSheetResourceClient.h"
#include "core/page/Page.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class InspectorResourceContentLoader::ResourceClient final
    : public GarbageCollectedFinalized<
          InspectorResourceContentLoader::ResourceClient>,
      private RawResourceClient,
      private StyleSheetResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(ResourceClient);

 public:
  explicit ResourceClient(InspectorResourceContentLoader* loader)
      : loader_(loader) {}

  void WaitForResource(Resource* resource) {
    if (resource->GetType() == Resource::kRaw)
      resource->AddClient(static_cast<RawResourceClient*>(this));
    else
      resource->AddClient(static_cast<StyleSheetResourceClient*>(this));
  }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(loader_);
    StyleSheetResourceClient::Trace(visitor);
    RawResourceClient::Trace(visitor);
  }

 private:
  Member<InspectorResourceContentLoader> loader_;

  void SetCSSStyleSheet(const String&,
                        const KURL&,
                        ReferrerPolicy,
                        const WTF::TextEncoding&,
                        const CSSStyleSheetResource*) override;
  void NotifyFinished(Resource*) override;
  String DebugName() const override {
    return "InspectorResourceContentLoader::ResourceClient";
  }
  void ResourceFinished(Resource*);

  friend class InspectorResourceContentLoader;
};

void InspectorResourceContentLoader::ResourceClient::ResourceFinished(
    Resource* resource) {
  if (loader_)
    loader_->ResourceFinished(this);

  if (resource->GetType() == Resource::kRaw)
    resource->RemoveClient(static_cast<RawResourceClient*>(this));
  else
    resource->RemoveClient(static_cast<StyleSheetResourceClient*>(this));
}

void InspectorResourceContentLoader::ResourceClient::SetCSSStyleSheet(
    const String&,
    const KURL& url,
    ReferrerPolicy,
    const WTF::TextEncoding&,
    const CSSStyleSheetResource* resource) {
  ResourceFinished(const_cast<CSSStyleSheetResource*>(resource));
}

void InspectorResourceContentLoader::ResourceClient::NotifyFinished(
    Resource* resource) {
  if (resource->GetType() == Resource::kCSSStyleSheet)
    return;
  ResourceFinished(resource);
}

InspectorResourceContentLoader::InspectorResourceContentLoader(
    LocalFrame* inspected_frame)
    : all_requests_started_(false),
      started_(false),
      inspected_frame_(inspected_frame),
      last_client_id_(0) {}

void InspectorResourceContentLoader::Start() {
  started_ = true;
  HeapVector<Member<Document>> documents;
  InspectedFrames* inspected_frames = InspectedFrames::Create(inspected_frame_);
  for (LocalFrame* frame : *inspected_frames) {
    documents.push_back(frame->GetDocument());
    documents.AppendVector(InspectorPageAgent::ImportsForFrame(frame));
  }
  for (Document* document : documents) {
    HashSet<String> urls_to_fetch;

    ResourceRequest resource_request;
    HistoryItem* item =
        document->Loader() ? document->Loader()->GetHistoryItem() : nullptr;
    if (item) {
      resource_request = item->GenerateResourceRequest(
          WebCachePolicy::kReturnCacheDataDontLoad);
    } else {
      resource_request = ResourceRequest(document->Url());
      resource_request.SetCachePolicy(WebCachePolicy::kReturnCacheDataDontLoad);
    }
    resource_request.SetRequestContext(WebURLRequest::kRequestContextInternal);

    if (!resource_request.Url().GetString().IsEmpty()) {
      urls_to_fetch.insert(resource_request.Url().GetString());
      ResourceLoaderOptions options;
      options.initiator_info.name = FetchInitiatorTypeNames::internal;
      FetchParameters params(resource_request, options);
      Resource* resource = RawResource::Fetch(params, document->Fetcher());
      if (resource) {
        // Prevent garbage collection by holding a reference to this resource.
        resources_.push_back(resource);
        ResourceClient* resource_client = new ResourceClient(this);
        pending_resource_clients_.insert(resource_client);
        resource_client->WaitForResource(resource);
      }
    }

    HeapVector<Member<CSSStyleSheet>> style_sheets;
    InspectorCSSAgent::CollectAllDocumentStyleSheets(document, style_sheets);
    for (CSSStyleSheet* style_sheet : style_sheets) {
      if (style_sheet->IsInline() || !style_sheet->Contents()->LoadCompleted())
        continue;
      String url = style_sheet->href();
      if (url.IsEmpty() || urls_to_fetch.Contains(url))
        continue;
      urls_to_fetch.insert(url);
      ResourceRequest resource_request(url);
      resource_request.SetRequestContext(
          WebURLRequest::kRequestContextInternal);
      ResourceLoaderOptions options;
      options.initiator_info.name = FetchInitiatorTypeNames::internal;
      FetchParameters params(resource_request, options);
      Resource* resource =
          CSSStyleSheetResource::Fetch(params, document->Fetcher());
      if (!resource)
        continue;
      // Prevent garbage collection by holding a reference to this resource.
      resources_.push_back(resource);
      ResourceClient* resource_client = new ResourceClient(this);
      pending_resource_clients_.insert(resource_client);
      resource_client->WaitForResource(resource);
    }
  }

  all_requests_started_ = true;
  CheckDone();
}

int InspectorResourceContentLoader::CreateClientId() {
  return ++last_client_id_;
}

void InspectorResourceContentLoader::EnsureResourcesContentLoaded(
    int client_id,
    WTF::Closure callback) {
  if (!started_)
    Start();
  callbacks_.insert(client_id, Callbacks())
      .stored_value->value.push_back(std::move(callback));
  CheckDone();
}

void InspectorResourceContentLoader::Cancel(int client_id) {
  callbacks_.erase(client_id);
}

InspectorResourceContentLoader::~InspectorResourceContentLoader() {
  DCHECK(resources_.IsEmpty());
}

DEFINE_TRACE(InspectorResourceContentLoader) {
  visitor->Trace(inspected_frame_);
  visitor->Trace(pending_resource_clients_);
  visitor->Trace(resources_);
}

void InspectorResourceContentLoader::DidCommitLoadForLocalFrame(
    LocalFrame* frame) {
  if (frame == inspected_frame_)
    Stop();
}

Resource* InspectorResourceContentLoader::ResourceForURL(const KURL& url) {
  for (const auto& resource : resources_) {
    if (resource->Url() == url)
      return resource;
  }
  return nullptr;
}

void InspectorResourceContentLoader::Dispose() {
  Stop();
}

void InspectorResourceContentLoader::Stop() {
  HeapHashSet<Member<ResourceClient>> pending_resource_clients;
  pending_resource_clients_.swap(pending_resource_clients);
  for (const auto& client : pending_resource_clients)
    client->loader_ = nullptr;
  resources_.clear();
  // Make sure all callbacks are called to prevent infinite waiting time.
  CheckDone();
  all_requests_started_ = false;
  started_ = false;
}

bool InspectorResourceContentLoader::HasFinished() {
  return all_requests_started_ && pending_resource_clients_.size() == 0;
}

void InspectorResourceContentLoader::CheckDone() {
  if (!HasFinished())
    return;
  HashMap<int, Callbacks> callbacks;
  callbacks.swap(callbacks_);
  for (const auto& key_value : callbacks) {
    for (const auto& callback : key_value.value)
      callback();
  }
}

void InspectorResourceContentLoader::ResourceFinished(ResourceClient* client) {
  pending_resource_clients_.erase(client);
  CheckDone();
}

}  // namespace blink
