/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/loader/fetch/ResourceRequest.h"

#include <memory>
#include "platform/HTTPNames.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/NetworkUtils.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

double ResourceRequest::default_timeout_interval_ = INT_MAX;

ResourceRequest::ResourceRequest() : ResourceRequest(KURL()) {}

ResourceRequest::ResourceRequest(const String& url_string)
    : ResourceRequest(KURL(kParsedURLString, url_string)) {}

ResourceRequest::ResourceRequest(const KURL& url)
    : url_(url),
      timeout_interval_(default_timeout_interval_),
      requestor_origin_(nullptr),
      http_method_(HTTPNames::GET),
      allow_stored_credentials_(true),
      report_upload_progress_(false),
      report_raw_headers_(false),
      has_user_gesture_(false),
      download_to_file_(false),
      use_stream_on_response_(false),
      keepalive_(false),
      should_reset_app_cache_(false),
      cache_policy_(WebCachePolicy::kUseProtocolCachePolicy),
      service_worker_mode_(WebURLRequest::ServiceWorkerMode::kAll),
      priority_(kResourceLoadPriorityLowest),
      intra_priority_value_(0),
      requestor_id_(0),
      requestor_process_id_(0),
      app_cache_host_id_(0),
      previews_state_(WebURLRequest::kPreviewsUnspecified),
      request_context_(WebURLRequest::kRequestContextUnspecified),
      frame_type_(WebURLRequest::kFrameTypeNone),
      fetch_request_mode_(WebURLRequest::kFetchRequestModeNoCORS),
      fetch_credentials_mode_(WebURLRequest::kFetchCredentialsModeInclude),
      fetch_redirect_mode_(WebURLRequest::kFetchRedirectModeFollow),
      referrer_policy_(kReferrerPolicyDefault),
      did_set_http_referrer_(false),
      check_for_browser_side_navigation_(true),
      ui_start_time_(0),
      is_external_request_(false),
      loading_ipc_type_(RuntimeEnabledFeatures::LoadingWithMojoEnabled()
                            ? WebURLRequest::LoadingIPCType::kMojo
                            : WebURLRequest::LoadingIPCType::kChromeIPC),
      is_same_document_navigation_(false),
      input_perf_metric_report_policy_(
          InputToLoadPerfMetricReportPolicy::kNoReport),
      redirect_status_(RedirectStatus::kNoRedirect) {}

ResourceRequest::ResourceRequest(CrossThreadResourceRequestData* data)
    : ResourceRequest(data->url_) {
  SetTimeoutInterval(data->timeout_interval_);
  SetFirstPartyForCookies(data->first_party_for_cookies_);
  SetRequestorOrigin(data->requestor_origin_);
  SetHTTPMethod(AtomicString(data->http_method_));
  SetPriority(data->priority_, data->intra_priority_value_);

  http_header_fields_.Adopt(std::move(data->http_headers_));

  SetHTTPBody(data->http_body_);
  SetAttachedCredential(data->attached_credential_);
  SetAllowStoredCredentials(data->allow_stored_credentials_);
  SetReportUploadProgress(data->report_upload_progress_);
  SetHasUserGesture(data->has_user_gesture_);
  SetDownloadToFile(data->download_to_file_);
  SetUseStreamOnResponse(data->use_stream_on_response_);
  SetKeepalive(data->keepalive_);
  SetCachePolicy(data->cache_policy_);
  SetServiceWorkerMode(data->service_worker_mode_);
  SetShouldResetAppCache(data->should_reset_app_cache_);
  SetRequestorID(data->requestor_id_);
  SetRequestorProcessID(data->requestor_process_id_);
  SetAppCacheHostID(data->app_cache_host_id_);
  SetPreviewsState(data->previews_state_);
  SetRequestContext(data->request_context_);
  SetFrameType(data->frame_type_);
  SetFetchRequestMode(data->fetch_request_mode_);
  SetFetchCredentialsMode(data->fetch_credentials_mode_);
  SetFetchRedirectMode(data->fetch_redirect_mode_);
  referrer_policy_ = data->referrer_policy_;
  did_set_http_referrer_ = data->did_set_http_referrer_;
  check_for_browser_side_navigation_ = data->check_for_browser_side_navigation_;
  ui_start_time_ = data->ui_start_time_;
  is_external_request_ = data->is_external_request_;
  loading_ipc_type_ = data->loading_ipc_type_;
  input_perf_metric_report_policy_ = data->input_perf_metric_report_policy_;
  redirect_status_ = data->redirect_status_;
}

