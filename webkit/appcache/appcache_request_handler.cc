// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_request_handler.h"

#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "webkit/appcache/appcache.h"

namespace appcache {

// AppCacheRequestHandler -----------------------------------------------------

static bool IsHttpOrHttpsGetOrEquivalent(URLRequest* request) {
  return false;  // TODO(michaeln): write me
}

AppCacheRequestHandler::AppCacheRequestHandler(AppCacheHost* host)
    : is_main_request_(true), cache_id_(kNoCacheId),
      host_(host->AsWeakPtr()), service_(host->service()) {
}

AppCacheRequestHandler::AppCacheRequestHandler(AppCache* cache)
    : is_main_request_(false), cache_id_(kNoCacheId),
      cache_(cache), service_(cache->service()) {
}

void AppCacheRequestHandler::GetExtraResponseInfo(
    int64* cache_id, GURL* manifest_url) {
  // TODO(michaeln): If this is a main request and it was retrieved from
  // an appcache, ensure that appcache survives the frame navigation. The
  // AppCacheHost should hold reference to that cache to prevent it from
  // being dropped from the in-memory collection of AppCaches. When cache
  // selection occurs, that extra reference should be dropped. Perhaps
  // maybe: if (is_main) host->LoadCacheOfMainResource(cache_id);
}

URLRequestJob* AppCacheRequestHandler::MaybeLoadResource(URLRequest* request) {
  if (!IsHttpOrHttpsGetOrEquivalent(request))
    return NULL;
  // TODO(michaeln): write me
  return NULL;
}

URLRequestJob* AppCacheRequestHandler::MaybeLoadFallbackForRedirect(
    URLRequest* request, const GURL& location) {
  if (!IsHttpOrHttpsGetOrEquivalent(request))
    return NULL;
  // TODO(michaeln): write me
  return NULL;
}

URLRequestJob* AppCacheRequestHandler::MaybeLoadFallbackForResponse(
    URLRequest* request) {
  if (!IsHttpOrHttpsGetOrEquivalent(request))
    return NULL;
  // TODO(michaeln): write me
  return NULL;
}

}  // namespace appcache

