/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/platform/WebURLRequest.h"

#include <memory>
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebHTTPBody.h"
#include "public/platform/WebHTTPHeaderVisitor.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebURL.h"

namespace blink {

namespace {

class URLRequestExtraDataContainer : public ResourceRequest::ExtraData {
 public:
  static RefPtr<URLRequestExtraDataContainer> Create(
      WebURLRequest::ExtraData* extra_data) {
    return WTF::AdoptRef(new URLRequestExtraDataContainer(extra_data));
  }

  ~URLRequestExtraDataContainer() override {}

  WebURLRequest::ExtraData* GetExtraData() const { return extra_data_.get(); }

 private:
  explicit URLRequestExtraDataContainer(WebURLRequest::ExtraData* extra_data)
      : extra_data_(WTF::WrapUnique(extra_data)) {}

  std::unique_ptr<WebURLRequest::ExtraData> extra_data_;
};

}  // namespace

// The purpose of this struct is to permit allocating a ResourceRequest on the
// heap, which is otherwise disallowed by DISALLOW_NEW_EXCEPT_PLACEMENT_NEW
// annotation on ResourceRequest.
struct WebURLRequest::ResourceRequestContainer {
  ResourceRequestContainer() {}
  explicit ResourceRequestContainer(const ResourceRequest& r)
      : resource_request(r) {}

