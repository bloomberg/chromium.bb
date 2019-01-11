// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

namespace blink {

// This is a CachedMetadataSender implementation for normal responses.
class CachedMetadataSenderImpl : public CachedMetadataSender {
 public:
  CachedMetadataSenderImpl(const ResourceResponse&,
                           blink::mojom::CodeCacheType);
  ~CachedMetadataSenderImpl() override = default;

  void Send(const uint8_t*, size_t) override;
  bool IsServedFromCacheStorage() override { return false; }

 private:
  const KURL response_url_;
  const Time response_time_;
  const blink::mojom::CodeCacheType code_cache_type_;
};

CachedMetadataSenderImpl::CachedMetadataSenderImpl(
    const ResourceResponse& response,
    blink::mojom::CodeCacheType code_cache_type)
    : response_url_(response.CurrentRequestUrl()),
      response_time_(response.ResponseTime()),
      code_cache_type_(code_cache_type) {
  DCHECK(response.CacheStorageCacheName().IsNull());
}

void CachedMetadataSenderImpl::Send(const uint8_t* data, size_t size) {
  Platform::Current()->CacheMetadata(code_cache_type_, response_url_,
                                     response_time_, data, size);
}

// This is a CachedMetadataSender implementation that does nothing.
class NullCachedMetadataSender : public CachedMetadataSender {
 public:
  NullCachedMetadataSender() = default;
  ~NullCachedMetadataSender() override = default;

  void Send(const uint8_t*, size_t) override {}
  bool IsServedFromCacheStorage() override { return false; }
};

// This is a CachedMetadataSender implementation for responses that are served
// by a ServiceWorker from cache storage.
class ServiceWorkerCachedMetadataSender : public CachedMetadataSender {
 public:
  ServiceWorkerCachedMetadataSender(const ResourceResponse&,
                                    scoped_refptr<const SecurityOrigin>);
  ~ServiceWorkerCachedMetadataSender() override = default;

  void Send(const uint8_t*, size_t) override;
  bool IsServedFromCacheStorage() override { return true; }

 private:
  const KURL response_url_;
  const Time response_time_;
  const String cache_storage_cache_name_;
  scoped_refptr<const SecurityOrigin> security_origin_;
};

ServiceWorkerCachedMetadataSender::ServiceWorkerCachedMetadataSender(
    const ResourceResponse& response,
    scoped_refptr<const SecurityOrigin> security_origin)
    : response_url_(response.CurrentRequestUrl()),
      response_time_(response.ResponseTime()),
      cache_storage_cache_name_(response.CacheStorageCacheName()),
      security_origin_(std::move(security_origin)) {
  DCHECK(!cache_storage_cache_name_.IsNull());
}

void ServiceWorkerCachedMetadataSender::Send(const uint8_t* data, size_t size) {
  Platform::Current()->CacheMetadataInCacheStorage(
      response_url_, response_time_, data, size,
      WebSecurityOrigin(security_origin_), cache_storage_cache_name_);
}

// static
std::unique_ptr<CachedMetadataSender> CachedMetadataSender::Create(
    const ResourceResponse& response,
    blink::mojom::CodeCacheType code_cache_type,
    scoped_refptr<const SecurityOrigin> requestor_origin) {
  if (response.WasFetchedViaServiceWorker()) {
    // TODO(leszeks): Check whether it's correct that |requestor_origin| can be
    // nullptr.
    if (!requestor_origin || response.CacheStorageCacheName().IsNull())
      return std::make_unique<NullCachedMetadataSender>();
    return std::make_unique<ServiceWorkerCachedMetadataSender>(
        response, std::move(requestor_origin));
  }
  return std::make_unique<CachedMetadataSenderImpl>(response, code_cache_type);
}

}  // namespace blink
