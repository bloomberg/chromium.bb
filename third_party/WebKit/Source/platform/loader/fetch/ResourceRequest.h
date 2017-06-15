/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#ifndef ResourceRequest_h
#define ResourceRequest_h

#include <memory>
#include "platform/HTTPNames.h"
#include "platform/loader/fetch/ResourceLoadPriority.h"
#include "platform/network/EncodedFormData.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/network/HTTPParsers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/Referrer.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/RefCounted.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

enum class ResourceRequestBlockedReason {
  CSP,
  kMixedContent,
  kOrigin,
  kInspector,
  kSubresourceFilter,
  kOther,
  kNone
};

enum InputToLoadPerfMetricReportPolicy : uint8_t {
  kNoReport,    // Don't report metrics for this ResourceRequest.
  kReportLink,  // Report metrics for this request as initiated by a link click.
  kReportIntent,  // Report metrics for this request as initiated by an intent.
};

struct CrossThreadResourceRequestData;

class PLATFORM_EXPORT ResourceRequest final {
  DISALLOW_NEW();

 public:
  enum class RedirectStatus : uint8_t { kFollowedRedirect, kNoRedirect };

  class ExtraData : public RefCounted<ExtraData> {
   public:
    virtual ~ExtraData() {}
  };

  ResourceRequest();
  explicit ResourceRequest(const String& url_string);
  explicit ResourceRequest(const KURL&);
  explicit ResourceRequest(CrossThreadResourceRequestData*);
  ResourceRequest(const ResourceRequest&);
  ResourceRequest& operator=(const ResourceRequest&);

  // Gets a copy of the data suitable for passing to another thread.
  std::unique_ptr<CrossThreadResourceRequestData> CopyData() const;

  bool IsNull() const;
  bool IsEmpty() const;

  const KURL& Url() const;
  void SetURL(const KURL&);

  void RemoveUserAndPassFromURL();

  WebCachePolicy GetCachePolicy() const;
  void SetCachePolicy(WebCachePolicy);

  double TimeoutInterval() const;  // May return 0 when using platform default.
  void SetTimeoutInterval(double);

  const KURL& FirstPartyForCookies() const;
  void SetFirstPartyForCookies(const KURL&);

  PassRefPtr<SecurityOrigin> RequestorOrigin() const;
  void SetRequestorOrigin(PassRefPtr<SecurityOrigin>);

  const AtomicString& HttpMethod() const;
  void SetHTTPMethod(const AtomicString&);

  const HTTPHeaderMap& HttpHeaderFields() const;
  const AtomicString& HttpHeaderField(const AtomicString& name) const;
  void SetHTTPHeaderField(const AtomicString& name, const AtomicString& value);
  void AddHTTPHeaderField(const AtomicString& name, const AtomicString& value);
  void AddHTTPHeaderFields(const HTTPHeaderMap& header_fields);
  void ClearHTTPHeaderField(const AtomicString& name);

  const AtomicString& HttpContentType() const {
    return HttpHeaderField(HTTPNames::Content_Type);
  }
  void SetHTTPContentType(const AtomicString& http_content_type) {
    SetHTTPHeaderField(HTTPNames::Content_Type, http_content_type);
  }

  bool DidSetHTTPReferrer() const { return did_set_http_referrer_; }
  const AtomicString& HttpReferrer() const {
    return HttpHeaderField(HTTPNames::Referer);
  }
  ReferrerPolicy GetReferrerPolicy() const { return referrer_policy_; }
  void SetHTTPReferrer(const Referrer&);
  void ClearHTTPReferrer();

  const AtomicString& HttpOrigin() const {
    return HttpHeaderField(HTTPNames::Origin);
  }
  const AtomicString& HttpSuborigin() const {
    return HttpHeaderField(HTTPNames::Suborigin);
  }
  // Note that these will also set and clear, respectively, the
  // Suborigin header, if appropriate.
  void SetHTTPOrigin(const SecurityOrigin*);
  void ClearHTTPOrigin();

  void AddHTTPOriginIfNeeded(const SecurityOrigin*);
  void AddHTTPOriginIfNeeded(const String&);

  const AtomicString& HttpUserAgent() const {
    return HttpHeaderField(HTTPNames::User_Agent);
  }
  void SetHTTPUserAgent(const AtomicString& http_user_agent) {
    SetHTTPHeaderField(HTTPNames::User_Agent, http_user_agent);
  }
  void ClearHTTPUserAgent();

  void SetHTTPAccept(const AtomicString& http_accept) {
    SetHTTPHeaderField(HTTPNames::Accept, http_accept);
  }

  EncodedFormData* HttpBody() const;
  void SetHTTPBody(PassRefPtr<EncodedFormData>);

  EncodedFormData* AttachedCredential() const;
  void SetAttachedCredential(PassRefPtr<EncodedFormData>);

  bool AllowStoredCredentials() const;
  void SetAllowStoredCredentials(bool allow_credentials);

  ResourceLoadPriority Priority() const;
  void SetPriority(ResourceLoadPriority, int intra_priority_value = 0);

  bool IsConditional() const;

