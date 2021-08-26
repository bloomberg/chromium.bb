// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_loader.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/mime_sniffer.h"
#include "net/base/schemeful_site.h"
#include "net/base/transport_info.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_file_element_reader.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/site_for_cookies.h"
#include "net/cookies/static_cookie_policy.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_request_headers.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_private_key.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/chunked_data_pipe_upload_data_stream.h"
#include "services/network/data_pipe_element_reader.h"
#include "services/network/origin_policy/origin_policy_constants.h"
#include "services/network/origin_policy/origin_policy_manager.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/cross_origin_resource_policy.h"
#include "services/network/public/cpp/empty_url_loader_client.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/opaque_response_blocking.h"
#include "services/network/public/cpp/origin_policy.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/cookie_access_observer.mojom-forward.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/http_raw_headers.mojom.h"
#include "services/network/public/mojom/origin_policy_manager.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/sec_header_helpers.h"
#include "services/network/throttling/scoped_throttling_token.h"
#include "services/network/trust_tokens/trust_token_request_helper.h"
#include "services/network/url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "services/network/radio_monitor_android.h"
#endif

namespace network {

namespace {

using ConcerningHeaderId = URLLoader::ConcerningHeaderId;

// Cannot use 0, because this means "default" in
// mojo::core::Core::CreateDataPipe
constexpr size_t kBlockedBodyAllocationSize = 1;

void PopulateResourceResponse(net::URLRequest* request,
                              bool is_load_timing_enabled,
                              int32_t options,
                              network::mojom::URLResponseHead* response) {
  response->request_time = request->request_time();
  response->response_time = request->response_time();
  response->headers = request->response_headers();
  response->parsed_headers =
      PopulateParsedHeaders(response->headers.get(), request->url());

  request->GetCharset(&response->charset);
  response->content_length = request->GetExpectedContentSize();
  request->GetMimeType(&response->mime_type);
  net::HttpResponseInfo response_info = request->response_info();
  response->was_fetched_via_spdy = response_info.was_fetched_via_spdy;
  response->was_alpn_negotiated = response_info.was_alpn_negotiated;
  response->alpn_negotiated_protocol = response_info.alpn_negotiated_protocol;
  response->connection_info = response_info.connection_info;
  response->remote_endpoint = response_info.remote_endpoint;
  response->was_fetched_via_cache = request->was_cached();
  response->is_validated = (response_info.cache_entry_status ==
                            net::HttpResponseInfo::ENTRY_VALIDATED);
  response->proxy_server = request->proxy_server();
  response->network_accessed = response_info.network_accessed;
  response->async_revalidation_requested =
      response_info.async_revalidation_requested;
  response->was_in_prefetch_cache =
      !(request->load_flags() & net::LOAD_PREFETCH) &&
      response_info.unused_since_prefetch;

  response->was_cookie_in_request = false;
  for (const auto& cookie_with_access_result : request->maybe_sent_cookies()) {
    if (cookie_with_access_result.access_result.status.IsInclude()) {
      // IsInclude() true means the cookie was sent.
      response->was_cookie_in_request = true;
      break;
    }
  }

  if (is_load_timing_enabled)
    request->GetLoadTimingInfo(&response->load_timing);

  if (request->ssl_info().cert.get()) {
    response->ct_policy_compliance = request->ssl_info().ct_policy_compliance;
    response->cert_status = request->ssl_info().cert_status;
    net::SSLVersion ssl_version = net::SSLConnectionStatusToVersion(
        request->ssl_info().connection_status);
    response->is_legacy_tls_version =
        ssl_version == net::SSLVersion::SSL_CONNECTION_VERSION_TLS1 ||
        ssl_version == net::SSLVersion::SSL_CONNECTION_VERSION_TLS1_1;

    if ((options & mojom::kURLLoadOptionSendSSLInfoWithResponse) ||
        (net::IsCertStatusError(request->ssl_info().cert_status) &&
         (options & mojom::kURLLoadOptionSendSSLInfoForCertificateError))) {
      response->ssl_info = request->ssl_info();
    }
  }

  response->request_start = request->creation_time();
  response->response_start = base::TimeTicks::Now();
  response->encoded_data_length = request->GetTotalReceivedBytes();
  response->auth_challenge_info = request->auth_challenge_info();
  response->has_range_requested = request->extra_request_headers().HasHeader(
      net::HttpRequestHeaders::kRange);
  response->dns_aliases = request->response_info().dns_aliases;
  response->request_include_credentials = request->allow_credentials();
}

// A subclass of net::UploadBytesElementReader which owns
// ResourceRequestBody.
class BytesElementReader : public net::UploadBytesElementReader {
 public:
  BytesElementReader(ResourceRequestBody* resource_request_body,
                     const DataElementBytes& element)
      : net::UploadBytesElementReader(element.AsStringPiece().data(),
                                      element.AsStringPiece().size()),
        resource_request_body_(resource_request_body) {}

  ~BytesElementReader() override {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(BytesElementReader);
};

// A subclass of net::UploadFileElementReader which owns
// ResourceRequestBody.
// This class is necessary to ensure the BlobData and any attached shareable
// files survive until upload completion.
class FileElementReader : public net::UploadFileElementReader {
 public:
  FileElementReader(ResourceRequestBody* resource_request_body,
                    base::TaskRunner* task_runner,
                    const DataElementFile& element,
                    base::File&& file)
      : net::UploadFileElementReader(task_runner,
                                     std::move(file),
                                     element.path(),
                                     element.offset(),
                                     element.length(),
                                     element.expected_modification_time()),
        resource_request_body_(resource_request_body) {}

  ~FileElementReader() override {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(FileElementReader);
};

std::unique_ptr<net::UploadDataStream> CreateUploadDataStream(
    ResourceRequestBody* body,
    std::vector<base::File>& opened_files,
    base::SequencedTaskRunner* file_task_runner) {
  // In the case of a chunked upload, there will just be one element.
  if (body->elements()->size() == 1) {
    if (body->elements()->begin()->type() ==
        network::mojom::DataElementDataView::Tag::kChunkedDataPipe) {
      auto& element =
          body->elements_mutable()->at(0).As<DataElementChunkedDataPipe>();
      auto upload_data_stream =
          std::make_unique<ChunkedDataPipeUploadDataStream>(
              body, element.ReleaseChunkedDataPipeGetter());
      if (element.read_only_once()) {
        upload_data_stream->EnableCache();
      }
      return upload_data_stream;
    }
  }

  auto opened_file = opened_files.begin();
  std::vector<std::unique_ptr<net::UploadElementReader>> element_readers;
  for (const auto& element : *body->elements()) {
    switch (element.type()) {
      case network::mojom::DataElementDataView::Tag::kBytes:
        element_readers.push_back(std::make_unique<BytesElementReader>(
            body, element.As<DataElementBytes>()));
        break;
      case network::mojom::DataElementDataView::Tag::kFile:
        DCHECK(opened_file != opened_files.end());
        element_readers.push_back(std::make_unique<FileElementReader>(
            body, file_task_runner, element.As<network::DataElementFile>(),
            std::move(*opened_file++)));
        break;
      case network::mojom::DataElementDataView::Tag::kDataPipe: {
        element_readers.push_back(std::make_unique<DataPipeElementReader>(
            body,
            element.As<network::DataElementDataPipe>().CloneDataPipeGetter()));
        break;
      }
      case network::mojom::DataElementDataView::Tag::kChunkedDataPipe: {
        // This shouldn't happen, as the traits logic should ensure that if
        // there's a chunked pipe, there's one and only one element.
        NOTREACHED();
        break;
      }
    }
  }
  DCHECK(opened_file == opened_files.end());

  return std::make_unique<net::ElementsUploadDataStream>(
      std::move(element_readers), body->identifier());
}

class SSLPrivateKeyInternal : public net::SSLPrivateKey {
 public:
  SSLPrivateKeyInternal(
      const std::string& provider_name,
      const std::vector<uint16_t>& algorithm_preferences,
      mojo::PendingRemote<mojom::SSLPrivateKey> ssl_private_key)
      : provider_name_(provider_name),
        algorithm_preferences_(algorithm_preferences),
        ssl_private_key_(std::move(ssl_private_key)) {
    ssl_private_key_.set_disconnect_handler(
        base::BindOnce(&SSLPrivateKeyInternal::HandleSSLPrivateKeyError,
                       base::Unretained(this)));
  }

  // net::SSLPrivateKey:
  std::string GetProviderName() override { return provider_name_; }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    return algorithm_preferences_;
  }

  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            net::SSLPrivateKey::SignCallback callback) override {
    std::vector<uint8_t> input_vector(input.begin(), input.end());
    if (!ssl_private_key_ || !ssl_private_key_.is_connected()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         net::ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY,
                         input_vector));
      return;
    }

    ssl_private_key_->Sign(algorithm, input_vector,
                           base::BindOnce(&SSLPrivateKeyInternal::Callback,
                                          this, std::move(callback)));
  }

 private:
  ~SSLPrivateKeyInternal() override = default;

  void HandleSSLPrivateKeyError() { ssl_private_key_.reset(); }

  void Callback(net::SSLPrivateKey::SignCallback callback,
                int32_t net_error,
                const std::vector<uint8_t>& input) {
    DCHECK_LE(net_error, 0);
    DCHECK_NE(net_error, net::ERR_IO_PENDING);
    std::move(callback).Run(static_cast<net::Error>(net_error), input);
  }

  std::string provider_name_;
  std::vector<uint16_t> algorithm_preferences_;
  mojo::Remote<mojom::SSLPrivateKey> ssl_private_key_;

  DISALLOW_COPY_AND_ASSIGN(SSLPrivateKeyInternal);
};

bool ShouldNotifyAboutCookie(net::CookieInclusionStatus status) {
  // Notify about cookies actually used, and those blocked by preferences ---
  // for purposes of cookie UI --- as well those carrying warnings pertaining to
  // SameSite features, in order to issue a deprecation warning for them.
  return status.IsInclude() || status.ShouldWarn() ||
         status.HasExclusionReason(
             net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES) ||
         status.HasExclusionReason(
             net::CookieInclusionStatus::EXCLUDE_INVALID_SAMEPARTY);
}

// Concerning headers that consumers probably shouldn't be allowed to set.
// Gathering numbers on these before adding them to kUnsafeHeaders.
const struct {
  const char* name;
  ConcerningHeaderId histogram_id;
} kConcerningHeaders[] = {
    {net::HttpRequestHeaders::kConnection, ConcerningHeaderId::kConnection},
    {net::HttpRequestHeaders::kCookie, ConcerningHeaderId::kCookie},
    {"Date", ConcerningHeaderId::kDate},
    {"Expect", ConcerningHeaderId::kExpect},
    // The referer is passed in from the caller on a per-request basis, but
    // there's a separate field for it that should be used instead.
    {net::HttpRequestHeaders::kReferer, ConcerningHeaderId::kReferer},
    {"Via", ConcerningHeaderId::kVia},
};