ResourceRequest::ResourceRequest(const ResourceRequest&) = default;

ResourceRequest& ResourceRequest::operator=(const ResourceRequest&) = default;

std::unique_ptr<CrossThreadResourceRequestData> ResourceRequest::CopyData()
    const {
  std::unique_ptr<CrossThreadResourceRequestData> data =
      WTF::MakeUnique<CrossThreadResourceRequestData>();
  data->url_ = Url().Copy();
  data->timeout_interval_ = TimeoutInterval();
  data->first_party_for_cookies_ = FirstPartyForCookies().Copy();
  data->requestor_origin_ =
      RequestorOrigin() ? RequestorOrigin()->IsolatedCopy() : nullptr;
  data->http_method_ = HttpMethod().GetString().IsolatedCopy();
  data->http_headers_ = HttpHeaderFields().CopyData();
  data->priority_ = Priority();
  data->intra_priority_value_ = intra_priority_value_;

  if (http_body_)
    data->http_body_ = http_body_->DeepCopy();
  if (attached_credential_)
    data->attached_credential_ = attached_credential_->DeepCopy();
  data->allow_stored_credentials_ = allow_stored_credentials_;
  data->report_upload_progress_ = report_upload_progress_;
  data->has_user_gesture_ = has_user_gesture_;
  data->download_to_file_ = download_to_file_;
  data->use_stream_on_response_ = use_stream_on_response_;
  data->keepalive_ = keepalive_;
  data->cache_policy_ = GetCachePolicy();
  data->service_worker_mode_ = service_worker_mode_;
  data->should_reset_app_cache_ = should_reset_app_cache_;
  data->requestor_id_ = requestor_id_;
  data->requestor_process_id_ = requestor_process_id_;
  data->app_cache_host_id_ = app_cache_host_id_;
  data->previews_state_ = previews_state_;
  data->request_context_ = request_context_;
  data->frame_type_ = frame_type_;
  data->fetch_request_mode_ = fetch_request_mode_;
  data->fetch_credentials_mode_ = fetch_credentials_mode_;
  data->fetch_redirect_mode_ = fetch_redirect_mode_;
  data->referrer_policy_ = referrer_policy_;
  data->did_set_http_referrer_ = did_set_http_referrer_;
  data->check_for_browser_side_navigation_ = check_for_browser_side_navigation_;
  data->ui_start_time_ = ui_start_time_;
  data->is_external_request_ = is_external_request_;
  data->loading_ipc_type_ = loading_ipc_type_;
  data->input_perf_metric_report_policy_ = input_perf_metric_report_policy_;
  data->redirect_status_ = redirect_status_;
  return data;
}

bool ResourceRequest::IsEmpty() const {
  return url_.IsEmpty();
}

bool ResourceRequest::IsNull() const {
  return url_.IsNull();
}

const KURL& ResourceRequest::Url() const {
  return url_;
}

void ResourceRequest::SetURL(const KURL& url) {
  url_ = url;
}

void ResourceRequest::RemoveUserAndPassFromURL() {
  if (url_.User().IsEmpty() && url_.Pass().IsEmpty())
    return;

  url_.SetUser(String());
  url_.SetPass(String());
}

WebCachePolicy ResourceRequest::GetCachePolicy() const {
  return cache_policy_;
}

void ResourceRequest::SetCachePolicy(WebCachePolicy cache_policy) {
  cache_policy_ = cache_policy;
}

double ResourceRequest::TimeoutInterval() const {
  return timeout_interval_;
}

void ResourceRequest::SetTimeoutInterval(double timout_interval_seconds) {
  timeout_interval_ = timout_interval_seconds;
}