  // Whether the associated ResourceHandleClient needs to be notified of
  // upload progress made for that resource.
  bool ReportUploadProgress() const { return report_upload_progress_; }
  void SetReportUploadProgress(bool report_upload_progress) {
    report_upload_progress_ = report_upload_progress;
  }

  // Whether actual headers being sent/received should be collected and reported
  // for the request.
  bool ReportRawHeaders() const { return report_raw_headers_; }
  void SetReportRawHeaders(bool report_raw_headers) {
    report_raw_headers_ = report_raw_headers;
  }

  // Allows the request to be matched up with its requestor.
  int RequestorID() const { return requestor_id_; }
  void SetRequestorID(int requestor_id) { requestor_id_ = requestor_id; }

  // The process id of the process from which this request originated. In
  // the case of out-of-process plugins, this allows to link back the
  // request to the plugin process (as it is processed through a render
  // view process).
  int RequestorProcessID() const { return requestor_process_id_; }
  void SetRequestorProcessID(int requestor_process_id) {
    requestor_process_id_ = requestor_process_id;
  }

  // Allows the request to be matched up with its app cache host.
  int AppCacheHostID() const { return app_cache_host_id_; }
  void SetAppCacheHostID(int id) { app_cache_host_id_ = id; }

  // True if request was user initiated.
  bool HasUserGesture() const { return has_user_gesture_; }
  void SetHasUserGesture(bool);

  // True if request should be downloaded to file.
  bool DownloadToFile() const { return download_to_file_; }
  void SetDownloadToFile(bool download_to_file) {
    download_to_file_ = download_to_file;
  }

  // True if the requestor wants to receive a response body as
  // WebDataConsumerHandle.
  bool UseStreamOnResponse() const { return use_stream_on_response_; }
  void SetUseStreamOnResponse(bool use_stream_on_response) {
    use_stream_on_response_ = use_stream_on_response;
  }

  // True if the request can work after the fetch group is terminated.
  bool GetKeepalive() const { return keepalive_; }
  void SetKeepalive(bool keepalive) { keepalive_ = keepalive; }

  // The service worker mode indicating which service workers should get events
  // for this request.
  WebURLRequest::ServiceWorkerMode GetServiceWorkerMode() const {
    return service_worker_mode_;
  }
  void SetServiceWorkerMode(
      WebURLRequest::ServiceWorkerMode service_worker_mode) {
    service_worker_mode_ = service_worker_mode;
  }

  // True if corresponding AppCache group should be resetted.
  bool ShouldResetAppCache() { return should_reset_app_cache_; }
  void SetShouldResetAppCache(bool should_reset_app_cache) {
    should_reset_app_cache_ = should_reset_app_cache;
  }

  // Extra data associated with this request.
  ExtraData* GetExtraData() const { return extra_data_.Get(); }
  void SetExtraData(PassRefPtr<ExtraData> extra_data) {
    extra_data_ = std::move(extra_data);
  }

  WebURLRequest::RequestContext GetRequestContext() const {
    return request_context_;
  }
  void SetRequestContext(WebURLRequest::RequestContext context) {
    request_context_ = context;
  }

  WebURLRequest::FrameType GetFrameType() const { return frame_type_; }
  void SetFrameType(WebURLRequest::FrameType frame_type) {
    frame_type_ = frame_type;
  }

  WebURLRequest::FetchRequestMode GetFetchRequestMode() const {
    return fetch_request_mode_;
  }
  void SetFetchRequestMode(WebURLRequest::FetchRequestMode mode) {
    fetch_request_mode_ = mode;
  }

  WebURLRequest::FetchCredentialsMode GetFetchCredentialsMode() const {
    return fetch_credentials_mode_;
  }
  void SetFetchCredentialsMode(WebURLRequest::FetchCredentialsMode mode) {
    fetch_credentials_mode_ = mode;
  }

  WebURLRequest::FetchRedirectMode GetFetchRedirectMode() const {
    return fetch_redirect_mode_;
  }
  void SetFetchRedirectMode(WebURLRequest::FetchRedirectMode redirect) {
    fetch_redirect_mode_ = redirect;
  }

  WebURLRequest::PreviewsState GetPreviewsState() const {
    return previews_state_;
  }
  void SetPreviewsState(WebURLRequest::PreviewsState previews_state) {
    previews_state_ = previews_state;
  }

  bool CacheControlContainsNoCache() const;
  bool CacheControlContainsNoStore() const;
  bool HasCacheValidatorFields() const;

  bool CheckForBrowserSideNavigation() const {
    return check_for_browser_side_navigation_;
  }
  void SetCheckForBrowserSideNavigation(bool check) {
    check_for_browser_side_navigation_ = check;
  }

  double UiStartTime() const { return ui_start_time_; }
  void SetUIStartTime(double ui_start_time_seconds) {
    ui_start_time_ = ui_start_time_seconds;
  }

  // https://mikewest.github.io/cors-rfc1918/#external-request
  bool IsExternalRequest() const { return is_external_request_; }
  void SetExternalRequestStateFromRequestorAddressSpace(WebAddressSpace);