void ReportFetchUploadStreamingUMA(const net::URLRequest* request,
                                   bool allow_http1_for_streaming_upload) {
  // Same as tools/metrics/histograms/enums.xml's.
  enum class HttpProtocolScheme {
    kHTTP1_1 = 0,
    kHTTP2 = 1,
    kQUIC = 2,
    kMaxValue = kQUIC
  } protocol;
  const auto connection_info = request->response_info().connection_info;
  switch (net::HttpResponseInfo::ConnectionInfoToCoarse(connection_info)) {
    case net::HttpResponseInfo::CONNECTION_INFO_COARSE_HTTP1:
      protocol = HttpProtocolScheme::kHTTP1_1;
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_COARSE_HTTP2:
      protocol = HttpProtocolScheme::kHTTP2;
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_COARSE_QUIC:
      protocol = HttpProtocolScheme::kQUIC;
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_COARSE_OTHER:
      protocol = HttpProtocolScheme::kHTTP1_1;
      break;
  }
  if (allow_http1_for_streaming_upload) {
    base::UmaHistogramEnumeration("Net.Fetch.UploadStreamingProtocolAllowH1",
                                  protocol);
  } else {
    base::UmaHistogramEnumeration("Net.Fetch.UploadStreamingProtocolNotAllowH1",
                                  protocol);
  }
}

// Parses AcceptCHFrame and removes client hints already in the headers.
std::vector<mojom::WebClientHintsType> ComputeAcceptCHFrameHints(
    const std::string& accept_ch_frame,
    const net::HttpRequestHeaders& headers) {
  absl::optional<std::vector<mojom::WebClientHintsType>> maybe_hints =
      ParseClientHintsHeader(accept_ch_frame);

  if (!maybe_hints)
    return {};

  // Only look at/add headers that aren't already present.
  std::vector<mojom::WebClientHintsType> hints;
  for (auto hint : maybe_hints.value()) {
    const char* header = kClientHintsNameMapping[static_cast<int>(hint)];
    if (!headers.HasHeader(header))
      hints.push_back(hint);
  }

  return hints;
}

// Returns true if the |credentials_mode| of the request allows sending
// credentials.
bool ShouldAllowCredentials(mojom::CredentialsMode credentials_mode) {
  switch (credentials_mode) {
    case mojom::CredentialsMode::kInclude:
    // TODO(crbug.com/943939): Make this work with CredentialsMode::kSameOrigin.
    case mojom::CredentialsMode::kSameOrigin:
      return true;

    case mojom::CredentialsMode::kOmit:
    case mojom::CredentialsMode::kOmitBug_775438_Workaround:
      return false;
  }
}

// Returns true when the |credentials_mode| of the request allows sending client
// certificates.
bool ShouldSendClientCertificates(mojom::CredentialsMode credentials_mode) {
  switch (credentials_mode) {
    case mojom::CredentialsMode::kInclude:
    case mojom::CredentialsMode::kOmit:
    case mojom::CredentialsMode::kSameOrigin:
      return true;

    case mojom::CredentialsMode::kOmitBug_775438_Workaround:
      return false;
  }
}

}  // namespace

URLLoader::URLLoader(
    net::URLRequestContext* url_request_context,
    URLLoaderFactory* url_loader_factory,
    mojom::NetworkContextClient* network_context_client,
    DeleteCallback delete_callback,
    mojo::PendingReceiver<mojom::URLLoader> url_loader_receiver,
    int32_t options,
    const ResourceRequest& request,
    mojo::PendingRemote<mojom::URLLoaderClient> url_loader_client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const mojom::URLLoaderFactoryParams* factory_params,
    mojom::CrossOriginEmbedderPolicyReporter* coep_reporter,
    uint32_t request_id,
    int keepalive_request_size,
    bool require_network_isolation_key,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
    base::WeakPtr<KeepaliveStatisticsRecorder> keepalive_statistics_recorder,
    mojom::TrustedURLLoaderHeaderClient* url_loader_header_client,
    mojom::OriginPolicyManager* origin_policy_manager,
    std::unique_ptr<TrustTokenRequestHelperFactory> trust_token_helper_factory,
    const cors::OriginAccessList& origin_access_list,
    mojo::PendingRemote<mojom::CookieAccessObserver> cookie_observer,
    mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer,
    mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
    mojo::PendingRemote<mojom::AcceptCHFrameObserver> accept_ch_frame_observer)
    : url_request_context_(url_request_context),
      url_loader_factory_(url_loader_factory),
      network_context_client_(network_context_client),
      delete_callback_(std::move(delete_callback)),
      options_(options),
      corb_detachable_(request.corb_detachable),
      resource_type_(request.resource_type),
      is_load_timing_enabled_(request.enable_load_timing),
      factory_params_(factory_params),
      coep_reporter_(coep_reporter),
      request_id_(request_id),
      keepalive_request_size_(keepalive_request_size),
      keepalive_(request.keepalive),
      do_not_prompt_for_login_(request.do_not_prompt_for_login),
      receiver_(this, std::move(url_loader_receiver)),
      url_loader_client_(std::move(url_loader_client)),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               base::SequencedTaskRunnerHandle::Get()),
      peer_closed_handle_watcher_(FROM_HERE,
                                  mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                                  base::SequencedTaskRunnerHandle::Get()),
      devtools_request_id_(request.devtools_request_id),
      request_mode_(request.mode),
      request_credentials_mode_(request.credentials_mode),
      request_destination_(request.destination),
      resource_scheduler_client_(std::move(resource_scheduler_client)),
      keepalive_statistics_recorder_(std::move(keepalive_statistics_recorder)),
      custom_proxy_pre_cache_headers_(request.custom_proxy_pre_cache_headers),
      custom_proxy_post_cache_headers_(request.custom_proxy_post_cache_headers),
      fetch_window_id_(request.fetch_window_id),
      trust_token_helper_factory_(std::move(trust_token_helper_factory)),
      origin_access_list_(origin_access_list),
      cookie_observer_(std::move(cookie_observer)),
      url_loader_network_observer_(std::move(url_loader_network_observer)),
      devtools_observer_(std::move(devtools_observer)),
      has_fetch_streaming_upload_body_(HasFetchStreamingUploadBody(&request)),
      allow_http1_for_streaming_upload_(
          request.request_body &&
          request.request_body->AllowHTTP1ForStreamingUpload()),
      accept_ch_frame_observer_(std::move(accept_ch_frame_observer)) {
  DCHECK(delete_callback_);
  DCHECK(factory_params_);

  if (url_loader_header_client &&
      (options_ & mojom::kURLLoadOptionUseHeaderClient)) {
    if (options_ & mojom::kURLLoadOptionAsCorsPreflight) {
      url_loader_header_client->OnLoaderForCorsPreflightCreated(
          request, header_client_.BindNewPipeAndPassReceiver());
    } else {
      url_loader_header_client->OnLoaderCreated(
          request_id_, header_client_.BindNewPipeAndPassReceiver());
    }
    // Make sure the loader dies if |header_client_| has an error, otherwise
    // requests can hang.
    header_client_.set_disconnect_handler(
        base::BindOnce(&URLLoader::OnMojoDisconnect, base::Unretained(this)));
  }
  if (devtools_request_id()) {
    options_ |= mojom::kURLLoadOptionSendSSLInfoWithResponse |
                mojom::kURLLoadOptionSendSSLInfoForCertificateError;
  }
  receiver_.set_disconnect_handler(
      base::BindOnce(&URLLoader::OnMojoDisconnect, base::Unretained(this)));
  url_request_ = url_request_context_->CreateRequest(
      GURL(request.url), request.priority, this, traffic_annotation);
  url_request_->set_method(request.method);
  url_request_->set_site_for_cookies(request.site_for_cookies);
  if (ShouldForceIgnoreSiteForCookies(request))
    url_request_->set_force_ignore_site_for_cookies(true);
  url_request_->SetReferrer(request.referrer.GetAsReferrer().spec());
  url_request_->set_referrer_policy(request.referrer_policy);
  url_request_->set_upgrade_if_insecure(request.upgrade_if_insecure);

  if (!factory_params_->isolation_info.IsEmpty()) {
    url_request_->set_isolation_info(factory_params_->isolation_info);
  } else if (request.trusted_params &&
             !request.trusted_params->isolation_info.IsEmpty()) {
    url_request_->set_isolation_info(request.trusted_params->isolation_info);
    if (request.credentials_mode != network::mojom::CredentialsMode::kOmit) {
      DCHECK(url_request_->isolation_info().site_for_cookies().IsEquivalent(
          request.site_for_cookies));
    }
  } else if (factory_params_->automatically_assign_isolation_info) {
    url::Origin origin = url::Origin::Create(request.url);
    url_request_->set_isolation_info(
        net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                   origin, origin, net::SiteForCookies()));
  }

  if (require_network_isolation_key)
    DCHECK(!url_request_->isolation_info().IsEmpty());

  if (ShouldForceIgnoreTopFramePartyForCookies())
    url_request_->set_force_ignore_top_frame_party_for_cookies(true);

  if (factory_params_->disable_secure_dns ||
      (request.trusted_params && request.trusted_params->disable_secure_dns)) {
    url_request_->SetSecureDnsPolicy(net::SecureDnsPolicy::kDisable);
  }

  // |cors_excempt_headers| must be merged here to avoid breaking CORS checks.
  // They are non-empty when the values are given by the UA code, therefore
  // they should be ignored by CORS checks.
  net::HttpRequestHeaders merged_headers = request.headers;
  merged_headers.MergeFrom(request.cors_exempt_headers);
  if (request.obey_origin_policy) {
    DCHECK(origin_policy_manager);
    origin_policy_manager_ = origin_policy_manager;
  }
  // This should be ensured by the CorsURLLoaderFactory(), which is called
  // before URLLoaders are created.
  DCHECK(AreRequestHeadersSafe(merged_headers));
  url_request_->SetExtraRequestHeaders(merged_headers);

  url_request_->SetUserData(kUserDataKey,
                            std::make_unique<UnownedPointer>(this));
  url_request_->set_accepted_stream_types(
      request.devtools_accepted_stream_types);

  if (request.trusted_params) {
    has_user_activation_ = request.trusted_params->has_user_activation;

    if (factory_params_->client_security_state) {
      // Enforce that only one ClientSecurityState is ever given to us, as this
      // is an invariant in the current codebase. In case of a compromised
      // renderer process, we might be passed both, in which case we prefer to
      // use the factory params' value: contrary to the request params, it is
      // always sourced from the browser process.
      DCHECK(!request.trusted_params->client_security_state)
          << "Must not provide a ClientSecurityState in both "
             "URLLoaderFactoryParams and ResourceRequest::TrustedParams.";
    } else {
      // This might be nullptr, but that does not matter. Clone it anyways.
      request_client_security_state_ =
          request.trusted_params->client_security_state.Clone();
    }
  }

  throttling_token_ = network::ScopedThrottlingToken::MaybeCreate(
      url_request_->net_log().source().id, request.throttling_profile_id);

  url_request_->set_initiator(request.request_initiator);

  SetFetchMetadataHeaders(url_request_.get(), request_mode_,
                          has_user_activation_, request_destination_, nullptr,
                          *factory_params_, origin_access_list_);

  if (request.update_first_party_url_on_redirect) {
    url_request_->set_first_party_url_policy(
        net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);
  }

  url_request_->SetLoadFlags(request.load_flags);
  SetRequestCredentials(request.url);

  url_request_->SetRequestHeadersCallback(base::BindRepeating(
      &URLLoader::SetRawRequestHeadersAndNotify, base::Unretained(this)));

  if (devtools_request_id()) {
    url_request_->SetResponseHeadersCallback(base::BindRepeating(
        &URLLoader::SetRawResponseHeaders, base::Unretained(this)));
  }

  url_request_->SetEarlyResponseHeadersCallback(base::BindRepeating(
      &URLLoader::NotifyEarlyResponse, base::Unretained(this)));

  if (keepalive_ && keepalive_statistics_recorder_) {
    keepalive_statistics_recorder_->OnLoadStarted(
        *factory_params_->top_frame_id, keepalive_request_size_);
  }

