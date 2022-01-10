// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_fcm_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/common/strings.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace safe_browsing {
namespace {

// The command line flag to control the max amount of concurrent active
// requests.
// TODO(crbug.com/1191061): Tweak this number to an "optimal" value.
constexpr char kMaxParallelActiveRequests[] = "wp-max-parallel-active-requests";
constexpr int kDefaultMaxParallelActiveRequests = 5;

constexpr base::TimeDelta kAuthTimeout = base::Seconds(10);
constexpr base::TimeDelta kScanningTimeout = base::Minutes(5);

const char kSbEnterpriseUploadUrl[] =
    "https://safebrowsing.google.com/safebrowsing/uploads/scan";

const char kSbConsumerUploadUrl[] =
    "https://safebrowsing.google.com/safebrowsing/uploads/consumer";

bool IsConsumerScanRequest(const BinaryUploadService::Request& request) {
  for (const std::string& tag : request.content_analysis_request().tags()) {
    if (tag == "dlp")
      return false;
  }
  return request.device_token().empty();
}

std::string ResultToString(BinaryUploadService::Result result) {
  switch (result) {
    case BinaryUploadService::Result::UNKNOWN:
      return "UNKNOWN";
    case BinaryUploadService::Result::SUCCESS:
      return "SUCCESS";
    case BinaryUploadService::Result::UPLOAD_FAILURE:
      return "UPLOAD_FAILURE";
    case BinaryUploadService::Result::TIMEOUT:
      return "TIMEOUT";
    case BinaryUploadService::Result::FILE_TOO_LARGE:
      return "FILE_TOO_LARGE";
    case BinaryUploadService::Result::FAILED_TO_GET_TOKEN:
      return "FAILED_TO_GET_TOKEN";
    case BinaryUploadService::Result::UNAUTHORIZED:
      return "UNAUTHORIZED";
    case BinaryUploadService::Result::FILE_ENCRYPTED:
      return "FILE_ENCRYPTED";
    case BinaryUploadService::Result::DLP_SCAN_UNSUPPORTED_FILE_TYPE:
      return "DLP_SCAN_UNSUPPORTED_FILE_TYPE";
    case BinaryUploadService::Result::TOO_MANY_REQUESTS:
      return "TOO_MANY_REQUESTS";
  }
}

constexpr char kBinaryUploadServiceUrlFlag[] = "binary-upload-service-url";

absl::optional<GURL> GetUrlOverride() {
  // Ignore this flag on Stable and Beta to avoid abuse.
  if (!g_browser_process || !g_browser_process->browser_policy_connector()
                                 ->IsCommandLineSwitchSupported()) {
    return absl::nullopt;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kBinaryUploadServiceUrlFlag)) {
    GURL url =
        GURL(command_line->GetSwitchValueASCII(kBinaryUploadServiceUrlFlag));
    if (url.is_valid())
      return url;
    else
      LOG(ERROR) << "--binary-upload-service-url is set to an invalid URL";
  }

  return absl::nullopt;
}

net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag(bool is_app) {
  if (is_app) {
    return net::DefineNetworkTrafficAnnotation(
        "safe_browsing_binary_upload_app", R"(
        semantics {
          sender: "Advanced Protection Program"
          description:
            "For users part of Google's Advanced Protection Program, when a "
            "file is downloaded, Chrome will upload that file to Safe Browsing "
            "for detailed scanning."
          trigger:
            "The browser will upload the file to Google when the user "
            "downloads a file, and the browser is enrolled into the "
            "Advanced Protection Program."
          data:
            "The downloaded file."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing Cookie Store"
          setting: "This is disabled by default an can only be enabled by "
            "policy."
          chrome_policy {
            AdvancedProtectionAllowed {
              AdvancedProtectionAllowed: false
            }
          }
        }
        )");
  } else {
    return net::DefineNetworkTrafficAnnotation(
        "safe_browsing_binary_upload_connector", R"(
        semantics {
          sender: "Chrome Enterprise Connectors"
          description:
            "For users with content analysis Chrome Enterprise Connectors "
            "enabled, Chrome will upload the data corresponding to the "
            "Connector for scanning."
          trigger:
            "If the OnFileAttachedEnterpriseConnector, "
            "OnFileDownloadedEnterpriseConnector or "
            "OnBulkDataEntryEnterpriseConnector policy is set, a request is made to "
            "scan a file attached to Chrome, a file downloaded by Chrome or "
            "data pasted in Chrome respectively."
          data:
            "The uploaded or downloaded file, or pasted data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing Cookie Store"
          setting: "This is disabled by default an can only be enabled by "
            "policy."
          chrome_policy {
            OnFileAttachedEnterpriseConnector {
              OnFileAttachedEnterpriseConnector: "[]"
            }
            OnFileDownloadedEnterpriseConnector {
              OnFileDownloadedEnterpriseConnector: "[]"
            }
            OnBulkDataEntryEnterpriseConnector {
              OnBulkDataEntryEnterpriseConnector: "[]"
            }
          }
        }
        )");
  }
}

}  // namespace

