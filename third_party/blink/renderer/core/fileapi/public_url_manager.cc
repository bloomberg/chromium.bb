/*
 * Copyright (C) 2012 Motorola Mobility Inc.
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"

#include "base/metrics/histogram_macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/renderer/core/fileapi/url_registry.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/blob/blob_url.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/url_security_origin_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// When a blob URL is created in an opaque origin or something whose
// SecurityOrigin::SerializesAsNull() returns true, the origin is serialized
// into the URL as "null". Since that makes it impossible to parse the origin
// back out and compare it against a context's origin (to check if a context is
// allowed to dereference the URL) we store a map of blob URL to SecurityOrigin
// instance for blob URLs with such the origins.

class BlobURLNullOriginMap final : public URLSecurityOriginMap {
 public:
  BlobURLNullOriginMap();

  // If the given blob URL has a "null" origin, returns SecurityOrigin that
  // represents the "null" origin. Otherwise, returns nullptr.
  SecurityOrigin* GetOrigin(const KURL& blob_url) override;
};

typedef HashMap<String, scoped_refptr<SecurityOrigin>> BlobURLOriginMap;
static ThreadSpecific<BlobURLOriginMap>& OriginMap() {
  // We want to create the BlobURLNullOriginMap exactly once because it is
  // shared by all the threads.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(BlobURLNullOriginMap, cache, ());
  // BlobURLNullOriginMap's constructor does the interesting work.
  (void)cache;

  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<BlobURLOriginMap>, map, ());
  return map;
}

static void SaveToOriginMap(SecurityOrigin* origin, const KURL& blob_url) {
  DCHECK(blob_url.ProtocolIs("blob"));
  DCHECK(!blob_url.HasFragmentIdentifier());

  // If the blob URL contains "null" origin, as in the context with opaque
  // security origin or file URL, save the mapping between url and origin so
  // that the origin can be retrieved when doing security origin check.
  //
  // See the definition of the origin of a Blob URL in the File API spec.
  if (origin && origin->SerializesAsNull()) {
    DCHECK_EQ(BlobURL::GetOrigin(blob_url), "null");
    OriginMap()->insert(blob_url.GetString(), origin);
  }
}

static void RemoveFromOriginMap(const KURL& blob_url) {
  DCHECK(blob_url.ProtocolIs("blob"));
  if (BlobURL::GetOrigin(blob_url) == "null")
    OriginMap()->erase(blob_url.GetString());
}

BlobURLNullOriginMap::BlobURLNullOriginMap() {
  SecurityOrigin::SetBlobURLNullOriginMap(this);
}

SecurityOrigin* BlobURLNullOriginMap::GetOrigin(const KURL& blob_url) {
  DCHECK(blob_url.ProtocolIs("blob"));
  KURL blob_url_without_fragment = blob_url;
  blob_url_without_fragment.RemoveFragmentIdentifier();
  return OriginMap()->at(blob_url_without_fragment.GetString());
}

}  // namespace

PublicURLManager::PublicURLManager(ExecutionContext* context)
    : ContextLifecycleObserver(context), is_stopped_(false) {}

String PublicURLManager::RegisterURL(URLRegistrable* registrable) {
  if (is_stopped_)
    return String();

  SecurityOrigin* origin = GetExecutionContext()->GetMutableSecurityOrigin();
  const KURL& url = BlobURL::CreatePublicURL(origin);
  DCHECK(!url.IsEmpty());
  const String& url_string = url.GetString();

  if (registrable->IsMojoBlob()) {
    // Measure how much jank the following synchronous IPC introduces.
    SCOPED_UMA_HISTOGRAM_TIMER("Storage.Blob.RegisterPublicURLTime");
    if (!url_store_) {
      BlobDataHandle::GetBlobRegistry()->URLStoreForOrigin(
          origin, url_store_.BindNewEndpointAndPassReceiver());
    }
    mojo::PendingRemote<mojom::blink::Blob> blob_remote;
    mojo::PendingReceiver<mojom::blink::Blob> blob_receiver =
        blob_remote.InitWithNewPipeAndPassReceiver();
    url_store_->Register(std::move(blob_remote), url);
    mojo_urls_.insert(url_string);
    registrable->CloneMojoBlob(std::move(blob_receiver));
  } else {
    URLRegistry* registry = &registrable->Registry();
    registry->RegisterURL(origin, url, registrable);
    url_to_registry_.insert(url_string, registry);
  }
  SaveToOriginMap(origin, url);

  return url_string;
}

void PublicURLManager::Revoke(const KURL& url) {
  if (is_stopped_)
    return;
  // Don't bother trying to revoke URLs that can't have been registered anyway.
  if (!url.ProtocolIs("blob") || url.HasFragmentIdentifier())
    return;
  // Don't support revoking cross-origin blob URLs.
  if (!SecurityOrigin::Create(url)->IsSameSchemeHostPort(
          GetExecutionContext()->GetSecurityOrigin()))
    return;

  if (!url_store_) {
    BlobDataHandle::GetBlobRegistry()->URLStoreForOrigin(
        GetExecutionContext()->GetSecurityOrigin(),
        url_store_.BindNewEndpointAndPassReceiver());
  }
  url_store_->Revoke(url);
  mojo_urls_.erase(url.GetString());

  RemoveFromOriginMap(url);
  auto it = url_to_registry_.find(url.GetString());
  if (it == url_to_registry_.end())
    return;
  it->value->UnregisterURL(url);
  url_to_registry_.erase(it);
}

void PublicURLManager::Resolve(
    const KURL& url,
    mojo::PendingReceiver<network::mojom::blink::URLLoaderFactory>
        factory_receiver) {
  if (is_stopped_)
    return;

  DCHECK(url.ProtocolIs("blob"));
  if (!url_store_) {
    BlobDataHandle::GetBlobRegistry()->URLStoreForOrigin(
        GetExecutionContext()->GetSecurityOrigin(),
        url_store_.BindNewEndpointAndPassReceiver());
  }
  url_store_->ResolveAsURLLoaderFactory(url, std::move(factory_receiver));
}

void PublicURLManager::Resolve(
    const KURL& url,
    mojo::PendingReceiver<mojom::blink::BlobURLToken> token_receiver) {
  if (is_stopped_)
    return;

  DCHECK(url.ProtocolIs("blob"));
  if (!url_store_) {
    BlobDataHandle::GetBlobRegistry()->URLStoreForOrigin(
        GetExecutionContext()->GetSecurityOrigin(),
        url_store_.BindNewEndpointAndPassReceiver());
  }
  url_store_->ResolveForNavigation(url, std::move(token_receiver));
}

void PublicURLManager::ContextDestroyed(ExecutionContext*) {
  if (is_stopped_)
    return;

  is_stopped_ = true;
  for (auto& url_registry : url_to_registry_) {
    url_registry.value->UnregisterURL(KURL(url_registry.key));
    RemoveFromOriginMap(KURL(url_registry.key));
  }
  for (const auto& url : mojo_urls_)
    RemoveFromOriginMap(KURL(url));

  url_to_registry_.clear();
  mojo_urls_.clear();

  url_store_.reset();
}

void PublicURLManager::Trace(blink::Visitor* visitor) {
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