#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kRecordRadioWakeupTrigger)) {
    RadioMonitorAndroid::GetInstance().MaybeRecordURLLoaderAnnotationId(
        traffic_annotation);
  }
#endif

  // Resolve elements from request_body and prepare upload data.
  if (request.request_body.get()) {
    OpenFilesForUpload(request);
    return;
  }

  BeginTrustTokenOperationIfNecessaryAndThenScheduleStart(request);
}

// This class is used to manage the queue of pending file upload operations
// initiated by the URLLoader::OpenFilesForUpload().
class URLLoader::FileOpenerForUpload {
 public:
  typedef base::OnceCallback<void(int, std::vector<base::File>)>
      SetUpUploadCallback;

  FileOpenerForUpload(std::vector<base::FilePath> paths,
                      URLLoader* url_loader,
                      int32_t process_id,
                      mojom::NetworkContextClient* const network_context_client,
                      SetUpUploadCallback set_up_upload_callback)
      : paths_(std::move(paths)),
        url_loader_(url_loader),
        process_id_(process_id),
        network_context_client_(network_context_client),
        set_up_upload_callback_(std::move(set_up_upload_callback)) {
    StartOpeningNextBatch();
  }

  ~FileOpenerForUpload() {
    if (!opened_files_.empty())
      PostCloseFiles(std::move(opened_files_));
  }

 private:
  static void OnFilesForUploadOpened(
      base::WeakPtr<FileOpenerForUpload> file_opener,
      size_t num_files_requested,
      int error_code,
      std::vector<base::File> opened_files) {
    if (!file_opener) {
      PostCloseFiles(std::move(opened_files));
      return;
    }

    if (error_code == net::OK && num_files_requested != opened_files.size())
      error_code = net::ERR_FAILED;

    if (error_code != net::OK) {
      PostCloseFiles(std::move(opened_files));
      file_opener->FilesForUploadOpenedDone(error_code);
      return;
    }

    for (base::File& file : opened_files)
      file_opener->opened_files_.push_back(std::move(file));

    if (file_opener->opened_files_.size() < file_opener->paths_.size()) {
      file_opener->StartOpeningNextBatch();
      return;
    }

    file_opener->FilesForUploadOpenedDone(net::OK);
  }

  // |opened_files| need to be closed on a blocking task runner, so move the
  // |opened_files| vector onto a sequence that can block so it gets destroyed
  // there.
  static void PostCloseFiles(std::vector<base::File> opened_files) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
        base::BindOnce(base::DoNothing::Once<std::vector<base::File>>(),
                       std::move(opened_files)));
  }

  void StartOpeningNextBatch() {
    size_t num_files_to_request = std::min(paths_.size() - opened_files_.size(),
                                           kMaxFileUploadRequestsPerBatch);
    std::vector<base::FilePath> batch_paths(
        paths_.begin() + opened_files_.size(),
        paths_.begin() + opened_files_.size() + num_files_to_request);

    network_context_client_->OnFileUploadRequested(
        process_id_, /*async=*/true, batch_paths,
        base::BindOnce(&FileOpenerForUpload::OnFilesForUploadOpened,
                       weak_ptr_factory_.GetWeakPtr(), num_files_to_request));
  }

  void FilesForUploadOpenedDone(int error_code) {
    url_loader_->url_request_->LogUnblocked();

    if (error_code == net::OK)
      std::move(set_up_upload_callback_).Run(net::OK, std::move(opened_files_));
    else
      std::move(set_up_upload_callback_).Run(error_code, {});
  }

  // The paths of files for upload
  const std::vector<base::FilePath> paths_;
  URLLoader* const url_loader_;
  const int32_t process_id_;
  mojom::NetworkContextClient* const network_context_client_;
  SetUpUploadCallback set_up_upload_callback_;
  // The files opened so far.
  std::vector<base::File> opened_files_;

  base::WeakPtrFactory<FileOpenerForUpload> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FileOpenerForUpload);
};

void URLLoader::OpenFilesForUpload(const ResourceRequest& request) {
  std::vector<base::FilePath> paths;
  for (const auto& element : *request.request_body.get()->elements()) {
    if (element.type() == mojom::DataElementDataView::Tag::kFile) {
      paths.push_back(element.As<network::DataElementFile>().path());
    }
  }
  if (paths.empty()) {
    SetUpUpload(request, net::OK, std::vector<base::File>());
    return;
  }
  if (!network_context_client_) {
    DLOG(ERROR) << "URLLoader couldn't upload a file because no "
                   "NetworkContextClient is set.";
    // Defer calling NotifyCompleted to make sure the URLLoader finishes
    // initializing before getting deleted.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&URLLoader::NotifyCompleted, base::Unretained(this),
                       net::ERR_ACCESS_DENIED));
    return;
  }
  url_request_->LogBlockedBy("Opening Files");
  file_opener_for_upload_ = std::make_unique<FileOpenerForUpload>(
      std::move(paths), this, factory_params_->process_id,
      network_context_client_,
      base::BindOnce(&URLLoader::SetUpUpload, base::Unretained(this), request));
}

void URLLoader::SetUpUpload(const ResourceRequest& request,
                            int error_code,
                            std::vector<base::File> opened_files) {
  if (error_code != net::OK) {
    DCHECK(opened_files.empty());
    // Defer calling NotifyCompleted to make sure the URLLoader finishes
    // initializing before getting deleted.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&URLLoader::NotifyCompleted,
                                  base::Unretained(this), error_code));
    return;
  }
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  url_request_->set_upload(CreateUploadDataStream(
      request.request_body.get(), opened_files, task_runner.get()));

  if (request.enable_upload_progress) {
    upload_progress_tracker_ = std::make_unique<UploadProgressTracker>(
        FROM_HERE,
        base::BindRepeating(&URLLoader::SendUploadProgress,
                            base::Unretained(this)),
        url_request_.get());
  }
  BeginTrustTokenOperationIfNecessaryAndThenScheduleStart(request);
}

void URLLoader::BeginTrustTokenOperationIfNecessaryAndThenScheduleStart(
    const ResourceRequest& request) {
  if (!request.trust_token_params) {
    ScheduleStart();
    return;
  }

  // Since the request has trust token parameters, |trust_token_helper_factory_|
  // is guaranteed to be non-null by URLLoader's constructor's contract.
  DCHECK(trust_token_helper_factory_);

  trust_token_helper_factory_->CreateTrustTokenHelperForRequest(
      *url_request_, request.trust_token_params.value(),
      base::BindOnce(&URLLoader::OnDoneConstructingTrustTokenHelper,
                     weak_ptr_factory_.GetWeakPtr(),
                     request.trust_token_params->type));
}

void URLLoader::OnDoneConstructingTrustTokenHelper(
    mojom::TrustTokenOperationType type,
    TrustTokenStatusOrRequestHelper status_or_helper) {
  if (!status_or_helper.ok()) {
    trust_token_status_ = status_or_helper.status();

    // Defer calling NotifyCompleted to make sure the URLLoader
    // finishes initializing before getting deleted.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&URLLoader::NotifyCompleted,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  net::ERR_TRUST_TOKEN_OPERATION_FAILED));

    auto* devtools_observer = GetDevToolsObserver();
    if (devtools_observer && devtools_request_id()) {
      mojom::TrustTokenOperationResultPtr operation_result =
          mojom::TrustTokenOperationResult::New();
      operation_result->status = *trust_token_status_;
      operation_result->type = type;
      devtools_observer->OnTrustTokenOperationDone(
          devtools_request_id().value(), std::move(operation_result));
    }
    return;
  }

  trust_token_helper_ = status_or_helper.TakeOrCrash();
  trust_token_helper_->Begin(
      url_request_.get(),
      base::BindOnce(&URLLoader::OnDoneBeginningTrustTokenOperation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void URLLoader::OnDoneBeginningTrustTokenOperation(
    mojom::TrustTokenOperationStatus status) {
  trust_token_status_ = status;

  // In case the operation failed or it succeeded in a manner where the request
  // does not need to be sent onwards, the DevTools event is emitted from here.
  // Otherwise the DevTools event is always emitted from
  // |OnDoneFinalizingTrustTokenOperation|.
  if (status != mojom::TrustTokenOperationStatus::kOk) {
    MaybeSendTrustTokenOperationResultToDevTools();
  }

  if (status == mojom::TrustTokenOperationStatus::kOk) {
    ScheduleStart();
  } else if (status == mojom::TrustTokenOperationStatus::kAlreadyExists ||
             status == mojom::TrustTokenOperationStatus::
                           kOperationSuccessfullyFulfilledLocally) {
    // The Trust Tokens operation succeeded without needing to send the request;
    // we return early with an "error" representing this success.
    //
    // Here and below, defer calling NotifyCompleted to make sure the URLLoader
    // finishes initializing before getting deleted.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &URLLoader::NotifyCompleted, weak_ptr_factory_.GetWeakPtr(),
            net::ERR_TRUST_TOKEN_OPERATION_SUCCESS_WITHOUT_SENDING_REQUEST));
  } else {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&URLLoader::NotifyCompleted,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  net::ERR_TRUST_TOKEN_OPERATION_FAILED));
  }
}

void URLLoader::ScheduleStart() {
  bool defer = false;
  if (resource_scheduler_client_) {
    resource_scheduler_request_handle_ =
        resource_scheduler_client_->ScheduleRequest(
            !(options_ & mojom::kURLLoadOptionSynchronous), url_request_.get());
    resource_scheduler_request_handle_->set_resume_callback(
        base::BindOnce(&URLLoader::ResumeStart, base::Unretained(this)));
    resource_scheduler_request_handle_->WillStartRequest(&defer);
  }
  if (defer)
    url_request_->LogBlockedBy("ResourceScheduler");
  else
    url_request_->Start();
}

URLLoader::~URLLoader() {
  RecordBodyReadFromNetBeforePausedIfNeeded();
  if (keepalive_ && keepalive_statistics_recorder_) {
    keepalive_statistics_recorder_->OnLoadFinished(
        *factory_params_->top_frame_id, keepalive_request_size_);
  }
}

// static
const void* const URLLoader::kUserDataKey = &URLLoader::kUserDataKey;

void URLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  if (!deferred_redirect_url_) {
    NOTREACHED();
    return;
  }

  // Removing headers can't make the set of pre-existing headers unsafe, but
  // adding headers can.
  if (!AreRequestHeadersSafe(modified_headers) |
      !AreRequestHeadersSafe(modified_cors_exempt_headers)) {
    NotifyCompleted(net::ERR_INVALID_ARGUMENT);
    // |this| may have been deleted.
    return;
  }

  if (!modified_headers.IsEmpty())
    LogConcerningRequestHeaders(modified_headers,
                                true /* added_during_redirect */);

  deferred_redirect_url_.reset();
  new_redirect_url_ = new_url;

  net::HttpRequestHeaders merged_modified_headers;
  merged_modified_headers.CopyFrom(modified_headers);
  merged_modified_headers.MergeFrom(modified_cors_exempt_headers);
  url_request_->FollowDeferredRedirect(removed_headers,
                                       merged_modified_headers);
  new_redirect_url_.reset();
}

void URLLoader::SetPriority(net::RequestPriority priority,
                            int32_t intra_priority_value) {
  if (url_request_ && resource_scheduler_client_) {
    resource_scheduler_client_->ReprioritizeRequest(
        url_request_.get(), priority, intra_priority_value);
  }
}

void URLLoader::PauseReadingBodyFromNet() {
  DVLOG(1) << "URLLoader pauses fetching response body for "
           << (url_request_ ? url_request_->original_url().spec()
                            : "a URL that has completed loading or failed.");

  // Please note that we pause reading body in all cases. Even if the URL
  // request indicates that the response was cached, there could still be
  // network activity involved. For example, the response was only partially
  // cached.
  //
  // On the other hand, we only report BodyReadFromNetBeforePaused histogram
  // when we are sure that the response body hasn't been read from cache. This
  // avoids polluting the histogram data with data points from cached responses.
  should_pause_reading_body_ = true;

  if (read_in_progress_) {
    update_body_read_before_paused_ = true;
  } else {
    body_read_before_paused_ = url_request_->GetRawBodyBytes();
  }
}

void URLLoader::ResumeReadingBodyFromNet() {
  DVLOG(1) << "URLLoader resumes fetching response body for "
           << (url_request_ ? url_request_->original_url().spec()
                            : "a URL that has completed loading or failed.");
  should_pause_reading_body_ = false;

  if (paused_reading_body_) {
    paused_reading_body_ = false;
    ReadMore();
  }
}

bool URLLoader::CanConnectToAddressSpace(
    mojom::IPAddressSpace resource_address_space) const {
  if (options_ & mojom::kURLLoadOptionBlockLocalRequest &&
      IsLessPublicAddressSpace(resource_address_space,
                               network::mojom::IPAddressSpace::kPublic)) {
    // Unconditionally block requests to non-public address spaces when the URL
    // loader options say so.
    return false;
  }

  // Depending on the type of URL request, we source the client security state
  // from either the URLRequest's trusted params (for navigations, which share
  // a factory) or the URLLoaderFactory's params. We prefer the factory params
  // over the request params, as the former always come from the browser
  // process.
  const mojom::ClientSecurityStatePtr& security_state =
      factory_params_->client_security_state
          ? factory_params_->client_security_state
          : request_client_security_state_;

  if (!security_state) {
    // Missing security state: allow all requests.
    return true;
  }

  if (!IsLessPublicAddressSpace(resource_address_space,
                                security_state->ip_address_space)) {
    // Resource is no less public than the initiator.
    return true;
  }

  bool is_warning = false;
  // We use a switch statement to force this code to be amended when values are
  // added to the PrivateNetworkRequestPolicy enum.
  switch (security_state->private_network_request_policy) {
    case mojom::PrivateNetworkRequestPolicy::kAllow:
      // Policy tells us to allow all.
      return true;
    case mojom::PrivateNetworkRequestPolicy::kWarn:
      is_warning = true;
      break;
    case mojom::PrivateNetworkRequestPolicy::kBlock:
      is_warning = false;
      break;
  }

  if (auto* devtools_observer = GetDevToolsObserver()) {
    devtools_observer->OnPrivateNetworkRequest(
        devtools_request_id(), url_request_->url(), is_warning,
        resource_address_space, security_state->Clone());
  }
  return is_warning;
}

int URLLoader::OnConnected(net::URLRequest* url_request,
                           const net::TransportInfo& info,
                           net::CompletionOnceCallback callback) {
  DCHECK_EQ(url_request, url_request_.get());

  DVLOG(1) << "Connection obtained for URL request to " << url_request->url()
           << ": " << info;

  // Now that the request endpoint's address has been resolved, check if
  // this request should be blocked by CORS-RFC1918 rules.
  mojom::IPAddressSpace resource_address_space =
      IPEndPointToIPAddressSpace(info.endpoint);
  if (!CanConnectToAddressSpace(resource_address_space)) {
    // Remember the CORS error so we can annotate the URLLoaderCompletionStatus
    // with it later, then fail the request with the same net error code as
    // other CORS errors.
    cors_error_status_ = CorsErrorStatus(resource_address_space);
    return net::ERR_FAILED;
  }

  if (!accept_ch_frame_observer_ || info.accept_ch_frame.empty() ||
      !base::FeatureList::IsEnabled(features::kAcceptCHFrame)) {
    return net::OK;
  }

  // Find client hints that are in the ACCEPT_CH frame that were not already
  // included in the request
  std::vector<mojom::WebClientHintsType> hints = ComputeAcceptCHFrameHints(
      info.accept_ch_frame, url_request->extra_request_headers());

  // If there are hints in the ACCEPT_CH frame that weren't included in the
  // original request, notify the observer. If those hints can be included,
  // this URLLoader will be destroyed and another with the correct hints
  // started. Otherwise, the callback to continue the network transaction will
  // be called and the URLLoader will continue as normal.
  if (!hints.empty()) {
    accept_ch_frame_observer_->OnAcceptCHFrameReceived(
        url_request->url(), hints, std::move(callback));
    return net::ERR_IO_PENDING;
  }

  return net::OK;
}

void URLLoader::OnReceivedRedirect(net::URLRequest* url_request,
                                   const net::RedirectInfo& redirect_info,
                                   bool* defer_redirect) {
  DCHECK(url_request == url_request_.get());

  DCHECK(!deferred_redirect_url_);
  deferred_redirect_url_ = std::make_unique<GURL>(redirect_info.new_url);

  // Send the redirect response to the client, allowing them to inspect it and
  // optionally follow the redirect.
  *defer_redirect = true;

  auto response = network::mojom::URLResponseHead::New();
  PopulateResourceResponse(url_request_.get(), is_load_timing_enabled_,
                           options_, response.get());

  ReportFlaggedResponseCookies();

  const CrossOriginEmbedderPolicy kEmpty;
  // Enforce the Cross-Origin-Resource-Policy (CORP) header.
  const CrossOriginEmbedderPolicy& cross_origin_embedder_policy =
      factory_params_->client_security_state
          ? factory_params_->client_security_state->cross_origin_embedder_policy
          : kEmpty;

  if (absl::optional<mojom::BlockedByResponseReason> blocked_reason =
          CrossOriginResourcePolicy::IsBlocked(
              url_request_->url(), url_request_->original_url(),
              url_request_->initiator(), *response, request_mode_,
              request_destination_, cross_origin_embedder_policy,
              coep_reporter_)) {
    CompleteBlockedResponse(net::ERR_BLOCKED_BY_RESPONSE, false,
                            blocked_reason);
    // TODO(https://crbug.com/1154250):  Close the socket here.
    // For more details see https://crbug.com/1154250#c17.
    // Item 2 discusses redirect handling.
    //
    // "url_request_->AbortAndCloseConnection()" should ideally close the
    // socket, but unfortunately, URLRequestHttpJob caches redirects in a way
    // that ignores their response bodies, since they'll never be read. It does
    // this by calling HttpCache::Transaction::StopCaching(), which also has the
    // effect of detaching the HttpNetworkTransaction, which owns the socket,
    // from the HttpCache::Transaction. To fix this, we'd either need to call
    // StopCaching() later in the process, or make the HttpCache::Transaction
    // continue to hang onto the HttpNetworkTransaction after this call.
    DeleteSelf();
    return;
  }

  SetRequestCredentials(redirect_info.new_url);

  // We may need to clear out old Sec- prefixed request headers. We'll attempt
  // to do this before we re-add any.
  MaybeRemoveSecHeaders(url_request_.get(), redirect_info.new_url);
  SetFetchMetadataHeaders(url_request_.get(), request_mode_,
                          has_user_activation_, request_destination_,
                          &redirect_info.new_url, *factory_params_,
                          origin_access_list_);

  url_loader_client_->OnReceiveRedirect(redirect_info, std::move(response));
}

// static
bool URLLoader::HasFetchStreamingUploadBody(const ResourceRequest* request) {
  const ResourceRequestBody* request_body = request->request_body.get();
  if (!request_body)
    return false;
  const std::vector<DataElement>* elements = request_body->elements();
  if (elements->size() != 1u)
    return false;
  const auto& element = elements->front();
  return element.type() == mojom::DataElementDataView::Tag::kChunkedDataPipe &&
         element.As<network::DataElementChunkedDataPipe>().read_only_once();
}

void URLLoader::OnAuthRequired(net::URLRequest* url_request,
                               const net::AuthChallengeInfo& auth_info) {
  if (has_fetch_streaming_upload_body_) {
    NotifyCompleted(net::ERR_FAILED);
    // |this| may have been deleted.
    return;
  }
  auto* url_loader_network_observer = GetURLLoaderNetworkServiceObserver();
  if (!url_loader_network_observer) {
    OnAuthCredentials(absl::nullopt);
    return;
  }

  if (do_not_prompt_for_login_) {
    OnAuthCredentials(absl::nullopt);
    return;
  }

  DCHECK(!auth_challenge_responder_receiver_.is_bound());

  url_loader_network_observer->OnAuthRequired(
      fetch_window_id_, request_id_, url_request_->url(), first_auth_attempt_,
      auth_info, url_request->response_headers(),
      auth_challenge_responder_receiver_.BindNewPipeAndPassRemote());

  auth_challenge_responder_receiver_.set_disconnect_handler(
      base::BindOnce(&URLLoader::DeleteSelf, base::Unretained(this)));

  first_auth_attempt_ = false;
}

void URLLoader::OnCertificateRequested(net::URLRequest* unused,
                                       net::SSLCertRequestInfo* cert_info) {
  DCHECK(!client_cert_responder_receiver_.is_bound());
  auto* url_loader_network_observer = GetURLLoaderNetworkServiceObserver();
  if (!url_loader_network_observer) {
    CancelRequest();
    return;
  }

  // Set up mojo endpoints for ClientCertificateResponder and bind to the
  // Receiver. This enables us to receive messages regarding the client
  // certificate selection.
  url_loader_network_observer->OnCertificateRequested(
      fetch_window_id_, cert_info,
      client_cert_responder_receiver_.BindNewPipeAndPassRemote());
  client_cert_responder_receiver_.set_disconnect_handler(
      base::BindOnce(&URLLoader::CancelRequest, base::Unretained(this)));
}