// static
size_t BinaryUploadService::GetParallelActiveRequestsMax() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kMaxParallelActiveRequests)) {
    int parsed_max;
    if (base::StringToInt(
            command_line->GetSwitchValueASCII(kMaxParallelActiveRequests),
            &parsed_max) &&
        parsed_max > 0) {
      return parsed_max;
    } else {
      LOG(ERROR) << "wp-max-parallel-active-requests had invalid value";
    }
  }

  return kDefaultMaxParallelActiveRequests;
}

BinaryUploadService::BinaryUploadService(Profile* profile)
    : url_loader_factory_(profile->GetURLLoaderFactory()),
      binary_fcm_service_(BinaryFCMService::Create(profile)),
      profile_(profile),
      weakptr_factory_(this) {}

BinaryUploadService::BinaryUploadService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    std::unique_ptr<BinaryFCMService> binary_fcm_service)
    : url_loader_factory_(url_loader_factory),
      binary_fcm_service_(std::move(binary_fcm_service)),
      profile_(profile),
      weakptr_factory_(this) {}

BinaryUploadService::~BinaryUploadService() {}

void BinaryUploadService::MaybeUploadForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsConsumerScanRequest(*request)) {
    DCHECK(!request->IsAuthRequest());
    const bool is_advanced_protection =
        safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
            profile_)
            ->IsUnderAdvancedProtection();
    const bool is_enhanced_protection =
        profile_ && IsEnhancedProtectionEnabled(*profile_->GetPrefs());

    const bool is_deep_scan_authorized =
        is_advanced_protection || is_enhanced_protection;
    MaybeUploadForDeepScanningCallback(std::move(request),
                                       /*authorized=*/is_deep_scan_authorized);
    return;
  }

  // Make copies of the connector and DM token since |request| is about to move.
  auto connector = request->analysis_connector();
  std::string dm_token = request->device_token();
  TokenAndConnector token_and_connector = {dm_token, connector};

  if (dm_token.empty()) {
    MaybeUploadForDeepScanningCallback(std::move(request),
                                       /*authorized*/ false);
    return;
  }

  if (!can_upload_enterprise_data_.contains(token_and_connector)) {
    // Get the URL first since |request| is about to move.
    GURL url = request->GetUrlWithParams();
    IsAuthorized(
        std::move(url),
        base::BindOnce(&BinaryUploadService::MaybeUploadForDeepScanningCallback,
                       weakptr_factory_.GetWeakPtr(), std::move(request)),
        dm_token, connector);
    return;
  }

  MaybeUploadForDeepScanningCallback(
      std::move(request), can_upload_enterprise_data_[token_and_connector]);
}

void BinaryUploadService::MaybeUploadForDeepScanningCallback(
    std::unique_ptr<BinaryUploadService::Request> request,
    bool authorized) {
  // Ignore the request if the browser cannot upload data.
  if (!authorized) {
    // TODO(crbug/1028133): Add extra logic to handle UX for non-authorized
    // users.
    request->FinishRequest(Result::UNAUTHORIZED,
                           enterprise_connectors::ContentAnalysisResponse());
    return;
  }
  QueueForDeepScanning(std::move(request));
}

void BinaryUploadService::QueueForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  if (active_requests_.size() >= GetParallelActiveRequestsMax())
    request_queue_.push(std::move(request));
  else
    UploadForDeepScanning(std::move(request));
}

