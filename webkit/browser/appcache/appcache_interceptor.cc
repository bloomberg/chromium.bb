// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/appcache/appcache_interceptor.h"

#include "webkit/browser/appcache/appcache_backend_impl.h"
#include "webkit/browser/appcache/appcache_host.h"
#include "webkit/browser/appcache/appcache_request_handler.h"
#include "webkit/browser/appcache/appcache_service.h"
#include "webkit/browser/appcache/appcache_url_request_job.h"
#include "webkit/common/appcache/appcache_interfaces.h"

namespace appcache {

// static
AppCacheInterceptor* AppCacheInterceptor::GetInstance() {
  return Singleton<AppCacheInterceptor>::get();
}

void AppCacheInterceptor::SetHandler(
    net::URLRequest* request, AppCacheRequestHandler* handler) {
  request->SetUserData(GetInstance(), handler);  // request takes ownership
}

AppCacheRequestHandler* AppCacheInterceptor::GetHandler(
    net::URLRequest* request) {
  return reinterpret_cast<AppCacheRequestHandler*>(
      request->GetUserData(GetInstance()));
}

void AppCacheInterceptor::SetExtraRequestInfo(
    net::URLRequest* request, AppCacheService* service, int process_id,
    int host_id, ResourceType::Type resource_type) {
  if (!service || (host_id == kNoHostId))
    return;

  AppCacheBackendImpl* backend = service->GetBackend(process_id);
  if (!backend)
    return;

  // TODO(michaeln): An invalid host id is indicative of bad data
  // from a child process. How should we handle that here?
  AppCacheHost* host = backend->GetHost(host_id);
  if (!host)
    return;

  // Create a handler for this request and associate it with the request.
  AppCacheRequestHandler* handler =
      host->CreateRequestHandler(request, resource_type);
  if (handler)
    SetHandler(request, handler);
}

void AppCacheInterceptor::GetExtraResponseInfo(net::URLRequest* request,
                                               int64* cache_id,
                                               GURL* manifest_url) {
  DCHECK(*cache_id == kNoCacheId);
  DCHECK(manifest_url->is_empty());
  AppCacheRequestHandler* handler = GetHandler(request);
  if (handler)
    handler->GetExtraResponseInfo(cache_id, manifest_url);
}

AppCacheInterceptor::AppCacheInterceptor() {
  net::URLRequest::Deprecated::RegisterRequestInterceptor(this);
}

AppCacheInterceptor::~AppCacheInterceptor() {
  net::URLRequest::Deprecated::UnregisterRequestInterceptor(this);
}

net::URLRequestJob* AppCacheInterceptor::MaybeIntercept(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return NULL;
  return handler->MaybeLoadResource(request, network_delegate);
}

net::URLRequestJob* AppCacheInterceptor::MaybeInterceptRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const GURL& location) {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return NULL;
  return handler->MaybeLoadFallbackForRedirect(
      request, network_delegate, location);
}

net::URLRequestJob* AppCacheInterceptor::MaybeInterceptResponse(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return NULL;
  return handler->MaybeLoadFallbackForResponse(request, network_delegate);
}

}  // namespace appcache
