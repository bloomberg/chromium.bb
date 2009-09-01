// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_interceptor.h"

#include "webkit/appcache/appcache_backend_impl.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/appcache/appcache_service.h"

namespace appcache {

// Extra info we associate with requests for use at MaybeIntercept time. This
// info is deleted when the URLRequest is deleted which occurs after the
// request is complete and all data has been read.
struct AppCacheInterceptor::ExtraInfo : public URLRequest::UserData {
  // Inputs, extra request info
  AppCacheService* service;
  int process_id;
  int host_id;
  ResourceType::Type resource_type;

  // Outputs, extra response info
  int64 cache_id;
  GURL manifest_url;

  // The host associated with the request
  // TODO(michaeln): Be careful with this data member, its not clear
  // if a URLRequest can outlive the associated host. As we get further
  // along, we'll need to notify reqeust waiting on cache selection to
  // allow them to continue upon completion of selection. But we also need
  // to handle navigating away from the page away prior to selection being
  // complete.
  AppCacheHost* host_;

  ExtraInfo(AppCacheService* service, int process_id, int host_id,
            ResourceType::Type resource_type, AppCacheHost* host)
      : service(service), process_id(process_id), host_id(host_id),
        resource_type(resource_type), cache_id(kNoCacheId), host_(host) {
  }

  static void SetInfo(URLRequest* request, ExtraInfo* info) {
    request->SetUserData(instance(), info);  // request takes ownership
  }

  static ExtraInfo* GetInfo(URLRequest* request) {
    return static_cast<ExtraInfo*>(request->GetUserData(instance()));
  }
};

static bool IsMainRequest(ResourceType::Type type) {
  // TODO(michaeln): SHARED_WORKER type?
  return ResourceType::IsFrame(type);
}

void AppCacheInterceptor::SetExtraRequestInfo(
    URLRequest* request, AppCacheService* service, int process_id,
    int host_id, ResourceType::Type resource_type) {
  if (service && (host_id != kNoHostId)) {
    AppCacheHost* host = service->GetBackend(process_id)->GetHost(host_id);
    DCHECK(host);
    if (IsMainRequest(resource_type) || host->selected_cache() ||
        host->is_selection_pending()) {
      ExtraInfo* info = new ExtraInfo(service, process_id,
                                       host_id, resource_type, host);
      ExtraInfo::SetInfo(request, info);
    }
  }
}

void AppCacheInterceptor::GetExtraResponseInfo(URLRequest* request,
                                               int64* cache_id,
                                               GURL* manifest_url) {
  ExtraInfo* info = ExtraInfo::GetInfo(request);
  if (info) {
    // TODO(michaeln): If this is a main request and it was retrieved from
    // an appcache, ensure that appcache survives the frame navigation. The
    // AppCacheHost should hold reference to that cache to prevent it from
    // being dropped from the in-memory collection of AppCaches. When cache
    // selection occurs, that extra reference should be dropped.
    *cache_id = info->cache_id;
    *manifest_url = info->manifest_url;
  } else {
    DCHECK(*cache_id == kNoCacheId);
    DCHECK(manifest_url->is_empty());
  }
}

AppCacheInterceptor::AppCacheInterceptor() {
  URLRequest::RegisterRequestInterceptor(this);
}

AppCacheInterceptor::~AppCacheInterceptor() {
  URLRequest::UnregisterRequestInterceptor(this);
}

URLRequestJob* AppCacheInterceptor::MaybeIntercept(URLRequest* request) {
  ExtraInfo* info = ExtraInfo::GetInfo(request);
  if (!info)
    return NULL;
  // TODO(michaeln): write me
  return NULL;
}

URLRequestJob* AppCacheInterceptor::MaybeInterceptRedirect(
                                        URLRequest* request,
                                        const GURL& location) {
  ExtraInfo* info = ExtraInfo::GetInfo(request);
  if (!info)
    return NULL;
  // TODO(michaeln): write me
  return NULL;
}

URLRequestJob* AppCacheInterceptor::MaybeInterceptResponse(
                                        URLRequest* request) {
  ExtraInfo* info = ExtraInfo::GetInfo(request);
  if (!info)
    return NULL;
  // TODO(michaeln): write me
  return NULL;
}

}  // namespace appcache