const KURL& ResourceRequest::FirstPartyForCookies() const {
  return first_party_for_cookies_;
}

void ResourceRequest::SetFirstPartyForCookies(
    const KURL& first_party_for_cookies) {
  first_party_for_cookies_ = first_party_for_cookies;
}

PassRefPtr<SecurityOrigin> ResourceRequest::RequestorOrigin() const {
  return requestor_origin_;
}

void ResourceRequest::SetRequestorOrigin(
    PassRefPtr<SecurityOrigin> requestor_origin) {
  requestor_origin_ = std::move(requestor_origin);
}

const AtomicString& ResourceRequest::HttpMethod() const {
  return http_method_;
}

void ResourceRequest::SetHTTPMethod(const AtomicString& http_method) {
  http_method_ = http_method;
}

const HTTPHeaderMap& ResourceRequest::HttpHeaderFields() const {
  return http_header_fields_;
}

const AtomicString& ResourceRequest::HttpHeaderField(
    const AtomicString& name) const {
  return http_header_fields_.Get(name);
}

void ResourceRequest::SetHTTPHeaderField(const AtomicString& name,
                                         const AtomicString& value) {
  http_header_fields_.Set(name, value);
}

void ResourceRequest::SetHTTPReferrer(const Referrer& referrer) {
  if (referrer.referrer.IsEmpty())
    http_header_fields_.Remove(HTTPNames::Referer);
  else
    SetHTTPHeaderField(HTTPNames::Referer, referrer.referrer);
  referrer_policy_ = referrer.referrer_policy;
  did_set_http_referrer_ = true;
}

void ResourceRequest::ClearHTTPReferrer() {
  http_header_fields_.Remove(HTTPNames::Referer);
  referrer_policy_ = kReferrerPolicyDefault;
  did_set_http_referrer_ = false;
}

void ResourceRequest::SetHTTPOrigin(const SecurityOrigin* origin) {
  SetHTTPHeaderField(HTTPNames::Origin, origin->ToAtomicString());
  if (origin->HasSuborigin()) {
    SetHTTPHeaderField(HTTPNames::Suborigin,
                       AtomicString(origin->GetSuborigin()->GetName()));
  }
}

void ResourceRequest::ClearHTTPOrigin() {
  http_header_fields_.Remove(HTTPNames::Origin);
  http_header_fields_.Remove(HTTPNames::Suborigin);
}

void ResourceRequest::AddHTTPOriginIfNeeded(const SecurityOrigin* origin) {
  if (NeedsHTTPOrigin())
    SetHTTPOrigin(origin);
}

void ResourceRequest::AddHTTPOriginIfNeeded(const String& origin_string) {
  if (NeedsHTTPOrigin())
    SetHTTPOrigin(SecurityOrigin::CreateFromString(origin_string).Get());
}

void ResourceRequest::ClearHTTPUserAgent() {
  http_header_fields_.Remove(HTTPNames::User_Agent);
}

EncodedFormData* ResourceRequest::HttpBody() const {
  return http_body_.Get();
}

void ResourceRequest::SetHTTPBody(PassRefPtr<EncodedFormData> http_body) {
  http_body_ = std::move(http_body);
}

EncodedFormData* ResourceRequest::AttachedCredential() const {
  return attached_credential_.Get();
}

void ResourceRequest::SetAttachedCredential(
    PassRefPtr<EncodedFormData> attached_credential) {
  attached_credential_ = std::move(attached_credential);
}

bool ResourceRequest::AllowStoredCredentials() const {
  return allow_stored_credentials_;
}

void ResourceRequest::SetAllowStoredCredentials(bool allow_credentials) {
  allow_stored_credentials_ = allow_credentials;
}

ResourceLoadPriority ResourceRequest::Priority() const {
  return priority_;
}

void ResourceRequest::SetPriority(ResourceLoadPriority priority,
                                  int intra_priority_value) {
  priority_ = priority;
  intra_priority_value_ = intra_priority_value;
}