void BinaryUploadService::UploadForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool is_auth_request = request->IsAuthRequest();
  Request* raw_request = request.get();
  active_requests_[raw_request] = std::move(request);
  start_times_[raw_request] = base::TimeTicks::Now();

  std::string token = base::RandBytesAsString(128);
  token = base::HexEncode(token.data(), token.size());
  active_tokens_[raw_request] = token;
  raw_request->set_request_token(token);

  if ((!binary_fcm_service_ || !binary_fcm_service_->Connected()) &&
      !is_auth_request) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&BinaryUploadService::FinishRequest,
                       weakptr_factory_.GetWeakPtr(), raw_request,
                       Result::FAILED_TO_GET_TOKEN,
                       enterprise_connectors::ContentAnalysisResponse()));
    return;
  }

  // Auth requests are never going to need waiting for an async response, so
  // don't bother getting a token from `binary_fcm_service_`.
  if (is_auth_request) {
    raw_request->GetRequestData(
        base::BindOnce(&BinaryUploadService::OnGetRequestData,
                       weakptr_factory_.GetWeakPtr(), raw_request));
  } else {
    binary_fcm_service_->SetCallbackForToken(
        token, base::BindRepeating(&BinaryUploadService::OnGetResponse,
                                   weakptr_factory_.GetWeakPtr(), raw_request));
    binary_fcm_service_->GetInstanceID(
        base::BindOnce(&BinaryUploadService::OnGetInstanceID,
                       weakptr_factory_.GetWeakPtr(), raw_request));
  }

  active_timers_[raw_request] = std::make_unique<base::OneShotTimer>();
  active_timers_[raw_request]->Start(
      FROM_HERE, is_auth_request ? kAuthTimeout : kScanningTimeout,
      base::BindOnce(&BinaryUploadService::OnTimeout,
                     weakptr_factory_.GetWeakPtr(), raw_request));
}

