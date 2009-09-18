// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_request_handler.h"

#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "webkit/appcache/appcache.h"

namespace appcache {

// AppCacheRequestHandler -----------------------------------------------------

AppCacheRequestHandler::AppCacheRequestHandler(AppCacheHost* host,
                                               bool is_main_resource)
    : is_main_request_(is_main_resource), host_(host) {
  DCHECK(host_);
  host_->AddObserver(this);
}

AppCacheRequestHandler::~AppCacheRequestHandler() {
  if (host_)
    host_->RemoveObserver(this);
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
  // 6.9.7 Changes to the networking model
  // If the resource is not to be fetched using the HTTP GET mechanism or
  // equivalent ... then fetch the resource normally
  if (!host_ || !IsSchemeAndMethodSupported(request))
    return NULL;
  // TODO(michaeln): write me
  return NULL;
}

URLRequestJob* AppCacheRequestHandler::MaybeLoadFallbackForRedirect(
    URLRequest* request, const GURL& location) {
  if (!host_ || !IsSchemeAndMethodSupported(request))
    return NULL;
  // TODO(michaeln): write me
  return NULL;
}

URLRequestJob* AppCacheRequestHandler::MaybeLoadFallbackForResponse(
    URLRequest* request) {
  if (!host_ || !IsSchemeAndMethodSupported(request))
    return NULL;
  // TODO(michaeln): write me
  return NULL;
}

void AppCacheRequestHandler::OnCacheSelectionComplete(AppCacheHost* host) {
  DCHECK(host == host_);
  // TODO(michaeln): write me
}

void AppCacheRequestHandler::OnDestructionImminent(AppCacheHost* host) {
  host_ = NULL;
  // TODO(michaeln): write me
}

}  // namespace appcache

