// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"

#include "platform/blob/BlobData.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/RefCounted.h"
#include "public/platform/WebHTTPHeaderVisitor.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class WebServiceWorkerRequestPrivate
    : public RefCounted<WebServiceWorkerRequestPrivate> {
 public:
  WebServiceWorkerRequestPrivate()
      : mode_(WebURLRequest::kFetchRequestModeNoCORS),
        is_main_resource_load_(false),
        credentials_mode_(WebURLRequest::kFetchCredentialsModeOmit),
        cache_mode_(WebURLRequest::kFetchRequestCacheModeDefault),
        redirect_mode_(WebURLRequest::kFetchRedirectModeFollow),
        request_context_(WebURLRequest::kRequestContextUnspecified),
        frame_type_(WebURLRequest::kFrameTypeNone),
        client_id_(WebString()),
        is_reload_(false) {}
  WebURL url_;
  WebString method_;
  HTTPHeaderMap headers_;
  RefPtr<BlobDataHandle> blob_data_handle;
  Referrer referrer_;
  WebURLRequest::FetchRequestMode mode_;
  bool is_main_resource_load_;
  WebURLRequest::FetchCredentialsMode credentials_mode_;
  WebURLRequest::FetchRequestCacheMode cache_mode_;
  WebURLRequest::FetchRedirectMode redirect_mode_;
  WebURLRequest::RequestContext request_context_;
  WebURLRequest::FrameType frame_type_;
  WebString integrity_;
  WebString client_id_;
  bool is_reload_;
};

WebServiceWorkerRequest::WebServiceWorkerRequest()
    : private_(WTF::AdoptRef(new WebServiceWorkerRequestPrivate)) {}

void WebServiceWorkerRequest::Reset() {
  private_.Reset();
}

void WebServiceWorkerRequest::Assign(const WebServiceWorkerRequest& other) {
  private_ = other.private_;
}

void WebServiceWorkerRequest::SetURL(const WebURL& url) {
  private_->url_ = url;
}

const WebString& WebServiceWorkerRequest::Integrity() const {
  return private_->integrity_;
}

const WebURL& WebServiceWorkerRequest::Url() const {
  return private_->url_;
}

void WebServiceWorkerRequest::SetMethod(const WebString& method) {
  private_->method_ = method;
}

const WebString& WebServiceWorkerRequest::Method() const {
  return private_->method_;
}

void WebServiceWorkerRequest::SetHeader(const WebString& key,
                                        const WebString& value) {
  if (DeprecatedEqualIgnoringCase(key, "referer"))
    return;
  private_->headers_.Set(key, value);
}

void WebServiceWorkerRequest::AppendHeader(const WebString& key,
                                           const WebString& value) {
  if (DeprecatedEqualIgnoringCase(key, "referer"))
    return;
  HTTPHeaderMap::AddResult result = private_->headers_.Add(key, value);
  if (!result.is_new_entry)
    result.stored_value->value =
        result.stored_value->value + ", " + String(value);
}

void WebServiceWorkerRequest::VisitHTTPHeaderFields(
    WebHTTPHeaderVisitor* header_visitor) const {
  for (HTTPHeaderMap::const_iterator i = private_->headers_.begin(),
                                     end = private_->headers_.end();
       i != end; ++i)
    header_visitor->VisitHeader(i->key, i->value);
}

const HTTPHeaderMap& WebServiceWorkerRequest::Headers() const {
  return private_->headers_;
}

void WebServiceWorkerRequest::SetBlob(const WebString& uuid,
                                      long long size,
                                      mojo::ScopedMessagePipeHandle blob_pipe) {
  SetBlob(uuid, size,
          mojom::blink::BlobPtrInfo(std::move(blob_pipe),
                                    mojom::blink::Blob::Version_));
}

void WebServiceWorkerRequest::SetBlob(const WebString& uuid,
                                      long long size,
                                      mojom::blink::BlobPtrInfo blob_info) {
  private_->blob_data_handle =
      BlobDataHandle::Create(uuid, String(), size, std::move(blob_info));
}