void ResourceRequest::AddHTTPHeaderField(const AtomicString& name,
                                         const AtomicString& value) {
  HTTPHeaderMap::AddResult result = http_header_fields_.Add(name, value);
  if (!result.is_new_entry)
    result.stored_value->value = result.stored_value->value + ", " + value;
}

void ResourceRequest::AddHTTPHeaderFields(const HTTPHeaderMap& header_fields) {
  HTTPHeaderMap::const_iterator end = header_fields.end();
  for (HTTPHeaderMap::const_iterator it = header_fields.begin(); it != end;
       ++it)
    AddHTTPHeaderField(it->key, it->value);
}

void ResourceRequest::ClearHTTPHeaderField(const AtomicString& name) {
  http_header_fields_.Remove(name);
}

void ResourceRequest::SetExternalRequestStateFromRequestorAddressSpace(
    WebAddressSpace requestor_space) {
  static_assert(kWebAddressSpaceLocal < kWebAddressSpacePrivate,
                "Local is inside Private");
  static_assert(kWebAddressSpaceLocal < kWebAddressSpacePublic,
                "Local is inside Public");
  static_assert(kWebAddressSpacePrivate < kWebAddressSpacePublic,
                "Private is inside Public");

  // TODO(mkwst): This only checks explicit IP addresses. We'll have to move all
  // this up to //net and //content in order to have any real impact on gateway
  // attacks. That turns out to be a TON of work. https://crbug.com/378566
  if (!RuntimeEnabledFeatures::CorsRFC1918Enabled()) {
    is_external_request_ = false;
    return;
  }

  WebAddressSpace target_space = kWebAddressSpacePublic;
  if (NetworkUtils::IsReservedIPAddress(url_.Host()))
    target_space = kWebAddressSpacePrivate;
  if (SecurityOrigin::Create(url_)->IsLocalhost())
    target_space = kWebAddressSpaceLocal;

  is_external_request_ = requestor_space > target_space;
}

void ResourceRequest::SetNavigationStartTime(double navigation_start) {
  navigation_start_ = navigation_start;
}

bool ResourceRequest::IsConditional() const {
  return (http_header_fields_.Contains(HTTPNames::If_Match) ||
          http_header_fields_.Contains(HTTPNames::If_Modified_Since) ||
          http_header_fields_.Contains(HTTPNames::If_None_Match) ||
          http_header_fields_.Contains(HTTPNames::If_Range) ||
          http_header_fields_.Contains(HTTPNames::If_Unmodified_Since));
}

void ResourceRequest::SetHasUserGesture(bool has_user_gesture) {
  has_user_gesture_ |= has_user_gesture;
}

const CacheControlHeader& ResourceRequest::GetCacheControlHeader() const {
  if (!cache_control_header_cache_.parsed) {
    cache_control_header_cache_ = ParseCacheControlDirectives(
        http_header_fields_.Get(HTTPNames::Cache_Control),
        http_header_fields_.Get(HTTPNames::Pragma));
  }
  return cache_control_header_cache_;
}

bool ResourceRequest::CacheControlContainsNoCache() const {
  return GetCacheControlHeader().contains_no_cache;
}

bool ResourceRequest::CacheControlContainsNoStore() const {
  return GetCacheControlHeader().contains_no_store;
}

bool ResourceRequest::HasCacheValidatorFields() const {
  return !http_header_fields_.Get(HTTPNames::Last_Modified).IsEmpty() ||
         !http_header_fields_.Get(HTTPNames::ETag).IsEmpty();
}

bool ResourceRequest::NeedsHTTPOrigin() const {
  if (!HttpOrigin().IsEmpty())
    return false;  // Request already has an Origin header.

  // Don't send an Origin header for GET or HEAD to avoid privacy issues.
  // For example, if an intranet page has a hyperlink to an external web
  // site, we don't want to include the Origin of the request because it
  // will leak the internal host name. Similar privacy concerns have lead
  // to the widespread suppression of the Referer header at the network
  // layer.
  if (HttpMethod() == HTTPNames::GET || HttpMethod() == HTTPNames::HEAD)
    return false;

  // For non-GET and non-HEAD methods, always send an Origin header so the
  // server knows we support this feature.
  return true;
}

}  // namespace blink