  ResourceRequest resource_request;
};

WebURLRequest::~WebURLRequest() {}

WebURLRequest::WebURLRequest()
    : owned_resource_request_(new ResourceRequestContainer()),
      resource_request_(&owned_resource_request_->resource_request) {}

WebURLRequest::WebURLRequest(const WebURLRequest& r)
    : owned_resource_request_(
          new ResourceRequestContainer(*r.resource_request_)),
      resource_request_(&owned_resource_request_->resource_request) {}

WebURLRequest::WebURLRequest(const WebURL& url) : WebURLRequest() {
  SetURL(url);
}

WebURLRequest& WebURLRequest::operator=(const WebURLRequest& r) {
  // Copying subclasses that have different m_resourceRequest ownership
  // semantics via this operator is just not supported.
  DCHECK(owned_resource_request_);
  DCHECK(resource_request_);
  if (&r != this)
    *resource_request_ = *r.resource_request_;
  return *this;
}

bool WebURLRequest::IsNull() const {
  return resource_request_->IsNull();
}

WebURL WebURLRequest::Url() const {
  return resource_request_->Url();
}

void WebURLRequest::SetURL(const WebURL& url) {
  resource_request_->SetURL(url);
}

WebURL WebURLRequest::SiteForCookies() const {
  return resource_request_->SiteForCookies();
}

void WebURLRequest::SetSiteForCookies(const WebURL& site_for_cookies) {
  resource_request_->SetSiteForCookies(site_for_cookies);
}

WebSecurityOrigin WebURLRequest::RequestorOrigin() const {
  return resource_request_->RequestorOrigin();
}

void WebURLRequest::SetRequestorOrigin(
    const WebSecurityOrigin& requestor_origin) {
  resource_request_->SetRequestorOrigin(requestor_origin);
}

bool WebURLRequest::AllowStoredCredentials() const {
  return resource_request_->AllowStoredCredentials();
}

void WebURLRequest::SetAllowStoredCredentials(bool allow_stored_credentials) {
  resource_request_->SetAllowStoredCredentials(allow_stored_credentials);
}

WebCachePolicy WebURLRequest::GetCachePolicy() const {
  return resource_request_->GetCachePolicy();
}

void WebURLRequest::SetCachePolicy(WebCachePolicy cache_policy) {
  resource_request_->SetCachePolicy(cache_policy);
}

WebString WebURLRequest::HttpMethod() const {
  return resource_request_->HttpMethod();
}

void WebURLRequest::SetHTTPMethod(const WebString& http_method) {
  resource_request_->SetHTTPMethod(http_method);
}

WebString WebURLRequest::HttpHeaderField(const WebString& name) const {
  return resource_request_->HttpHeaderField(name);
}

void WebURLRequest::SetHTTPHeaderField(const WebString& name,
                                       const WebString& value) {
  CHECK(!DeprecatedEqualIgnoringCase(name, "referer"));
  resource_request_->SetHTTPHeaderField(name, value);
}

void WebURLRequest::SetHTTPReferrer(const WebString& web_referrer,
                                    WebReferrerPolicy referrer_policy) {
  // WebString doesn't have the distinction between empty and null. We use
  // the null WTFString for referrer.
  DCHECK_EQ(Referrer::NoReferrer(), String());
  String referrer =
      web_referrer.IsEmpty() ? Referrer::NoReferrer() : String(web_referrer);
  resource_request_->SetHTTPReferrer(
      Referrer(referrer, static_cast<ReferrerPolicy>(referrer_policy)));
}

void WebURLRequest::AddHTTPHeaderField(const WebString& name,
                                       const WebString& value) {
  resource_request_->AddHTTPHeaderField(name, value);
}

void WebURLRequest::ClearHTTPHeaderField(const WebString& name) {
  resource_request_->ClearHTTPHeaderField(name);
}

void WebURLRequest::VisitHTTPHeaderFields(WebHTTPHeaderVisitor* visitor) const {
  const HTTPHeaderMap& map = resource_request_->HttpHeaderFields();
  for (HTTPHeaderMap::const_iterator it = map.begin(); it != map.end(); ++it)
    visitor->VisitHeader(it->key, it->value);
}

WebHTTPBody WebURLRequest::HttpBody() const {
  // TODO(mkwst): This is wrong, as it means that we're producing the body
  // before any ServiceWorker has a chance to operate, which means we're
  // revealing data to the SW that we ought to be hiding. Baby steps.
  // https://crbug.com/599597
  if (resource_request_->AttachedCredential())
    return WebHTTPBody(resource_request_->AttachedCredential());
  return WebHTTPBody(resource_request_->HttpBody());
}

void WebURLRequest::SetHTTPBody(const WebHTTPBody& http_body) {
  resource_request_->SetHTTPBody(http_body);
}

WebHTTPBody WebURLRequest::AttachedCredential() const {
  return WebHTTPBody(resource_request_->AttachedCredential());
}

void WebURLRequest::SetAttachedCredential(
    const WebHTTPBody& attached_credential) {
  resource_request_->SetAttachedCredential(attached_credential);
}

bool WebURLRequest::ReportUploadProgress() const {
  return resource_request_->ReportUploadProgress();
}

void WebURLRequest::SetReportUploadProgress(bool report_upload_progress) {
  resource_request_->SetReportUploadProgress(report_upload_progress);
}

void WebURLRequest::SetReportRawHeaders(bool report_raw_headers) {
  resource_request_->SetReportRawHeaders(report_raw_headers);
}

bool WebURLRequest::ReportRawHeaders() const {
  return resource_request_->ReportRawHeaders();
}

WebURLRequest::RequestContext WebURLRequest::GetRequestContext() const {
  return resource_request_->GetRequestContext();
}

WebURLRequest::FrameType WebURLRequest::GetFrameType() const {
  return resource_request_->GetFrameType();
}

WebReferrerPolicy WebURLRequest::GetReferrerPolicy() const {
  return static_cast<WebReferrerPolicy>(resource_request_->GetReferrerPolicy());
}

void WebURLRequest::AddHTTPOriginIfNeeded(const WebSecurityOrigin& origin) {
  resource_request_->AddHTTPOriginIfNeeded(origin.Get());
}

bool WebURLRequest::HasUserGesture() const {
  return resource_request_->HasUserGesture();
}

void WebURLRequest::SetHasUserGesture(bool has_user_gesture) {
  resource_request_->SetHasUserGesture(has_user_gesture);
}

void WebURLRequest::SetRequestContext(RequestContext request_context) {
  resource_request_->SetRequestContext(request_context);
}

void WebURLRequest::SetFrameType(FrameType frame_type) {
  resource_request_->SetFrameType(frame_type);
}

int WebURLRequest::RequestorID() const {
  return resource_request_->RequestorID();
}

void WebURLRequest::SetRequestorID(int requestor_id) {
  resource_request_->SetRequestorID(requestor_id);
}

int WebURLRequest::RequestorProcessID() const {
  return resource_request_->RequestorProcessID();
}

void WebURLRequest::SetRequestorProcessID(int requestor_process_id) {
  resource_request_->SetRequestorProcessID(requestor_process_id);
}

int WebURLRequest::AppCacheHostID() const {
  return resource_request_->AppCacheHostID();
}

void WebURLRequest::SetAppCacheHostID(int app_cache_host_id) {
  resource_request_->SetAppCacheHostID(app_cache_host_id);
}

bool WebURLRequest::DownloadToFile() const {
  return resource_request_->DownloadToFile();
}

void WebURLRequest::SetDownloadToFile(bool download_to_file) {
  resource_request_->SetDownloadToFile(download_to_file);
}

bool WebURLRequest::UseStreamOnResponse() const {
  return resource_request_->UseStreamOnResponse();
}

void WebURLRequest::SetUseStreamOnResponse(bool use_stream_on_response) {
  resource_request_->SetUseStreamOnResponse(use_stream_on_response);
}

bool WebURLRequest::GetKeepalive() const {
  return resource_request_->GetKeepalive();
}

void WebURLRequest::SetKeepalive(bool keepalive) {
  resource_request_->SetKeepalive(keepalive);
}

WebURLRequest::ServiceWorkerMode WebURLRequest::GetServiceWorkerMode() const {
  return resource_request_->GetServiceWorkerMode();
}

void WebURLRequest::SetServiceWorkerMode(
    WebURLRequest::ServiceWorkerMode service_worker_mode) {
  resource_request_->SetServiceWorkerMode(service_worker_mode);
}

bool WebURLRequest::ShouldResetAppCache() const {
  return resource_request_->ShouldResetAppCache();
}

void WebURLRequest::SetShouldResetAppCache(bool set_should_reset_app_cache) {
  resource_request_->SetShouldResetAppCache(set_should_reset_app_cache);
}

WebURLRequest::FetchRequestMode WebURLRequest::GetFetchRequestMode() const {
  return resource_request_->GetFetchRequestMode();
}

void WebURLRequest::SetFetchRequestMode(WebURLRequest::FetchRequestMode mode) {
  return resource_request_->SetFetchRequestMode(mode);
}

WebURLRequest::FetchCredentialsMode WebURLRequest::GetFetchCredentialsMode()
    const {
  return resource_request_->GetFetchCredentialsMode();
}

void WebURLRequest::SetFetchCredentialsMode(
    WebURLRequest::FetchCredentialsMode mode) {
  return resource_request_->SetFetchCredentialsMode(mode);
}

WebURLRequest::FetchRedirectMode WebURLRequest::GetFetchRedirectMode() const {
  return resource_request_->GetFetchRedirectMode();
}

void WebURLRequest::SetFetchRedirectMode(
    WebURLRequest::FetchRedirectMode redirect) {
  return resource_request_->SetFetchRedirectMode(redirect);
}

WebString WebURLRequest::GetFetchIntegrity() const {
  return resource_request_->GetFetchIntegrity();
}

void WebURLRequest::SetFetchIntegrity(const WebString& integrity) {
  return resource_request_->SetFetchIntegrity(integrity);
}

WebURLRequest::PreviewsState WebURLRequest::GetPreviewsState() const {
  return resource_request_->GetPreviewsState();
}

void WebURLRequest::SetPreviewsState(
    WebURLRequest::PreviewsState previews_state) {
  return resource_request_->SetPreviewsState(previews_state);
}

WebURLRequest::ExtraData* WebURLRequest::GetExtraData() const {
  RefPtr<ResourceRequest::ExtraData> data = resource_request_->GetExtraData();
  if (!data)
    return nullptr;
  return static_cast<URLRequestExtraDataContainer*>(data.get())->GetExtraData();
}

void WebURLRequest::SetExtraData(WebURLRequest::ExtraData* extra_data) {
  if (extra_data != GetExtraData()) {
    resource_request_->SetExtraData(
        URLRequestExtraDataContainer::Create(extra_data));
  }
}

ResourceRequest& WebURLRequest::ToMutableResourceRequest() {
  DCHECK(resource_request_);
  return *resource_request_;
}

WebURLRequest::Priority WebURLRequest::GetPriority() const {
  return static_cast<WebURLRequest::Priority>(resource_request_->Priority());
}

void WebURLRequest::SetPriority(WebURLRequest::Priority priority) {
  resource_request_->SetPriority(static_cast<ResourceLoadPriority>(priority));
}

bool WebURLRequest::CheckForBrowserSideNavigation() const {
  return resource_request_->CheckForBrowserSideNavigation();
}

void WebURLRequest::SetCheckForBrowserSideNavigation(bool check) {
  resource_request_->SetCheckForBrowserSideNavigation(check);
}

double WebURLRequest::UiStartTime() const {
  return resource_request_->UiStartTime();
}

void WebURLRequest::SetUiStartTime(double time_seconds) {
  resource_request_->SetUIStartTime(time_seconds);
}

bool WebURLRequest::IsExternalRequest() const {
  return resource_request_->IsExternalRequest();
}

WebURLRequest::LoadingIPCType WebURLRequest::GetLoadingIPCType() const {
  return resource_request_->GetLoadingIPCType();
}

void WebURLRequest::SetNavigationStartTime(double navigation_start_seconds) {
  resource_request_->SetNavigationStartTime(navigation_start_seconds);
}

void WebURLRequest::SetIsSameDocumentNavigation(bool is_same_document) {
  resource_request_->SetIsSameDocumentNavigation(is_same_document);
}

WebURLRequest::InputToLoadPerfMetricReportPolicy
WebURLRequest::InputPerfMetricReportPolicy() const {
  return static_cast<WebURLRequest::InputToLoadPerfMetricReportPolicy>(
      resource_request_->InputPerfMetricReportPolicy());
}

void WebURLRequest::SetInputPerfMetricReportPolicy(
    WebURLRequest::InputToLoadPerfMetricReportPolicy policy) {
  resource_request_->SetInputPerfMetricReportPolicy(
      static_cast<blink::InputToLoadPerfMetricReportPolicy>(policy));
}

const ResourceRequest& WebURLRequest::ToResourceRequest() const {
  DCHECK(resource_request_);
  return *resource_request_;
}

WebURLRequest::WebURLRequest(ResourceRequest& r) : resource_request_(&r) {}

}  // namespace blink
