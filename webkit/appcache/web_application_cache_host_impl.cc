// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/web_application_cache_host_impl.h"

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/api/public/WebURLResponse.h"

using WebKit::WebApplicationCacheHost;
using WebKit::WebApplicationCacheHostClient;
using WebKit::WebURLRequest;
using WebKit::WebURL;
using WebKit::WebURLResponse;

namespace appcache {

static IDMap<WebApplicationCacheHostImpl> all_hosts;

WebApplicationCacheHostImpl* WebApplicationCacheHostImpl::FromId(int id) {
  return all_hosts.Lookup(id);
}

WebApplicationCacheHostImpl::WebApplicationCacheHostImpl(
    WebApplicationCacheHostClient* client,
    AppCacheBackend* backend)
    : client_(client),
      backend_(backend),
      ALLOW_THIS_IN_INITIALIZER_LIST(host_id_(all_hosts.Add(this))),
      has_status_(false),
      status_(UNCACHED),
      has_cached_status_(false),
      cached_status_(UNCACHED),
      is_in_http_family_(false),
      should_capture_main_response_(MAYBE) {
  DCHECK(client && backend && (host_id_ != kNoHostId));

  backend_->RegisterHost(host_id_);
}

WebApplicationCacheHostImpl::~WebApplicationCacheHostImpl() {
  backend_->UnregisterHost(host_id_);
}

void WebApplicationCacheHostImpl::OnCacheSelected(int64 selected_cache_id,
                                                  appcache::Status status) {
  status_ = status;
  has_status_ = true;
}

void WebApplicationCacheHostImpl::OnStatusChanged(appcache::Status status) {
  if (has_status_)
    status_ = status;
}

void WebApplicationCacheHostImpl::OnEventRaised(appcache::EventID event_id) {
  client_->notifyEventListener(static_cast<EventID>(event_id));
}

void WebApplicationCacheHostImpl::willStartMainResourceRequest(
    WebURLRequest& request) {
  request.setAppCacheContextID(host_id_);
}

void WebApplicationCacheHostImpl::willStartSubResourceRequest(
    WebURLRequest& request) {
  request.setAppCacheContextID(host_id_);
}

void WebApplicationCacheHostImpl::selectCacheWithoutManifest() {
  // Reset any previous status values we've received from the backend
  // since we're now selecting a new cache.
  has_status_ = false;
  has_cached_status_ = false;
  should_capture_main_response_ = NO;
  backend_->SelectCache(host_id_, main_response_url_,
                        main_response_.appCacheID(),
                        GURL());
}

bool WebApplicationCacheHostImpl::selectCacheWithManifest(
    const WebURL& manifest_url) {
  // Reset any previous status values we've received from the backend
  // since we're now selecting a new cache.
  has_status_ = false;
  has_cached_status_ = false;

  // Check for new 'master' entries.
  if (main_response_.appCacheID() == kNoCacheId) {
    should_capture_main_response_ = is_in_http_family_ ? YES : NO;
    backend_->SelectCache(host_id_, main_response_url_,
                          kNoCacheId, manifest_url);
    return true;
  }

  // Check for 'foreign' entries.
  // TODO(michaeln): add manifestUrl() accessor to WebURLResponse,
  //                 for now we don't really detect 'foreign' entries.
  // TODO(michaeln): put an == operator on WebURL?
  GURL manifest_gurl(manifest_url);
  GURL main_response_manifest_gurl(manifest_url);  // = mainResp.manifestUrl()
  if (main_response_manifest_gurl != manifest_gurl) {
    backend_->MarkAsForeignEntry(host_id_, main_response_url_,
                                 main_response_.appCacheID());
    selectCacheWithoutManifest();
    return false;  // the navigation will be restarted
  }

  // Its a 'master' entry thats already in the cache.
  backend_->SelectCache(host_id_, main_response_url_,
                        main_response_.appCacheID(),
                        manifest_gurl);
  return true;
}

void WebApplicationCacheHostImpl::didReceiveResponseForMainResource(
    const WebURLResponse& response) {
  main_response_ = response;
  main_response_url_ = main_response_.url();
  is_in_http_family_ =  main_response_url_.SchemeIs("http") ||
                        main_response_url_.SchemeIs("https");
  if ((main_response_.appCacheID() != kNoCacheId) || !is_in_http_family_)
    should_capture_main_response_ = NO;
}

void WebApplicationCacheHostImpl::didReceiveDataForMainResource(
    const char* data, int len) {
  // TODO(michaeln): write me
}

void WebApplicationCacheHostImpl::didFinishLoadingMainResource(bool success) {
  // TODO(michaeln): write me
}

WebApplicationCacheHost::Status WebApplicationCacheHostImpl::status() {
  // We're careful about the status value to avoid race conditions.
  //
  // Generally the webappcachehost sends an async stream of messages to the
  // backend, and receives an asyncronous stream of events from the backend.
  // In the backend, all operations are serialized and as state changes
  // 'events' are streamed out to relevant parties. In particular the
  // 'SelectCache' message is async. Regular page loading and navigation
  // involves two non-blocking ipc calls: RegisterHost + SelectCache.
  //
  // However, the page can call the scriptable API in advance of a cache
  // selection being complete (and/or in advance of the webappcachehost having
  // received the event about completion). In that case, we force an end-to-end
  // fetch of the 'status' value, and cache that value seperately from the
  // value we receive via the async event stream. We'll use that cached value
  // until cache selection is complete.
  if (has_status_)
    return static_cast<WebApplicationCacheHost::Status>(status_);

  if (!has_cached_status_) {
    cached_status_ = backend_->GetStatus(host_id_);
    has_cached_status_ = true;
  }
  return static_cast<WebApplicationCacheHost::Status>(cached_status_);
}

bool WebApplicationCacheHostImpl::startUpdate() {
  return backend_->StartUpdate(host_id_);
}

bool WebApplicationCacheHostImpl::swapCache() {
  return backend_->SwapCache(host_id_);
}

}  // appcache namespace