  void OverrideLoadingIPCType(WebURLRequest::LoadingIPCType loading_ipc_type) {
    loading_ipc_type_ = loading_ipc_type;
  }
  WebURLRequest::LoadingIPCType GetLoadingIPCType() const {
    return loading_ipc_type_;
  }

  InputToLoadPerfMetricReportPolicy InputPerfMetricReportPolicy() const {
    return input_perf_metric_report_policy_;
  }
  void SetInputPerfMetricReportPolicy(
      InputToLoadPerfMetricReportPolicy input_perf_metric_report_policy) {
    input_perf_metric_report_policy_ = input_perf_metric_report_policy;
  }

  void SetRedirectStatus(RedirectStatus status) { redirect_status_ = status; }
  RedirectStatus GetRedirectStatus() const { return redirect_status_; }

  void SetNavigationStartTime(double);
  double NavigationStartTime() const { return navigation_start_; }

  void SetIsSameDocumentNavigation(bool is_same_document) {
    is_same_document_navigation_ = is_same_document;
  }
  bool IsSameDocumentNavigation() const { return is_same_document_navigation_; }

 private:
  const CacheControlHeader& GetCacheControlHeader() const;

  bool NeedsHTTPOrigin() const;

  KURL url_;
  double timeout_interval_;  // 0 is a magic value for platform default on
                             // platforms that have one.
  KURL first_party_for_cookies_;
  RefPtr<SecurityOrigin> requestor_origin_;
  AtomicString http_method_;
  HTTPHeaderMap http_header_fields_;
  RefPtr<EncodedFormData> http_body_;
  RefPtr<EncodedFormData> attached_credential_;
  bool allow_stored_credentials_ : 1;
  bool report_upload_progress_ : 1;
  bool report_raw_headers_ : 1;
  bool has_user_gesture_ : 1;
  bool download_to_file_ : 1;
  bool use_stream_on_response_ : 1;
  bool keepalive_ : 1;
  bool should_reset_app_cache_ : 1;
  WebCachePolicy cache_policy_;
  WebURLRequest::ServiceWorkerMode service_worker_mode_;
  ResourceLoadPriority priority_;
  int intra_priority_value_;
  int requestor_id_;
  int requestor_process_id_;
  int app_cache_host_id_;
  WebURLRequest::PreviewsState previews_state_;
  RefPtr<ExtraData> extra_data_;
  WebURLRequest::RequestContext request_context_;
  WebURLRequest::FrameType frame_type_;
  WebURLRequest::FetchRequestMode fetch_request_mode_;
  WebURLRequest::FetchCredentialsMode fetch_credentials_mode_;
  WebURLRequest::FetchRedirectMode fetch_redirect_mode_;
  ReferrerPolicy referrer_policy_;
  bool did_set_http_referrer_;
  bool check_for_browser_side_navigation_;
  double ui_start_time_;
  bool is_external_request_;
  WebURLRequest::LoadingIPCType loading_ipc_type_;
  bool is_same_document_navigation_;
  InputToLoadPerfMetricReportPolicy input_perf_metric_report_policy_;
  RedirectStatus redirect_status_;

  mutable CacheControlHeader cache_control_header_cache_;

  static double default_timeout_interval_;

  double navigation_start_ = 0;
};

struct CrossThreadResourceRequestData {
  WTF_MAKE_NONCOPYABLE(CrossThreadResourceRequestData);
  USING_FAST_MALLOC(CrossThreadResourceRequestData);

 public:
  CrossThreadResourceRequestData() {}
  KURL url_;

  WebCachePolicy cache_policy_;
  double timeout_interval_;
  KURL first_party_for_cookies_;
  RefPtr<SecurityOrigin> requestor_origin_;

  String http_method_;
  std::unique_ptr<CrossThreadHTTPHeaderMapData> http_headers_;
  RefPtr<EncodedFormData> http_body_;
  RefPtr<EncodedFormData> attached_credential_;
  bool allow_stored_credentials_;
  bool report_upload_progress_;
  bool has_user_gesture_;
  bool download_to_file_;
  WebURLRequest::ServiceWorkerMode service_worker_mode_;
  bool use_stream_on_response_;
  bool keepalive_;
  bool should_reset_app_cache_;
  ResourceLoadPriority priority_;
  int intra_priority_value_;
  int requestor_id_;
  int requestor_process_id_;
  int app_cache_host_id_;
  WebURLRequest::RequestContext request_context_;
  WebURLRequest::FrameType frame_type_;
  WebURLRequest::FetchRequestMode fetch_request_mode_;
  WebURLRequest::FetchCredentialsMode fetch_credentials_mode_;
  WebURLRequest::FetchRedirectMode fetch_redirect_mode_;
  WebURLRequest::PreviewsState previews_state_;
  ReferrerPolicy referrer_policy_;
  bool did_set_http_referrer_;
  bool check_for_browser_side_navigation_;
  double ui_start_time_;
  bool is_external_request_;
  WebURLRequest::LoadingIPCType loading_ipc_type_;
  InputToLoadPerfMetricReportPolicy input_perf_metric_report_policy_;
  ResourceRequest::RedirectStatus redirect_status_;
};

}  // namespace blink

#endif  // ResourceRequest_h
