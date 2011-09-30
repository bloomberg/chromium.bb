// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_REQUEST_HANDLER_H_
#define WEBKIT_APPCACHE_APPCACHE_REQUEST_HANDLER_H_

#include "base/compiler_specific.h"
#include "net/url_request/url_request.h"
#include "webkit/appcache/appcache_entry.h"
#include "webkit/appcache/appcache_export.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/glue/resource_type.h"

namespace net {
class URLRequest;
class URLRequestJob;
}  // namespace net

namespace appcache {

class AppCacheURLRequestJob;

// An instance is created for each net::URLRequest. The instance survives all
// http transactions involved in the processing of its net::URLRequest, and is
// given the opportunity to hijack the request along the way. Callers
// should use AppCacheHost::CreateRequestHandler to manufacture instances
// that can retrieve resources for a particular host.
class APPCACHE_EXPORT AppCacheRequestHandler
    : NON_EXPORTED_BASE(public net::URLRequest::UserData),
      public AppCacheHost::Observer,
      public AppCacheStorage::Delegate  {
 public:
  virtual ~AppCacheRequestHandler();

  // These are called on each request intercept opportunity.
  AppCacheURLRequestJob* MaybeLoadResource(net::URLRequest* request);
  AppCacheURLRequestJob* MaybeLoadFallbackForRedirect(net::URLRequest* request,
                                                      const GURL& location);
  AppCacheURLRequestJob* MaybeLoadFallbackForResponse(net::URLRequest* request);

  void GetExtraResponseInfo(int64* cache_id, GURL* manifest_url);

  static bool IsMainResourceType(ResourceType::Type type) {
    return ResourceType::IsFrame(type) ||
           ResourceType::IsSharedWorker(type);
  }

 private:
  friend class AppCacheHost;

  // Callers should use AppCacheHost::CreateRequestHandler.
  AppCacheRequestHandler(AppCacheHost* host, ResourceType::Type resource_type);

  // AppCacheHost::Observer override
  virtual void OnDestructionImminent(AppCacheHost* host);

  // Helpers to instruct a waiting job with what response to
  // deliver for the request we're handling.
  void DeliverAppCachedResponse(const AppCacheEntry& entry, int64 cache_id,
                                const GURL& manifest_url, bool is_fallback,
                                const GURL& fallback_url);
  void DeliverNetworkResponse();
  void DeliverErrorResponse();

  // Helper to retrieve a pointer to the storage object.
  AppCacheStorage* storage() const;

  bool is_main_resource() const {
    return IsMainResourceType(resource_type_);
  }

  // Main-resource loading -------------------------------------
  // Frame and SharedWorker main resources are handled here.

  void MaybeLoadMainResource(net::URLRequest* request);

  // AppCacheStorage::Delegate methods
  virtual void OnMainResponseFound(
      const GURL& url, const AppCacheEntry& entry,
      const GURL& fallback_url, const AppCacheEntry& fallback_entry,
      int64 cache_id, const GURL& mainfest_url);

  // Sub-resource loading -------------------------------------
  // Dedicated worker and all manner of sub-resources are handled here.

  void MaybeLoadSubResource(net::URLRequest* request);
  void ContinueMaybeLoadSubResource();

  // AppCacheHost::Observer override
  virtual void OnCacheSelectionComplete(AppCacheHost* host);

  // Data members -----------------------------------------------

  // What host we're servicing a request for.
  AppCacheHost* host_;

  // Frame vs subresource vs sharedworker loads are somewhat different.
  ResourceType::Type resource_type_;

  // Subresource requests wait until after cache selection completes.
  bool is_waiting_for_cache_selection_;

  // Info about the type of response we found for delivery.
  // These are relevant for both main and subresource requests.
  int64 found_cache_id_;
  AppCacheEntry found_entry_;
  AppCacheEntry found_fallback_entry_;
  GURL found_fallback_url_;
  GURL found_manifest_url_;
  bool found_network_namespace_;

  // True if a cache entry this handler attempted to return was
  // not found in the disk cache. Once set, the handler will take
  // no action on all subsequent intercept opportunities, so the
  // request and any redirects will be handled by the network library.
  bool cache_entry_not_found_;

  // The job we use to deliver a response.
  scoped_refptr<AppCacheURLRequestJob> job_;

  friend class AppCacheRequestHandlerTest;
  DISALLOW_COPY_AND_ASSIGN(AppCacheRequestHandler);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_REQUEST_HANDLER_H_
