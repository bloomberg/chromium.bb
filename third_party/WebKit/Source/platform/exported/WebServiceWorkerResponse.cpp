// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"

#include "platform/blob/BlobData.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Time.h"
#include "public/platform/WebHTTPHeaderVisitor.h"

namespace blink {

class WebServiceWorkerResponsePrivate
    : public RefCounted<WebServiceWorkerResponsePrivate> {
 public:
  WebServiceWorkerResponsePrivate()
      : status(0),
        response_type(kWebServiceWorkerResponseTypeDefault),
        error(kWebServiceWorkerResponseErrorUnknown) {}
  WebVector<WebURL> url_list;
  unsigned short status;
  WebString status_text;
  WebServiceWorkerResponseType response_type;
  HTTPHeaderMap headers;
  RefPtr<BlobDataHandle> blob_data_handle;
  WebServiceWorkerResponseError error;
  Time response_time;
  WebString cache_storage_cache_name;
  WebVector<WebString> cors_exposed_header_names;
};

WebServiceWorkerResponse::WebServiceWorkerResponse()
    : private_(AdoptRef(new WebServiceWorkerResponsePrivate)) {}

void WebServiceWorkerResponse::Reset() {
  private_.Reset();
}

void WebServiceWorkerResponse::Assign(const WebServiceWorkerResponse& other) {
  private_ = other.private_;
}

void WebServiceWorkerResponse::SetURLList(const WebVector<WebURL>& url_list) {
  private_->url_list = url_list;
}

const WebVector<WebURL>& WebServiceWorkerResponse::UrlList() const {
  return private_->url_list;
}

void WebServiceWorkerResponse::SetStatus(unsigned short status) {
  private_->status = status;
}

unsigned short WebServiceWorkerResponse::Status() const {
  return private_->status;
}

void WebServiceWorkerResponse::SetStatusText(const WebString& status_text) {
  private_->status_text = status_text;
}

const WebString& WebServiceWorkerResponse::StatusText() const {
  return private_->status_text;
}

void WebServiceWorkerResponse::SetResponseType(
    WebServiceWorkerResponseType response_type) {
  private_->response_type = response_type;
}

WebServiceWorkerResponseType WebServiceWorkerResponse::ResponseType() const {
  return private_->response_type;
}

void WebServiceWorkerResponse::SetHeader(const WebString& key,
                                         const WebString& value) {
  private_->headers.Set(key, value);
}

void WebServiceWorkerResponse::AppendHeader(const WebString& key,
                                            const WebString& value) {
  HTTPHeaderMap::AddResult add_result = private_->headers.Add(key, value);
  if (!add_result.is_new_entry)
    add_result.stored_value->value =
        add_result.stored_value->value + ", " + String(value);
}

WebVector<WebString> WebServiceWorkerResponse::GetHeaderKeys() const {
  Vector<String> keys;
  for (HTTPHeaderMap::const_iterator it = private_->headers.begin(),
                                     end = private_->headers.end();
       it != end; ++it)
    keys.push_back(it->key);

  return keys;
}

WebString WebServiceWorkerResponse::GetHeader(const WebString& key) const {
  return private_->headers.Get(key);
}

void WebServiceWorkerResponse::VisitHTTPHeaderFields(
    WebHTTPHeaderVisitor* header_visitor) const {
  for (HTTPHeaderMap::const_iterator i = private_->headers.begin(),
                                     end = private_->headers.end();
       i != end; ++i)
    header_visitor->VisitHeader(i->key, i->value);
}

void WebServiceWorkerResponse::SetBlob(const WebString& uuid, uint64_t size) {
  private_->blob_data_handle = BlobDataHandle::Create(uuid, String(), size);
}

WebString WebServiceWorkerResponse::BlobUUID() const {
  if (!private_->blob_data_handle)
    return WebString();
  return private_->blob_data_handle->Uuid();
}

uint64_t WebServiceWorkerResponse::BlobSize() const {
  if (!private_->blob_data_handle)
    return 0;
  return private_->blob_data_handle->size();
}

void WebServiceWorkerResponse::SetError(WebServiceWorkerResponseError error) {
  private_->error = error;
}

WebServiceWorkerResponseError WebServiceWorkerResponse::GetError() const {
  return private_->error;
}

void WebServiceWorkerResponse::SetResponseTime(base::Time time) {
  private_->response_time = time;
}

base::Time WebServiceWorkerResponse::ResponseTime() const {
  return private_->response_time;
}

void WebServiceWorkerResponse::SetCacheStorageCacheName(
    const WebString& cache_storage_cache_name) {
  private_->cache_storage_cache_name = cache_storage_cache_name;
}

const WebString& WebServiceWorkerResponse::CacheStorageCacheName() const {
  return private_->cache_storage_cache_name;
}

void WebServiceWorkerResponse::SetCorsExposedHeaderNames(
    const WebVector<WebString>& header_names) {
  private_->cors_exposed_header_names = header_names;
}

const WebVector<WebString>& WebServiceWorkerResponse::CorsExposedHeaderNames()
    const {
  return private_->cors_exposed_header_names;
}

const HTTPHeaderMap& WebServiceWorkerResponse::Headers() const {
  return private_->headers;
}

void WebServiceWorkerResponse::SetBlobDataHandle(
    PassRefPtr<BlobDataHandle> blob_data_handle) {
  private_->blob_data_handle = std::move(blob_data_handle);
}

PassRefPtr<BlobDataHandle> WebServiceWorkerResponse::GetBlobDataHandle() const {
  return private_->blob_data_handle;
}

}  // namespace blink
