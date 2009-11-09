// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_interceptor.h"

#include "webkit/appcache/appcache_backend_impl.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/appcache/appcache_request_handler.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/appcache_url_request_job.h"

namespace appcache {

void AppCacheInterceptor::SetHandler(
    URLRequest* request, AppCacheRequestHandler* handler) {
  request->SetUserData(instance(), handler);  // request takes ownership
}

AppCacheRequestHandler* AppCacheInterceptor::GetHandler(URLRequest* request) {
  return reinterpret_cast<AppCacheRequestHandler*>(
      request->GetUserData(instance()));
}

void AppCacheInterceptor::SetExtraRequestInfo(
    URLRequest* request, AppCacheService* service, int process_id,
    int host_id, ResourceType::Type resource_type) {
  if (!service || (host_id == kNoHostId))
    return;

  // TODO(michaeln): An invalid host id is indicative of bad data
  // from a child process. How should we handle that here?
  AppCacheHost* host = service->GetBackend(process_id)->GetHost(host_id);
  if (!host)
    return;

  // TODO(michaeln): SHARED_WORKER type too
  bool is_main_request = ResourceType::IsFrame(resource_type);

  // Create a handler for this request and associate it with the request.
  AppCacheRequestHandler* handler =
      host->CreateRequestHandler(request, is_main_request);
  if (handler)
    SetHandler(request, handler);
}

void AppCacheInterceptor::GetExtraResponseInfo(URLRequest* request,
                                               int64* cache_id,
                                               GURL* manifest_url) {
  DCHECK(*cache_id == kNoCacheId);
  DCHECK(manifest_url->is_empty());
  AppCacheRequestHandler* handler = GetHandler(request);
  if (handler)
    handler->GetExtraResponseInfo(cache_id, manifest_url);
}

AppCacheInterceptor::AppCacheInterceptor() {
  URLRequest::RegisterRequestInterceptor(this);
}

AppCacheInterceptor::~AppCacheInterceptor() {
  URLRequest::UnregisterRequestInterceptor(this);
}

URLRequestJob* AppCacheInterceptor::MaybeIntercept(URLRequest* request) {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return NULL;
  return handler->MaybeLoadResource(request);
}

URLRequestJob* AppCacheInterceptor::MaybeInterceptRedirect(
                                        URLRequest* request,
                                        const GURL& location) {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return NULL;
  return handler->MaybeLoadFallbackForRedirect(request, location);
}

URLRequestJob* AppCacheInterceptor::MaybeInterceptResponse(
                                        URLRequest* request) {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return NULL;
  return handler->MaybeLoadFallbackForResponse(request);
}

}  // namespace appcache