void BinaryUploadService::OnGetInstanceID(Request* request,
                                          const std::string& instance_id) {
  if (!IsActive(request))
    return;

  if (instance_id == BinaryFCMService::kInvalidId) {
    FinishRequest(request, Result::FAILED_TO_GET_TOKEN,
                  enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  base::UmaHistogramCustomTimes(
      "SafeBrowsingBinaryUploadRequest.TimeToGetFCMToken",
      base::TimeTicks::Now() - start_times_[request], base::Milliseconds(1),
      base::Minutes(6), 50);

  request->set_fcm_token(instance_id);
  request->GetRequestData(base::BindOnce(&BinaryUploadService::OnGetRequestData,
                                         weakptr_factory_.GetWeakPtr(),
                                         request));
}

void BinaryUploadService::OnGetRequestData(Request* request,
                                           Result result,
                                           Request::Data data) {
  if (!IsActive(request))
    return;

  if (result != Result::SUCCESS) {
    FinishRequest(request, result,
                  enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  if (!request->IsAuthRequest() && data.size == 0) {
    // A size of 0 implies an edge case like an empty file being uploaded. In
    // such a case, the file doesn't need to scan so the request can simply
    // finish early.
    FinishRequest(request, Result::SUCCESS,
                  enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  std::string metadata;
  request->SerializeToString(&metadata);
  base::Base64Encode(metadata, &metadata);

  GURL url = request->GetUrlWithParams();
  if (!url.is_valid())
    url = GetUploadUrl(IsConsumerScanRequest(*request));
  net::NetworkTrafficAnnotationTag traffic_annotation =
      GetTrafficAnnotationTag(IsConsumerScanRequest(*request));
  auto callback = base::BindOnce(&BinaryUploadService::OnUploadComplete,
                                 weakptr_factory_.GetWeakPtr(), request);
  std::unique_ptr<MultipartUploadRequest> upload_request;
  if (request->IsAuthRequest() || !data.contents.empty()) {
    upload_request = MultipartUploadRequest::CreateStringRequest(
        url_loader_factory_, std::move(url), metadata, data.contents,
        std::move(traffic_annotation), std::move(callback));
  } else {
    DCHECK(!data.path.empty());
    upload_request = MultipartUploadRequest::CreateFileRequest(
        url_loader_factory_, std::move(url), metadata, data.path,
        std::move(traffic_annotation), std::move(callback));
  }

  WebUIInfoSingleton::GetInstance()->AddToDeepScanRequests(
      request->tab_url(), request->per_profile_request(),
      request->content_analysis_request());

  // |request| might have been deleted by the call to Start() in tests, so don't
  // dereference it afterwards.
  upload_request->Start();
  active_uploads_[request] = std::move(upload_request);
}

void BinaryUploadService::OnUploadComplete(Request* request,
                                           bool success,
                                           int http_status,
                                           const std::string& response_data) {
  if (!IsActive(request))
    return;

  if (http_status == net::HTTP_TOO_MANY_REQUESTS) {
    FinishRequest(request, Result::TOO_MANY_REQUESTS,
                  enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  if (!success) {
    FinishRequest(request, Result::UPLOAD_FAILURE,
                  enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  enterprise_connectors::ContentAnalysisResponse response;
  if (!response.ParseFromString(response_data)) {
    FinishRequest(request, Result::UPLOAD_FAILURE,
                  enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  active_uploads_.erase(request);

  // Synchronous scans can return results in the initial response proto, so
  // check for those.
  OnGetResponse(request, response);
}

void BinaryUploadService::OnGetResponse(
    Request* request,
    enterprise_connectors::ContentAnalysisResponse response) {
  if (!IsActive(request))
    return;

  for (const auto& result : response.results()) {
    if (result.has_tag() && !result.tag().empty()) {
      VLOG(1) << "Request " << request->request_token()
              << " finished scanning tag <" << result.tag() << ">";
      received_connector_results_[request][result.tag()] = result;
    }
  }

  MaybeFinishRequest(request);
}

void BinaryUploadService::MaybeFinishRequest(Request* request) {
  for (const std::string& tag : request->content_analysis_request().tags()) {
    const auto& results = received_connector_results_[request];
    if (std::none_of(results.begin(), results.end(),
                     [&tag](const auto& tag_and_result) {
                       return tag_and_result.first == tag;
                     })) {
      VLOG(1) << "Request " << request->request_token() << " is waiting for <"
              << tag << "> scanning to complete.";
      return;
    }
  }

  // It's OK to move here since the map entry is about to be removed.
  enterprise_connectors::ContentAnalysisResponse response;
  response.set_request_token(request->request_token());
  for (auto& tag_and_result : received_connector_results_[request])
    *response.add_results() = std::move(tag_and_result.second);
  FinishRequest(request, Result::SUCCESS, std::move(response));
}

void BinaryUploadService::OnTimeout(Request* request) {
  if (IsActive(request))
    FinishRequest(request, Result::TIMEOUT,
                  enterprise_connectors::ContentAnalysisResponse());
}

void BinaryUploadService::FinishRequest(
    Request* request,
    Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  RecordRequestMetrics(request, result, response);

  // We add the request here in case we never actually uploaded anything, so it
  // wasn't added in OnGetRequestData
  WebUIInfoSingleton::GetInstance()->AddToDeepScanRequests(
      request->tab_url(), request->per_profile_request(),
      request->content_analysis_request());
  WebUIInfoSingleton::GetInstance()->AddToDeepScanResponses(
      active_tokens_[request], ResultToString(result), response);

  std::string instance_id = request->fcm_notification_token();
  request->FinishRequest(result, response);
  FinishRequestCleanup(request, instance_id);

  // Now that a request has been cleaned up, we can try to allocate resources
  // for queued uploads.
  PopRequestQueue();
}

void BinaryUploadService::FinishRequestCleanup(Request* request,
                                               const std::string& instance_id) {
  std::string dm_token = request->device_token();
  auto connector = request->analysis_connector();
  active_requests_.erase(request);
  active_timers_.erase(request);
  active_uploads_.erase(request);
  received_connector_results_.erase(request);

  auto token_it = active_tokens_.find(request);
  if (binary_fcm_service_ && token_it != active_tokens_.end())
    binary_fcm_service_->ClearCallbackForToken(token_it->second);

  if (binary_fcm_service_ && instance_id != BinaryFCMService::kInvalidId) {
    // The BinaryFCMService will handle all recoverable errors. In case of
    // unrecoverable error, there's nothing we can do here.
    binary_fcm_service_->UnregisterInstanceID(
        instance_id,
        base::BindOnce(&BinaryUploadService::InstanceIDUnregisteredCallback,
                       weakptr_factory_.GetWeakPtr(), dm_token, connector));
  } else {
    // `binary_fcm_service_` can be null in tests and `instance_id` can be
    // invalid if getting an FCM token failed, timed out or for an auth request.
    // InstanceIDUnregisteredCallback should be called anyway so the requests
    // waiting on authentication can complete.
    InstanceIDUnregisteredCallback(dm_token, connector, true);
  }

  // Re-obtain `token_it` as auth requests calls to
  // InstanceIDUnregisteredCallback can result in new requests that invalidate
  // the iterator.
  token_it = active_tokens_.find(request);
  if (token_it != active_tokens_.end()) {
    active_tokens_.erase(token_it);
  }
}

void BinaryUploadService::InstanceIDUnregisteredCallback(
    const std::string& dm_token,
    enterprise_connectors::AnalysisConnector connector,
    bool) {
  TokenAndConnector token_and_connector = {dm_token, connector};
  // Calling RunAuthorizationCallbacks after the instance ID of the initial
  // authentication is unregistered avoids registration/unregistration conflicts
  // with normal requests.
  if (authorization_callbacks_.contains(token_and_connector) &&
      !authorization_callbacks_[token_and_connector].empty() &&
      can_upload_enterprise_data_.contains(token_and_connector)) {
    RunAuthorizationCallbacks(dm_token, connector);
  }
}

void BinaryUploadService::RecordRequestMetrics(Request* request,
                                               Result result) {
  base::UmaHistogramEnumeration("SafeBrowsingBinaryUploadRequest.Result",
                                result);
  base::UmaHistogramCustomTimes("SafeBrowsingBinaryUploadRequest.Duration",
                                base::TimeTicks::Now() - start_times_[request],
                                base::Milliseconds(1), base::Minutes(6), 50);
}

void BinaryUploadService::RecordRequestMetrics(
    Request* request,
    Result result,
    const enterprise_connectors::ContentAnalysisResponse& response) {
  RecordRequestMetrics(request, result);
  for (const auto& response_result : response.results()) {
    if (response_result.tag() == "malware") {
      base::UmaHistogramBoolean(
          "SafeBrowsingBinaryUploadRequest.MalwareResult",
          response_result.status() !=
              enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);
    }
    if (response_result.tag() == "dlp") {
      base::UmaHistogramBoolean(
          "SafeBrowsingBinaryUploadRequest.DlpResult",
          response_result.status() !=
              enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);
    }
  }
}

BinaryUploadService::Request::Data::Data() = default;
BinaryUploadService::Request::Data::Data(Data&&) = default;
BinaryUploadService::Request::Data&
BinaryUploadService::Request::Data::operator=(
    BinaryUploadService::Request::Data&& other) = default;
BinaryUploadService::Request::Data::~Data() = default;

BinaryUploadService::Request::Request(ContentAnalysisCallback callback,
                                      GURL url)
    : content_analysis_callback_(std::move(callback)), url_(url) {}

BinaryUploadService::Request::~Request() = default;

void BinaryUploadService::Request::set_tab_url(const GURL& tab_url) {
  tab_url_ = tab_url;
}

const GURL& BinaryUploadService::Request::tab_url() const {
  return tab_url_;
}

void BinaryUploadService::Request::set_per_profile_request(
    bool per_profile_request) {
  per_profile_request_ = per_profile_request;
}

bool BinaryUploadService::Request::per_profile_request() const {
  return per_profile_request_;
}

void BinaryUploadService::Request::set_fcm_token(const std::string& token) {
  content_analysis_request_.set_fcm_notification_token(token);
}

void BinaryUploadService::Request::set_device_token(const std::string& token) {
  content_analysis_request_.set_device_token(token);
}

void BinaryUploadService::Request::set_request_token(const std::string& token) {
  content_analysis_request_.set_request_token(token);
}

void BinaryUploadService::Request::set_filename(const std::string& filename) {
  content_analysis_request_.mutable_request_data()->set_filename(filename);
}

void BinaryUploadService::Request::set_digest(const std::string& digest) {
  content_analysis_request_.mutable_request_data()->set_digest(digest);
}

void BinaryUploadService::Request::clear_dlp_scan_request() {
  auto* tags = content_analysis_request_.mutable_tags();
  auto it = std::find(tags->begin(), tags->end(), "dlp");
  if (it != tags->end())
    tags->erase(it);
}

void BinaryUploadService::Request::set_analysis_connector(
    enterprise_connectors::AnalysisConnector connector) {
  content_analysis_request_.set_analysis_connector(connector);
}

void BinaryUploadService::Request::set_url(const std::string& url) {
  content_analysis_request_.mutable_request_data()->set_url(url);
}

void BinaryUploadService::Request::set_csd(ClientDownloadRequest csd) {
  *content_analysis_request_.mutable_request_data()->mutable_csd() =
      std::move(csd);
}

void BinaryUploadService::Request::add_tag(const std::string& tag) {
  content_analysis_request_.add_tags(tag);
}

void BinaryUploadService::Request::set_email(const std::string& email) {
  content_analysis_request_.mutable_request_data()->set_email(email);
}

void BinaryUploadService::Request::set_client_metadata(
    enterprise_connectors::ClientMetadata metadata) {
  *content_analysis_request_.mutable_client_metadata() = std::move(metadata);
}

void BinaryUploadService::Request::set_content_type(const std::string& type) {
  content_analysis_request_.mutable_request_data()->set_content_type(type);
}

enterprise_connectors::AnalysisConnector
BinaryUploadService::Request::analysis_connector() {
  return content_analysis_request_.analysis_connector();
}

const std::string& BinaryUploadService::Request::device_token() const {
  return content_analysis_request_.device_token();
}

const std::string& BinaryUploadService::Request::request_token() const {
  return content_analysis_request_.request_token();
}

const std::string& BinaryUploadService::Request::fcm_notification_token()
    const {
  return content_analysis_request_.fcm_notification_token();
}

const std::string& BinaryUploadService::Request::filename() const {
  return content_analysis_request_.request_data().filename();
}

const std::string& BinaryUploadService::Request::digest() const {
  return content_analysis_request_.request_data().digest();
}

const std::string& BinaryUploadService::Request::content_type() const {
  return content_analysis_request_.request_data().content_type();
}

void BinaryUploadService::Request::FinishRequest(
    Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  std::move(content_analysis_callback_).Run(result, response);
}

void BinaryUploadService::Request::SerializeToString(
    std::string* destination) const {
  content_analysis_request_.SerializeToString(destination);
}

GURL BinaryUploadService::Request::GetUrlWithParams() const {
  GURL url = GetUrlOverride().value_or(url_);

  url = net::AppendQueryParameter(url, enterprise::kUrlParamDeviceToken,
                                  device_token());

  std::string connector;
  switch (content_analysis_request_.analysis_connector()) {
    case enterprise_connectors::FILE_ATTACHED:
      connector = "OnFileAttached";
      break;
    case enterprise_connectors::FILE_DOWNLOADED:
      connector = "OnFileDownloaded";
      break;
    case enterprise_connectors::BULK_DATA_ENTRY:
      connector = "OnBulkDataEntry";
      break;
    case enterprise_connectors::PRINT:
      connector = "OnPrint";
      break;
    case enterprise_connectors::ANALYSIS_CONNECTOR_UNSPECIFIED:
      break;
  }
  if (!connector.empty()) {
    url = net::AppendQueryParameter(url, enterprise::kUrlParamConnector,
                                    connector);
  }

  for (const std::string& tag : content_analysis_request_.tags())
    url = net::AppendQueryParameter(url, enterprise::kUrlParamTag, tag);

  return url;
}

bool BinaryUploadService::Request::IsAuthRequest() const {
  return false;
}

bool BinaryUploadService::IsActive(Request* request) {
  return (active_requests_.find(request) != active_requests_.end());
}

class ValidateDataUploadRequest : public BinaryUploadService::Request {
 public:
  ValidateDataUploadRequest(
      BinaryUploadService::ContentAnalysisCallback callback,
      GURL url)
      : BinaryUploadService::Request(std::move(callback), url) {}
  ValidateDataUploadRequest(const ValidateDataUploadRequest&) = delete;
  ValidateDataUploadRequest& operator=(const ValidateDataUploadRequest&) =
      delete;
  ~ValidateDataUploadRequest() override = default;

 private:
  // BinaryUploadService::Request implementation.
  void GetRequestData(DataCallback callback) override;

  bool IsAuthRequest() const override;
};

inline void ValidateDataUploadRequest::GetRequestData(DataCallback callback) {
  std::move(callback).Run(BinaryUploadService::Result::SUCCESS,
                          BinaryUploadService::Request::Data());
}

bool ValidateDataUploadRequest::IsAuthRequest() const {
  return true;
}

void BinaryUploadService::IsAuthorized(
    const GURL& url,
    AuthorizationCallback callback,
    const std::string& dm_token,
    enterprise_connectors::AnalysisConnector connector) {
  // Start |timer_| on the first call to IsAuthorized. This is necessary in
  // order to invalidate the authorization every 24 hours.
  if (!timer_.IsRunning()) {
    timer_.Start(
        FROM_HERE, base::Hours(24),
        base::BindRepeating(&BinaryUploadService::ResetAuthorizationData,
                            weakptr_factory_.GetWeakPtr(), url));
  }

  TokenAndConnector token_and_connector = {dm_token, connector};
  if (!can_upload_enterprise_data_.contains(token_and_connector)) {
    // Send a request to check if the browser can upload data.
    authorization_callbacks_[token_and_connector].push_back(
        std::move(callback));
    if (!pending_validate_data_upload_request_.contains(token_and_connector)) {
      pending_validate_data_upload_request_.insert(token_and_connector);
      auto request = std::make_unique<ValidateDataUploadRequest>(
          base::BindOnce(
              &BinaryUploadService::ValidateDataUploadRequestConnectorCallback,
              weakptr_factory_.GetWeakPtr(), dm_token, connector),
          url);
      request->set_device_token(dm_token);
      request->set_analysis_connector(connector);
      QueueForDeepScanning(std::move(request));
    }
    return;
  }
  std::move(callback).Run(can_upload_enterprise_data_[token_and_connector]);
}

void BinaryUploadService::ValidateDataUploadRequestConnectorCallback(
    const std::string& dm_token,
    enterprise_connectors::AnalysisConnector connector,
    BinaryUploadService::Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  TokenAndConnector token_and_connector = {dm_token, connector};
  pending_validate_data_upload_request_.erase(token_and_connector);
  can_upload_enterprise_data_[token_and_connector] =
      (result == BinaryUploadService::Result::SUCCESS);
}

void BinaryUploadService::RunAuthorizationCallbacks(
    const std::string& dm_token,
    enterprise_connectors::AnalysisConnector connector) {
  TokenAndConnector token_and_connector = {dm_token, connector};
  DCHECK(can_upload_enterprise_data_.contains(token_and_connector));

  auto it = authorization_callbacks_[token_and_connector].begin();
  while (it != authorization_callbacks_[token_and_connector].end()) {
    std::move(*it).Run(can_upload_enterprise_data_[token_and_connector]);
    it = authorization_callbacks_[token_and_connector].erase(it);
  }
}

void BinaryUploadService::ResetAuthorizationData(const GURL& url) {
  // Clearing |can_upload_enterprise_data_| will make the next
  // call to IsAuthorized send out a request to validate data uploads.
  auto it = can_upload_enterprise_data_.begin();
  while (it != can_upload_enterprise_data_.end()) {
    std::string dm_token = it->first.first;
    enterprise_connectors::AnalysisConnector connector = it->first.second;
    it = can_upload_enterprise_data_.erase(it);
    IsAuthorized(url, base::DoNothing(), dm_token, connector);
  }
}

void BinaryUploadService::Shutdown() {
  if (!active_requests_.empty()) {
    base::UmaHistogramCounts10000(
        "SafeBrowsingBinaryUploadService.ActiveRequestsAtShutdown",
        active_requests_.size());
  }
  if (binary_fcm_service_)
    binary_fcm_service_->Shutdown();
}

void BinaryUploadService::SetAuthForTesting(const std::string& dm_token,
                                            bool authorized) {
  for (enterprise_connectors::AnalysisConnector connector :
       {enterprise_connectors::AnalysisConnector::
            ANALYSIS_CONNECTOR_UNSPECIFIED,
        enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED,
        enterprise_connectors::AnalysisConnector::FILE_ATTACHED,
        enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY,
        enterprise_connectors::AnalysisConnector::PRINT}) {
    TokenAndConnector token_and_connector = {dm_token, connector};
    can_upload_enterprise_data_[token_and_connector] = authorized;
  }
}

// static
GURL BinaryUploadService::GetUploadUrl(bool is_consumer_scan_eligible) {
  if (is_consumer_scan_eligible) {
    return GURL(kSbConsumerUploadUrl);
  } else {
    return GURL(kSbEnterpriseUploadUrl);
  }
}

void BinaryUploadService::PopRequestQueue() {
  while (active_requests_.size() < GetParallelActiveRequestsMax() &&
         !request_queue_.empty()) {
    std::unique_ptr<Request> request = std::move(request_queue_.front());
    request_queue_.pop();
    UploadForDeepScanning(std::move(request));
  }
}

}  // namespace safe_browsing