void URLLoader::OnSSLCertificateError(net::URLRequest* request,
                                      int net_error,
                                      const net::SSLInfo& ssl_info,
                                      bool fatal) {
  auto* url_loader_network_observer = GetURLLoaderNetworkServiceObserver();
  if (!url_loader_network_observer) {
    OnSSLCertificateErrorResponse(ssl_info, net::ERR_INSECURE_RESPONSE);
    return;
  }
  url_loader_network_observer->OnSSLCertificateError(
      url_request_->url(), net_error, ssl_info, fatal,
      base::BindOnce(&URLLoader::OnSSLCertificateErrorResponse,
                     weak_ptr_factory_.GetWeakPtr(), ssl_info));
}

void URLLoader::OnResponseStarted(net::URLRequest* url_request, int net_error) {
  DCHECK(url_request == url_request_.get());
  has_received_response_ = true;

  ReportFlaggedResponseCookies();
  if (has_fetch_streaming_upload_body_) {
    ReportFetchUploadStreamingUMA(url_request_.get(),
                                  allow_http1_for_streaming_upload_);
  }

  if (net_error != net::OK) {
    NotifyCompleted(net_error);
    // |this| may have been deleted.
    return;
  }

  response_ = network::mojom::URLResponseHead::New();
  PopulateResourceResponse(url_request_.get(), is_load_timing_enabled_,
                           options_, response_.get());

  // Parse and remove the Trust Tokens response headers, if any are expected,
  // potentially failing the request if an error occurs.
  if (trust_token_helper_) {
    trust_token_helper_->Finalize(
        response_.get(),
        base::BindOnce(&URLLoader::OnDoneFinalizingTrustTokenOperation,
                       weak_ptr_factory_.GetWeakPtr()));
    // |this| may have been deleted.
    return;
  }

  ContinueOnResponseStarted();
}

void URLLoader::OnDoneFinalizingTrustTokenOperation(
    mojom::TrustTokenOperationStatus status) {
  trust_token_status_ = status;

  MaybeSendTrustTokenOperationResultToDevTools();

  if (status != mojom::TrustTokenOperationStatus::kOk) {
    NotifyCompleted(net::ERR_TRUST_TOKEN_OPERATION_FAILED);
    // |this| may have been deleted.
    return;
  }
  ContinueOnResponseStarted();
}

void URLLoader::MaybeSendTrustTokenOperationResultToDevTools() {
  CHECK(trust_token_helper_ && trust_token_status_);

  auto* devtools_observer = GetDevToolsObserver();
  if (!devtools_observer || !devtools_request_id())
    return;

  mojom::TrustTokenOperationResultPtr operation_result =
      trust_token_helper_->CollectOperationResultWithStatus(
          *trust_token_status_);
  devtools_observer->OnTrustTokenOperationDone(devtools_request_id().value(),
                                               std::move(operation_result));
}

void URLLoader::ContinueOnResponseStarted() {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      network::features::GetDataPipeDefaultAllocationSize();
  MojoResult result =
      mojo::CreateDataPipe(&options, response_body_stream_, consumer_handle_);
  if (result != MOJO_RESULT_OK) {
    NotifyCompleted(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }
  DCHECK(response_body_stream_.is_valid());
  DCHECK(consumer_handle_.is_valid());

  // Do not account header bytes when reporting received body bytes to client.
  reported_total_encoded_bytes_ = url_request_->GetTotalReceivedBytes();

  if (upload_progress_tracker_) {
    upload_progress_tracker_->OnUploadCompleted();
    upload_progress_tracker_ = nullptr;
  }

  peer_closed_handle_watcher_.Watch(
      response_body_stream_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&URLLoader::OnResponseBodyStreamConsumerClosed,
                          base::Unretained(this)));
  peer_closed_handle_watcher_.ArmOrNotify();

  writable_handle_watcher_.Watch(
      response_body_stream_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::BindRepeating(&URLLoader::OnResponseBodyStreamReady,
                          base::Unretained(this)));

  // Enforce the Cross-Origin-Resource-Policy (CORP) header.
  const CrossOriginEmbedderPolicy kEmpty;
  const CrossOriginEmbedderPolicy& cross_origin_embedder_policy =
      factory_params_->client_security_state
          ? factory_params_->client_security_state->cross_origin_embedder_policy
          : kEmpty;
  if (absl::optional<mojom::BlockedByResponseReason> blocked_reason =
          CrossOriginResourcePolicy::IsBlocked(
              url_request_->url(), url_request_->original_url(),
              url_request_->initiator(), *response_, request_mode_,
              request_destination_, cross_origin_embedder_policy,
              coep_reporter_)) {
    CompleteBlockedResponse(net::ERR_BLOCKED_BY_RESPONSE, false,
                            blocked_reason);
    // Close the socket associated with the request, to prevent leaking
    // information.
    url_request_->AbortAndCloseConnection();
    DeleteSelf();
    return;
  }

  // Figure out if we need to sniff (for MIME type detection or for Cross-Origin
  // Read Blocking / CORB).
  LogUmaForOpaqueResponseBlocking(url_request_->url(),
                                  url_request_->initiator(), request_mode_,
                                  request_destination_, *response_);
  if (factory_params_->is_corb_enabled) {
    CrossOriginReadBlocking::LogAction(
        CrossOriginReadBlocking::Action::kResponseStarted);

    corb_analyzer_ =
        std::make_unique<CrossOriginReadBlocking::ResponseAnalyzer>(
            url_request_->url(), url_request_->initiator(), *response_,
            request_mode_);
    is_more_corb_sniffing_needed_ = corb_analyzer_->needs_sniffing();
    if (corb_analyzer_->ShouldBlock()) {
      DCHECK(!is_more_corb_sniffing_needed_);
      corb_analyzer_->LogBlockedResponse();
      if (BlockResponseForCorb() == kWillCancelRequest)
        return;
    } else if (corb_analyzer_->ShouldAllow()) {
      DCHECK(!is_more_corb_sniffing_needed_);
      corb_analyzer_->LogAllowedResponse();
    }
  }
  if ((options_ & mojom::kURLLoadOptionSniffMimeType)) {
    if (ShouldSniffContent(url_request_->url(), *response_)) {
      // We're going to look at the data before deciding what the content type
      // is.  That means we need to delay sending the response started IPC.
      VLOG(1) << "Will sniff content for mime type: " << url_request_->url();
      is_more_mime_sniffing_needed_ = true;
    } else if (response_->mime_type.empty()) {
      // Ugg.  The server told us not to sniff the content but didn't give us
      // a mime type.  What's a browser to do?  Turns out, we're supposed to
      // treat the response as "text/plain".  This is the most secure option.
      response_->mime_type.assign("text/plain");
    }
  }

  // If necessary, retrieve the associated origin policy, before sending the
  // response to the client.
  if (origin_policy_manager_ && url_request_->response_headers()) {
    // The request should have been rejected in IsolationInfo if this were
    // empty.
    DCHECK(!url_request_->isolation_info().IsEmpty());

    absl::optional<std::string> origin_policy_header;
    std::string origin_policy_header_value;
    if (url_request_->response_headers()->GetNormalizedHeader(
            "origin-policy", &origin_policy_header_value)) {
      origin_policy_header = origin_policy_header_value;
    }

    OriginPolicyManager::RetrieveOriginPolicyCallback
        origin_policy_manager_done =
            base::BindOnce(&URLLoader::OnOriginPolicyManagerRetrieveDone,
                           weak_ptr_factory_.GetWeakPtr());

    // Create IsolationInfo as if this were an uncredentialed subresource
    // request of the original URL.
    net::IsolationInfo isolation_info = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther,
        url_request_->isolation_info().top_frame_origin().value(),
        url_request_->isolation_info().frame_origin().value(),
        net::SiteForCookies());
    origin_policy_manager_->RetrieveOriginPolicy(
        url::Origin::Create(url_request_->url()), isolation_info,
        origin_policy_header, std::move(origin_policy_manager_done));

    // The callback will continue by calling
    // `StartReading()` after retrieving the origin
    // policy.
    return;
  }

  StartReading();
}

void URLLoader::ReadMore() {
  DCHECK(!read_in_progress_);
  // Once the MIME type is sniffed, all data is sent as soon as it is read from
  // the network.
  DCHECK(consumer_handle_.is_valid() || !pending_write_);

  if (should_pause_reading_body_) {
    paused_reading_body_ = true;
    return;
  }

  if (!pending_write_.get()) {
    // TODO: we should use the abstractions in MojoAsyncResourceHandler.
    DCHECK_EQ(0u, pending_write_buffer_offset_);
    MojoResult result = NetToMojoPendingBuffer::BeginWrite(
        &response_body_stream_, &pending_write_, &pending_write_buffer_size_);
    if (result != MOJO_RESULT_OK && result != MOJO_RESULT_SHOULD_WAIT) {
      // The response body stream is in a bad state. Bail.
      NotifyCompleted(net::ERR_FAILED);
      return;
    }

    DCHECK_GT(static_cast<uint32_t>(std::numeric_limits<int>::max()),
              pending_write_buffer_size_);
    if (consumer_handle_.is_valid()) {
      DCHECK_GE(pending_write_buffer_size_,
                static_cast<uint32_t>(net::kMaxBytesToSniff));
    }
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      // The pipe is full. We need to wait for it to have more space.
      writable_handle_watcher_.ArmOrNotify();
      return;
    }
  }

  auto buf = base::MakeRefCounted<NetToMojoIOBuffer>(
      pending_write_.get(), pending_write_buffer_offset_);
  read_in_progress_ = true;
  int bytes_read = url_request_->Read(
      buf.get(), static_cast<int>(pending_write_buffer_size_ -
                                  pending_write_buffer_offset_));
  if (bytes_read != net::ERR_IO_PENDING) {
    DidRead(bytes_read, true);
    // |this| may have been deleted.
  }
}