RefPtr<BlobDataHandle> WebServiceWorkerRequest::GetBlobDataHandle() const {
  return private_->blob_data_handle;
}

void WebServiceWorkerRequest::SetReferrer(const WebString& web_referrer,
                                          WebReferrerPolicy referrer_policy) {
  // WebString doesn't have the distinction between empty and null. We use
  // the null WTFString for referrer.
  DCHECK_EQ(Referrer::NoReferrer(), String());
  String referrer =
      web_referrer.IsEmpty() ? Referrer::NoReferrer() : String(web_referrer);
  private_->referrer_ =
      Referrer(referrer, static_cast<ReferrerPolicy>(referrer_policy));
}

WebURL WebServiceWorkerRequest::ReferrerUrl() const {
  return KURL(private_->referrer_.referrer);
}

WebReferrerPolicy WebServiceWorkerRequest::GetReferrerPolicy() const {
  return static_cast<WebReferrerPolicy>(private_->referrer_.referrer_policy);
}

const Referrer& WebServiceWorkerRequest::GetReferrer() const {
  return private_->referrer_;
}

void WebServiceWorkerRequest::SetMode(WebURLRequest::FetchRequestMode mode) {
  private_->mode_ = mode;
}

WebURLRequest::FetchRequestMode WebServiceWorkerRequest::Mode() const {
  return private_->mode_;
}

void WebServiceWorkerRequest::SetIsMainResourceLoad(
    bool is_main_resource_load) {
  private_->is_main_resource_load_ = is_main_resource_load;
}

bool WebServiceWorkerRequest::IsMainResourceLoad() const {
  return private_->is_main_resource_load_;
}

void WebServiceWorkerRequest::SetCredentialsMode(
    WebURLRequest::FetchCredentialsMode credentials_mode) {
  private_->credentials_mode_ = credentials_mode;
}

void WebServiceWorkerRequest::SetIntegrity(const WebString& integrity) {
  private_->integrity_ = integrity;
}

WebURLRequest::FetchCredentialsMode WebServiceWorkerRequest::CredentialsMode()
    const {
  return private_->credentials_mode_;
}

void WebServiceWorkerRequest::SetCacheMode(
    WebURLRequest::FetchRequestCacheMode cache_mode) {
  private_->cache_mode_ = cache_mode;
}

WebURLRequest::FetchRequestCacheMode WebServiceWorkerRequest::CacheMode()
    const {
  return private_->cache_mode_;
}

void WebServiceWorkerRequest::SetRedirectMode(
    WebURLRequest::FetchRedirectMode redirect_mode) {
  private_->redirect_mode_ = redirect_mode;
}

WebURLRequest::FetchRedirectMode WebServiceWorkerRequest::RedirectMode() const {
  return private_->redirect_mode_;
}

void WebServiceWorkerRequest::SetRequestContext(
    WebURLRequest::RequestContext request_context) {
  private_->request_context_ = request_context;
}

WebURLRequest::RequestContext WebServiceWorkerRequest::GetRequestContext()
    const {
  return private_->request_context_;
}

void WebServiceWorkerRequest::SetFrameType(
    WebURLRequest::FrameType frame_type) {
  private_->frame_type_ = frame_type;
}

WebURLRequest::FrameType WebServiceWorkerRequest::GetFrameType() const {
  return private_->frame_type_;
}

void WebServiceWorkerRequest::SetClientId(const WebString& client_id) {
  private_->client_id_ = client_id;
}

const WebString& WebServiceWorkerRequest::ClientId() const {
  return private_->client_id_;
}

void WebServiceWorkerRequest::SetIsReload(bool is_reload) {
  private_->is_reload_ = is_reload;
}

bool WebServiceWorkerRequest::IsReload() const {
  return private_->is_reload_;
}

}  // namespace blink