void URLLoader::DidRead(int num_bytes, bool completed_synchronously) {
  DCHECK(read_in_progress_);
  read_in_progress_ = false;

  size_t new_data_offset = pending_write_buffer_offset_;
  if (num_bytes > 0) {
    pending_write_buffer_offset_ += num_bytes;

    // Only notify client of download progress if we're done sniffing and
    // started sending response.
    if (!consumer_handle_.is_valid()) {
      int64_t total_encoded_bytes = url_request_->GetTotalReceivedBytes();
      int64_t delta = total_encoded_bytes - reported_total_encoded_bytes_;
      DCHECK_LE(0, delta);
      if (delta)
        url_loader_client_->OnTransferSizeUpdated(delta);
      reported_total_encoded_bytes_ = total_encoded_bytes;
    }
  }
  if (update_body_read_before_paused_) {
    update_body_read_before_paused_ = false;
    body_read_before_paused_ = url_request_->GetRawBodyBytes();
  }

  bool complete_read = true;
  if (consumer_handle_.is_valid()) {
    // |pending_write_| may be null if the job self-aborts due to a suspend;
    // this will have |consumer_handle_| valid when the loader is paused.
    if (pending_write_) {
      // Limit sniffing to the first net::kMaxBytesToSniff.
      size_t data_length = pending_write_buffer_offset_;
      if (data_length > net::kMaxBytesToSniff)
        data_length = net::kMaxBytesToSniff;

      base::StringPiece data(pending_write_->buffer(), data_length);

      if (is_more_mime_sniffing_needed_) {
        const std::string& type_hint = response_->mime_type;
        std::string new_type;
        is_more_mime_sniffing_needed_ = !net::SniffMimeType(
            data, url_request_->url(), type_hint,
            net::ForceSniffFileUrlsForHtml::kDisabled, &new_type);
        // SniffMimeType() returns false if there is not enough data to
        // determine the mime type. However, even if it returns false, it
        // returns a new type that is probably better than the current one.
        response_->mime_type.assign(new_type);
        response_->did_mime_sniff = true;
      }

      if (is_more_corb_sniffing_needed_) {
        corb_analyzer_->SniffResponseBody(data, new_data_offset);
        if (corb_analyzer_->ShouldBlock()) {
          corb_analyzer_->LogBlockedResponse();
          is_more_corb_sniffing_needed_ = false;
          if (BlockResponseForCorb() == kWillCancelRequest)
            return;
        } else if (corb_analyzer_->ShouldAllow()) {
          corb_analyzer_->LogAllowedResponse();
          is_more_corb_sniffing_needed_ = false;
        }
      }
    }

    if (num_bytes <= 0 ||
        pending_write_buffer_offset_ >= net::kMaxBytesToSniff) {
      is_more_mime_sniffing_needed_ = false;

      if (is_more_corb_sniffing_needed_) {
        corb_analyzer_->LogAllowedResponse();
        is_more_corb_sniffing_needed_ = false;
      }
    }

    if (!is_more_mime_sniffing_needed_ && !is_more_corb_sniffing_needed_) {
      SendResponseToClient();
    } else {
      complete_read = false;
    }
  }

  if (num_bytes <= 0) {
    // There may be no |pending_write_| if a URLRequestJob cancelled itself in
    // URLRequestJob::OnSuspend() after receiving headers, while there was no
    // pending read.
    // TODO(mmenke): That case is rather unfortunate - something should be done
    // at the socket layer instead, both to make for a better API (Only complete
    // reads when there's a pending read), and to cover all TCP socket uses,
    // since the concern is the effect that entering suspend mode has on
    // sockets. See https://crbug.com/651120.
    if (pending_write_)
      CompletePendingWrite(num_bytes == 0);
    NotifyCompleted(num_bytes);
    // |this| will have been deleted.
    return;
  }

  if (complete_read) {
    CompletePendingWrite(true /* success */);
  }
  if (completed_synchronously) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&URLLoader::ReadMore, weak_ptr_factory_.GetWeakPtr()));
  } else {
    ReadMore();
  }
}

void URLLoader::OnReadCompleted(net::URLRequest* url_request, int bytes_read) {
  DCHECK(url_request == url_request_.get());

  DidRead(bytes_read, false);
  // |this| may have been deleted.
}

int URLLoader::OnBeforeStartTransaction(
    const net::HttpRequestHeaders& headers,
    net::NetworkDelegate::OnBeforeStartTransactionCallback callback) {
  if (header_client_) {
    header_client_->OnBeforeSendHeaders(
        headers,
        base::BindOnce(&URLLoader::OnBeforeSendHeadersComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

int URLLoader::OnHeadersReceived(
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    const net::IPEndPoint& endpoint,
    absl::optional<GURL>* preserve_fragment_on_redirect_url) {
  if (header_client_) {
    header_client_->OnHeadersReceived(
        original_response_headers->raw_headers(), endpoint,
        base::BindOnce(&URLLoader::OnHeadersReceivedComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       override_response_headers,
                       preserve_fragment_on_redirect_url));
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

mojom::LoadInfoPtr URLLoader::CreateLoadInfo() {
  auto load_info = mojom::LoadInfo::New();
  load_info->timestamp = base::TimeTicks::Now();
  load_info->host = url_request_->url().host();
  auto load_state = url_request_->GetLoadState();
  load_info->load_state = static_cast<uint32_t>(load_state.state);
  load_info->state_param = std::move(load_state.param);
  auto upload_progress = url_request_->GetUploadProgress();
  load_info->upload_size = upload_progress.size();
  load_info->upload_position = upload_progress.position();
  return load_info;
}

void URLLoader::OnBeforeURLRequest() {
  if (url_loader_factory_)
    return url_loader_factory_->OnBeforeURLRequest();
}

net::LoadState URLLoader::GetLoadStateForTesting() const {
  return url_request_->GetLoadState().state;
}

int32_t URLLoader::GetProcessId() const {
  return factory_params_->process_id;
}

void URLLoader::SetEnableReportingRawHeaders(bool allow) {
  enable_reporting_raw_headers_ = allow;
}

uint32_t URLLoader::GetResourceType() const {
  return resource_type_;
}

bool URLLoader::AllowCookies(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies) const {
  net::StaticCookiePolicy::Type policy =
      net::StaticCookiePolicy::ALLOW_ALL_COOKIES;
  if (options_ & mojom::kURLLoadOptionBlockAllCookies) {
    policy = net::StaticCookiePolicy::BLOCK_ALL_COOKIES;
  } else if (options_ & mojom::kURLLoadOptionBlockThirdPartyCookies) {
    policy = net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES;
  } else {
    return true;
  }
  return net::StaticCookiePolicy(policy).CanAccessCookies(
             url, site_for_cookies) == net::OK;
}

// static
URLLoader* URLLoader::ForRequest(const net::URLRequest& request) {
  auto* pointer =
      static_cast<UnownedPointer*>(request.GetUserData(kUserDataKey));
  if (!pointer)
    return nullptr;
  return pointer->get();
}

// static
void URLLoader::LogConcerningRequestHeaders(
    const net::HttpRequestHeaders& request_headers,
    bool added_during_redirect) {
  net::HttpRequestHeaders::Iterator it(request_headers);

  bool concerning_header_found = false;

  while (it.GetNext()) {
    for (const auto& header : kConcerningHeaders) {
      if (base::EqualsCaseInsensitiveASCII(header.name, it.name())) {
        concerning_header_found = true;
        if (added_during_redirect) {
          UMA_HISTOGRAM_ENUMERATION(
              "NetworkService.ConcerningRequestHeader.HeaderAddedOnRedirect",
              header.histogram_id);
        } else {
          UMA_HISTOGRAM_ENUMERATION(
              "NetworkService.ConcerningRequestHeader.HeaderPresentOnStart",
              header.histogram_id);
        }
      }
    }
  }

  if (added_during_redirect) {
    UMA_HISTOGRAM_BOOLEAN(
        "NetworkService.ConcerningRequestHeader.AddedOnRedirect",
        concerning_header_found);
  } else {
    UMA_HISTOGRAM_BOOLEAN(
        "NetworkService.ConcerningRequestHeader.PresentOnStart",
        concerning_header_found);
  }
}

void URLLoader::OnAuthCredentials(
    const absl::optional<net::AuthCredentials>& credentials) {
  auth_challenge_responder_receiver_.reset();

  if (!credentials.has_value()) {
    url_request_->CancelAuth();
  } else {
    // CancelAuth will proceed to the body, so cookies only need to be reported
    // here.
    ReportFlaggedResponseCookies();
    url_request_->SetAuth(credentials.value());
  }
}

void URLLoader::ContinueWithCertificate(
    const scoped_refptr<net::X509Certificate>& x509_certificate,
    const std::string& provider_name,
    const std::vector<uint16_t>& algorithm_preferences,
    mojo::PendingRemote<mojom::SSLPrivateKey> ssl_private_key) {
  client_cert_responder_receiver_.reset();
  auto key = base::MakeRefCounted<SSLPrivateKeyInternal>(
      provider_name, algorithm_preferences, std::move(ssl_private_key));
  url_request_->ContinueWithCertificate(std::move(x509_certificate),
                                        std::move(key));
}

void URLLoader::ContinueWithoutCertificate() {
  client_cert_responder_receiver_.reset();
  url_request_->ContinueWithCertificate(nullptr, nullptr);
}

void URLLoader::CancelRequest() {
  client_cert_responder_receiver_.reset();
  url_request_->CancelWithError(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
}

void URLLoader::NotifyCompleted(int error_code) {
  // Ensure sending the final upload progress message here, since
  // OnResponseCompleted can be called without OnResponseStarted on cancellation
  // or error cases.
  if (upload_progress_tracker_) {
    upload_progress_tracker_->OnUploadCompleted();
    upload_progress_tracker_ = nullptr;
  }

  auto* url_loader_network_observer = GetURLLoaderNetworkServiceObserver();
  if (url_loader_network_observer &&
      (url_request_->GetTotalReceivedBytes() > 0 ||
       url_request_->GetTotalSentBytes() > 0)) {
    url_loader_network_observer->OnDataUseUpdate(
        url_request_->traffic_annotation().unique_id_hash_code,
        url_request_->GetTotalReceivedBytes(),
        url_request_->GetTotalSentBytes());
  }

  if (url_loader_client_) {
    if (consumer_handle_.is_valid())
      SendResponseToClient();

    URLLoaderCompletionStatus status;
    status.error_code = error_code;
    if (error_code == net::ERR_QUIC_PROTOCOL_ERROR) {
      net::NetErrorDetails details;
      url_request_->PopulateNetErrorDetails(&details);
      status.extended_error_code = details.quic_connection_error;
    }
    status.exists_in_cache = url_request_->response_info().was_cached;
    status.completion_time = base::TimeTicks::Now();
    status.encoded_data_length = url_request_->GetTotalReceivedBytes();
    status.encoded_body_length = url_request_->GetRawBodyBytes();
    status.decoded_body_length = total_written_bytes_;
    status.proxy_server = url_request_->proxy_server();
    status.resolve_error_info =
        url_request_->response_info().resolve_error_info;
    if (trust_token_status_)
      status.trust_token_operation_status = *trust_token_status_;
    status.cors_error_status = cors_error_status_;

    if ((options_ & mojom::kURLLoadOptionSendSSLInfoForCertificateError) &&
        net::IsCertStatusError(url_request_->ssl_info().cert_status)) {
      status.ssl_info = url_request_->ssl_info();
    }

    url_loader_client_->OnComplete(status);
  }

  DeleteSelf();
}

void URLLoader::OnMojoDisconnect() {
  NotifyCompleted(net::ERR_FAILED);
}

void URLLoader::OnResponseBodyStreamConsumerClosed(MojoResult result) {
  NotifyCompleted(net::ERR_FAILED);
}

void URLLoader::OnResponseBodyStreamReady(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    NotifyCompleted(net::ERR_FAILED);
    return;
  }

  ReadMore();
}

void URLLoader::DeleteSelf() {
  std::move(delete_callback_).Run(this);
}

void URLLoader::SendResponseToClient() {
  TRACE_EVENT1("loading", "network::URLLoader::SendResponseToClient", "url",
               url_request_->url().possibly_invalid_spec());
  url_loader_client_->OnReceiveResponse(std::move(response_));
  url_loader_client_->OnStartLoadingResponseBody(std::move(consumer_handle_));
}

void URLLoader::CompletePendingWrite(bool success) {
  if (success) {
    // The write can only be completed immediately in case of a success, since
    // doing so invalidates memory of any attached NetToMojoIOBuffer's; but in
    // case of an abort, particularly one caused by a suspend, the failure may
    // be delivered to URLLoader while the disk_cache layer is still hanging on
    // to the now-invalid IOBuffer in some worker thread trying to commit it to
    // disk.  In case of an error, this will have to wait till everything is
    // destroyed.
    response_body_stream_ =
        pending_write_->Complete(pending_write_buffer_offset_);
  }
  total_written_bytes_ += pending_write_buffer_offset_;
  pending_write_ = nullptr;
  pending_write_buffer_offset_ = 0;
}

void URLLoader::SetRawResponseHeaders(
    scoped_refptr<const net::HttpResponseHeaders> headers) {
  raw_response_headers_ = headers;
}

void URLLoader::NotifyEarlyResponse(
    scoped_refptr<const net::HttpResponseHeaders> headers) {
  DCHECK(!has_received_response_);
  DCHECK(url_loader_client_);
  DCHECK(headers);
  DCHECK_EQ(headers->response_code(), 103);

  // Calculate IP address space.
  mojom::ParsedHeadersPtr parsed_headers =
      PopulateParsedHeaders(headers.get(), url_request_->url());
  std::vector<GURL> url_list_via_service_worker;
  net::IPEndPoint transaction_endpoint;
  bool has_endpoint =
      url_request_->GetTransactionRemoteEndpoint(&transaction_endpoint);
  DCHECK(has_endpoint);
  CalculateClientAddressSpaceParams params(
      url_list_via_service_worker, parsed_headers, transaction_endpoint);
  mojom::IPAddressSpace ip_address_space =
      CalculateClientAddressSpace(url_request_->url(), params);

  // Populate origin trial tokens.
  std::vector<std::string> origin_trial_tokens;
  size_t iter = 0;
  std::string value;
  while (headers->EnumerateHeader(&iter, "Origin-Trial", &value)) {
    origin_trial_tokens.push_back(value);
  }

  url_loader_client_->OnReceiveEarlyHints(
      mojom::EarlyHints::New(std::move(parsed_headers), ip_address_space,
                             std::move(origin_trial_tokens)));
}

void URLLoader::SetRawRequestHeadersAndNotify(
    net::HttpRawRequestHeaders headers) {
  auto* devtools_observer = GetDevToolsObserver();
  if (devtools_observer && devtools_request_id()) {
    std::vector<network::mojom::HttpRawHeaderPairPtr> header_array;
    header_array.reserve(headers.headers().size());

    for (const auto& header : headers.headers()) {
      network::mojom::HttpRawHeaderPairPtr pair =
          network::mojom::HttpRawHeaderPair::New();
      pair->key = header.first;
      pair->value = header.second;
      header_array.push_back(std::move(pair));
    }

    mojom::ClientSecurityStatePtr client_security_state;
    if (factory_params_->client_security_state) {
      client_security_state = factory_params_->client_security_state->Clone();
    } else if (request_client_security_state_) {
      client_security_state = request_client_security_state_->Clone();
    }

    devtools_observer->OnRawRequest(
        devtools_request_id().value(), url_request_->maybe_sent_cookies(),
        std::move(header_array), std::move(client_security_state));
  }

  if (auto* cookie_observer = GetCookieAccessObserver()) {
    std::vector<mojom::CookieOrLineWithAccessResultPtr> reported_cookies;
    for (const auto& cookie_with_access_result :
         url_request_->maybe_sent_cookies()) {
      if (ShouldNotifyAboutCookie(
              cookie_with_access_result.access_result.status)) {
        reported_cookies.push_back(mojom::CookieOrLineWithAccessResult::New(
            mojom::CookieOrLine::NewCookie(cookie_with_access_result.cookie),
            cookie_with_access_result.access_result));
      }
    }

    if (!reported_cookies.empty()) {
      cookie_observer->OnCookiesAccessed(mojom::CookieAccessDetails::New(
          mojom::CookieAccessDetails::Type::kRead, url_request_->url(),
          url_request_->site_for_cookies(), std::move(reported_cookies),
          devtools_request_id()));
    }
  }

  if (devtools_request_id())
    raw_request_headers_.Assign(std::move(headers));
}

void URLLoader::SendUploadProgress(const net::UploadProgress& progress) {
  url_loader_client_->OnUploadProgress(
      progress.position(), progress.size(),
      base::BindOnce(&URLLoader::OnUploadProgressACK,
                     weak_ptr_factory_.GetWeakPtr()));
}

void URLLoader::OnUploadProgressACK() {
  if (upload_progress_tracker_)
    upload_progress_tracker_->OnAckReceived();
}

void URLLoader::OnSSLCertificateErrorResponse(const net::SSLInfo& ssl_info,
                                              int net_error) {
  if (net_error == net::OK) {
    url_request_->ContinueDespiteLastError();
    return;
  }

  url_request_->CancelWithSSLError(net_error, ssl_info);
}

bool URLLoader::HasDataPipe() const {
  return pending_write_ || response_body_stream_.is_valid();
}

void URLLoader::RecordBodyReadFromNetBeforePausedIfNeeded() {
  if (update_body_read_before_paused_)
    body_read_before_paused_ = url_request_->GetRawBodyBytes();
  if (body_read_before_paused_ != -1) {
    if (!url_request_->was_cached()) {
      UMA_HISTOGRAM_COUNTS_1M("Network.URLLoader.BodyReadFromNetBeforePaused",
                              body_read_before_paused_);
    } else {
      DVLOG(1) << "The request has been paused, but "
               << "Network.URLLoader.BodyReadFromNetBeforePaused is not "
               << "reported because the response body may be from cache. "
               << "body_read_before_paused_: " << body_read_before_paused_;
    }
  }
}

void URLLoader::ResumeStart() {
  url_request_->LogUnblocked();
  url_request_->Start();
}

void URLLoader::OnBeforeSendHeadersComplete(
    net::NetworkDelegate::OnBeforeStartTransactionCallback callback,
    int result,
    const absl::optional<net::HttpRequestHeaders>& headers) {
  std::move(callback).Run(result, headers);
}

void URLLoader::OnHeadersReceivedComplete(
    net::CompletionOnceCallback callback,
    scoped_refptr<net::HttpResponseHeaders>* out_headers,
    absl::optional<GURL>* out_preserve_fragment_on_redirect_url,
    int result,
    const absl::optional<std::string>& headers,
    const absl::optional<GURL>& preserve_fragment_on_redirect_url) {
  if (headers) {
    *out_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(headers.value());
  }
  *out_preserve_fragment_on_redirect_url = preserve_fragment_on_redirect_url;
  std::move(callback).Run(result);
}

void URLLoader::CompleteBlockedResponse(
    int error_code,
    bool should_report_corb_blocking,
    absl::optional<mojom::BlockedByResponseReason> reason) {
  if (has_received_response_) {
    // The response headers and body shouldn't yet be sent to the
    // URLLoaderClient.
    DCHECK(response_);
    DCHECK(consumer_handle_.is_valid());
  }

  // Tell the URLLoaderClient that the response has been completed.
  URLLoaderCompletionStatus status;
  status.error_code = error_code;
  status.completion_time = base::TimeTicks::Now();
  status.encoded_data_length = 0;
  status.encoded_body_length = 0;
  status.decoded_body_length = 0;
  status.should_report_corb_blocking = should_report_corb_blocking;
  status.blocked_by_response_reason = reason;
  url_loader_client_->OnComplete(status);

  // Reset the connection to the URLLoaderClient.  This helps ensure that we
  // won't accidentally leak any data to the renderer from this point on.
  url_loader_client_.reset();
}

URLLoader::BlockResponseForCorbResult URLLoader::BlockResponseForCorb() {
  // CORB should only do work after the response headers have been received.
  DCHECK(has_received_response_);

  // The response headers and body shouldn't yet be sent to the URLLoaderClient.
  DCHECK(response_);
  DCHECK(consumer_handle_.is_valid());

  // Send stripped headers to the real URLLoaderClient.
  CrossOriginReadBlocking::SanitizeBlockedResponse(response_.get());
  url_loader_client_->OnReceiveResponse(response_->Clone());

  // Send empty body to the real URLLoaderClient.
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  CHECK_EQ(mojo::CreateDataPipe(kBlockedBodyAllocationSize, producer_handle,
                                consumer_handle),
           MOJO_RESULT_OK);
  producer_handle.reset();
  url_loader_client_->OnStartLoadingResponseBody(std::move(consumer_handle));

  // Tell the real URLLoaderClient that the response has been completed.
  bool should_report_corb_blocking =
      corb_analyzer_->ShouldReportBlockedResponse();
  if (corb_detachable_) {
    // TODO(lukasza): https://crbug.com/827633#c5: Consider passing net::ERR_OK
    // instead.  net::ERR_ABORTED was chosen for consistency with the old CORB
    // implementation that used to go through DetachableResourceHandler.
    CompleteBlockedResponse(net::ERR_ABORTED, should_report_corb_blocking);
  } else {
    // CORB responses are reported as a success.
    CompleteBlockedResponse(net::OK, should_report_corb_blocking);
  }

  // If the factory is asking to complete requests of this type, then we need to
  // continue processing the response to make sure the network cache is
  // populated.  Otherwise we can cancel the request.
  if (corb_detachable_) {
    // Discard any remaining callbacks or data by rerouting the pipes to
    // EmptyURLLoaderClient.
    receiver_.reset();
    EmptyURLLoaderClientWrapper::DrainURLRequest(
        url_loader_client_.BindNewPipeAndPassReceiver(),
        receiver_.BindNewPipeAndPassRemote());
    receiver_.set_disconnect_handler(
        base::BindOnce(&URLLoader::OnMojoDisconnect, base::Unretained(this)));

    // Ask the caller to continue processing the request.
    return kContinueRequest;
  }

  // Close the socket associated with the request, to prevent leaking
  // information.
  url_request_->AbortAndCloseConnection();

  // Delete self and cancel the request - the caller doesn't need to continue.
  //
  // DeleteSelf is posted asynchronously, to make sure that the callers (e.g.
  // URLLoader::OnResponseStarted and/or URLLoader::DidRead instance methods)
  // can still safely dereference |this|.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&URLLoader::DeleteSelf, weak_ptr_factory_.GetWeakPtr()));
  return kWillCancelRequest;
}

void URLLoader::ReportFlaggedResponseCookies() {
  auto* devtools_observer = GetDevToolsObserver();
  if (devtools_observer && devtools_request_id() &&
      url_request_->response_headers()) {
    std::vector<network::mojom::HttpRawHeaderPairPtr> header_array;

    // This is gated by enable_reporting_raw_headers_ to be backwards compatible
    // with the old report_raw_headers behavior, where we wouldn't even send
    // raw_response_headers_ to the trusted browser process based devtools
    // instrumentation. This is observed in the case of HSTS redirects, where
    // url_request_->response_headers has the HSTS redirect headers, like
    // Non-Authoritative-Reason, but raw_response_headers_ has something else
    // which doesn't include HSTS information. This is tested by
    // DevToolsTest.TestRawHeadersWithRedirectAndHSTS.
    // TODO(crbug.com/1234823): Remove enable_reporting_raw_headers_
    const net::HttpResponseHeaders* response_headers =
        raw_response_headers_ && enable_reporting_raw_headers_
            ? raw_response_headers_.get()
            : url_request_->response_headers();

    size_t iterator = 0;
    std::string name, value;
    while (response_headers->EnumerateHeaderLines(&iterator, &name, &value)) {
      network::mojom::HttpRawHeaderPairPtr pair =
          network::mojom::HttpRawHeaderPair::New();
      pair->key = name;
      pair->value = value;
      header_array.push_back(std::move(pair));
    }

    // Only send the "raw" header text when the headers were actually send in
    // text form (i.e. not QUIC or SPDY)
    absl::optional<std::string> raw_response_headers;

    const net::HttpResponseInfo& response_info = url_request_->response_info();

    if (!response_info.DidUseQuic() && !response_info.was_fetched_via_spdy) {
      raw_response_headers =
          absl::make_optional(net::HttpUtil::ConvertHeadersBackToHTTPResponse(
              response_headers->raw_headers()));
    }

    devtools_observer->OnRawResponse(
        devtools_request_id().value(), url_request_->maybe_stored_cookies(),
        std::move(header_array), raw_response_headers,
        IPEndPointToIPAddressSpace(response_info.remote_endpoint),
        response_headers->response_code());
  }

  if (auto* cookie_observer = GetCookieAccessObserver()) {
    std::vector<mojom::CookieOrLineWithAccessResultPtr> reported_cookies;
    for (const auto& cookie_line_and_access_result :
         url_request_->maybe_stored_cookies()) {
      if (ShouldNotifyAboutCookie(
              cookie_line_and_access_result.access_result.status)) {
        mojom::CookieOrLinePtr cookie_or_line = mojom::CookieOrLine::New();
        if (cookie_line_and_access_result.cookie.has_value()) {
          cookie_or_line->set_cookie(
              cookie_line_and_access_result.cookie.value());
        } else {
          cookie_or_line->set_cookie_string(
              cookie_line_and_access_result.cookie_string);
        }

        reported_cookies.push_back(mojom::CookieOrLineWithAccessResult::New(
            std::move(cookie_or_line),
            cookie_line_and_access_result.access_result));
      }
    }

    if (!reported_cookies.empty()) {
      cookie_observer->OnCookiesAccessed(mojom::CookieAccessDetails::New(
          mojom::CookieAccessDetails::Type::kChange, url_request_->url(),
          url_request_->site_for_cookies(), std::move(reported_cookies),
          devtools_request_id()));
    }
  }
}

void URLLoader::StartReading() {
  if (!is_more_mime_sniffing_needed_ && !is_more_corb_sniffing_needed_) {
    // Treat feed types as text/plain.
    if (response_->mime_type == "application/rss+xml" ||
        response_->mime_type == "application/atom+xml") {
      response_->mime_type.assign("text/plain");
    }
    SendResponseToClient();
  }

  // Start reading...
  ReadMore();
}

void URLLoader::OnOriginPolicyManagerRetrieveDone(
    const OriginPolicy& origin_policy) {
  response_->origin_policy = origin_policy;

  StartReading();
}

bool URLLoader::ShouldForceIgnoreSiteForCookies(
    const ResourceRequest& request) {
  // Ignore site for cookies in requests from an initiator covered by the
  // same-origin-policy exclusions in `origin_access_list_` (typically requests
  // initiated by Chrome Extensions).
  if (request.request_initiator.has_value() &&
      cors::OriginAccessList::AccessState::kAllowed ==
          origin_access_list_.CheckAccessState(
              request.request_initiator.value(), request.url)) {
    return true;
  }

  // Convert `site_for_cookies` into an origin (an opaque origin if
  // `net::SiteForCookies::IsNull()` returns true).
  //
  // Note that `site_for_cookies` is a _site_ rather than an _origin_, but for
  // Chrome Extensions the _site_ and _origin_ of a host are the same extension
  // id.  Thanks to this, for Chrome Extensions, we can pass a _site_ into
  // OriginAccessChecks (which normally expect an _origin_).
  url::Origin site_origin =
      url::Origin::Create(request.site_for_cookies.RepresentativeUrl());

  // If `site_for_cookies` represents an origin that is granted access to the
  // initiator and the target by `origin_access_list_` (typically such
  // `site_for_cookies` represents a Chrome Extension), then we also should
  // force ignoring of site for cookies if the initiator and the target are
  // same-site.
  //
  // Ideally we would walk up the frame tree and check that each ancestor is
  // first-party to the main frame (treating the `origin_access_list_`
  // exceptions as "first-party").  But walking up the tree is not possible in
  // //services/network and so we make do with just checking the direct
  // initiator of the request.
  //
  // We also check same-siteness between the initiator and the requested URL,
  // because setting `force_ignore_site_for_cookies` to true causes Strict
  // cookies to be attached, and having the initiator be same-site to the
  // request URL is a requirement for Strict cookies (see
  // net::cookie_util::ComputeSameSiteContext).
  if (!site_origin.opaque() && request.request_initiator.has_value()) {
    bool site_can_access_target =
        cors::OriginAccessList::AccessState::kAllowed ==
        origin_access_list_.CheckAccessState(site_origin, request.url);
    bool site_can_access_initiator =
        cors::OriginAccessList::AccessState::kAllowed ==
        origin_access_list_.CheckAccessState(
            site_origin, request.request_initiator->GetURL());
    net::SiteForCookies site_of_initiator =
        net::SiteForCookies::FromOrigin(request.request_initiator.value());
    bool are_initiator_and_target_same_site =
        site_of_initiator.IsFirstParty(request.url);
    if (site_can_access_initiator && site_can_access_target &&
        are_initiator_and_target_same_site) {
      return true;
    }
  }

  return false;
}

bool URLLoader::ShouldForceIgnoreTopFramePartyForCookies() const {
  const net::IsolationInfo& isolation_info = url_request_->isolation_info();
  const absl::optional<url::Origin>& top_frame_origin =
      isolation_info.top_frame_origin();

  if (!top_frame_origin || top_frame_origin->opaque())
    return false;

  const absl::optional<std::set<net::SchemefulSite>>& party_context =
      isolation_info.party_context();
  if (!party_context)
    return false;

  // The top frame origin must have access to the request URL.
  if (cors::OriginAccessList::AccessState::kAllowed !=
      origin_access_list_.CheckAccessState(*top_frame_origin,
                                           url_request_->url())) {
    return false;
  }

  // The top frame origin must have access to each site in the party_context.
  return base::ranges::all_of(
      *party_context,
      [this, &top_frame_origin](const net::SchemefulSite& site) {
        return origin_access_list_.CheckAccessState(*top_frame_origin,
                                                    site.GetURL()) ==
               cors::OriginAccessList::AccessState::kAllowed;
      });
}

mojom::DevToolsObserver* URLLoader::GetDevToolsObserver() const {
  if (devtools_observer_)
    return devtools_observer_.get();
  if (url_loader_factory_)
    return url_loader_factory_->GetDevToolsObserver();
  return nullptr;
}

mojom::CookieAccessObserver* URLLoader::GetCookieAccessObserver() const {
  if (cookie_observer_)
    return cookie_observer_.get();
  if (url_loader_factory_)
    return url_loader_factory_->GetCookieAccessObserver();
  return nullptr;
}

mojom::URLLoaderNetworkServiceObserver*
URLLoader::GetURLLoaderNetworkServiceObserver() const {
  if (url_loader_network_observer_)
    return url_loader_network_observer_.get();
  if (url_loader_factory_)
    return url_loader_factory_->GetURLLoaderNetworkServiceObserver();
  return nullptr;
}

void URLLoader::SetRequestCredentials(const GURL& url) {
  bool coep_allow_credentials = CoepAllowCredentials(url);

  bool allow_credentials = ShouldAllowCredentials(request_credentials_mode_) &&
                           coep_allow_credentials;

  bool allow_client_certificates =
      ShouldSendClientCertificates(request_credentials_mode_) &&
      coep_allow_credentials;

  // TODO(https://crbug.com/799935) net::LOAD_DO_NOT_* are in the process of
  // being converted to credentials_mode. Using set_allow_credentials will
  // implicitly override the deprecated LOAD_DO_NOT_SAVE_COOKIE flag. As a
  // result, set_allow_credentials should not be called when not needed, or it
  // would have side effects.
  if (url_request_->allow_credentials() != allow_credentials)
    url_request_->set_allow_credentials(allow_credentials);

  url_request_->set_send_client_certs(allow_client_certificates);

  // Contrary to Firefox or blink's cache, the HTTP cache doesn't distinguish
  // requests including user's credentials from the anonymous ones yet. See
  // https://docs.google.com/document/d/1lvbiy4n-GM5I56Ncw304sgvY5Td32R6KHitjRXvkZ6U
  // As a workaround until a solution is implemented, the cached responses
  // aren't used for those requests.
  if (!coep_allow_credentials) {
    DCHECK(base::FeatureList::IsEnabled(
               features::kCrossOriginEmbedderPolicyCredentialless) ||
           base::FeatureList::IsEnabled(
               features::kCrossOriginEmbedderPolicyCredentiallessOriginTrial));
    url_request_->SetLoadFlags(url_request_->load_flags() |
                               net::LOAD_BYPASS_CACHE);
  }
}

// https://github.com/mikewest/credentiallessness
bool URLLoader::CoepAllowCredentials(const GURL& url) {
  switch (request_mode_) {
    case mojom::RequestMode::kCors:
    case mojom::RequestMode::kCorsWithForcedPreflight:
    case mojom::RequestMode::kNavigate:
    case mojom::RequestMode::kSameOrigin:
      return true;

    case mojom::RequestMode::kNoCors:
      break;
  }

  mojom::CrossOriginEmbedderPolicyValue coep_policy =
      factory_params_->client_security_state
          ? factory_params_->client_security_state->cross_origin_embedder_policy
                .value
          : mojom::CrossOriginEmbedderPolicyValue::kNone;
  if (coep_policy != mojom::CrossOriginEmbedderPolicyValue::kCredentialless)
    return true;
  DCHECK(base::FeatureList::IsEnabled(
             features::kCrossOriginEmbedderPolicyCredentialless) ||
         base::FeatureList::IsEnabled(
             features::kCrossOriginEmbedderPolicyCredentiallessOriginTrial));

  url::Origin request_origin = url::Origin::Create(url);
  url::Origin request_initiator =
      url_request_->initiator().value_or(url::Origin());
  if (request_origin.IsSameOriginWith(request_initiator))
    return true;

  return false;
}

}  // namespace network
