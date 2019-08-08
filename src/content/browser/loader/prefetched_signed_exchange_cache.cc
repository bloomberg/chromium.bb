// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetched_signed_exchange_cache.h"

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "content/browser/loader/cross_origin_read_blocking_checker.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/mojo_blob_reader.h"

namespace content {

namespace {

// The max number of cached entry per one frame. This limit is intended to
// prevent OOM crash of the browser process.
constexpr size_t kMaxEntrySize = 100u;

void UpdateRequestResponseStartTime(
    network::ResourceResponseHead* response_head) {
  const base::TimeTicks now_ticks = base::TimeTicks::Now();
  const base::Time now = base::Time::Now();
  response_head->request_start = now_ticks;
  response_head->response_start = now_ticks;
  response_head->load_timing.request_start_time = now;
  response_head->load_timing.request_start = now_ticks;
}

// A utility subclass of MojoBlobReader::Delegate that calls the passed callback
// in OnComplete().
class MojoBlobReaderDelegate : public storage::MojoBlobReader::Delegate {
 public:
  explicit MojoBlobReaderDelegate(base::OnceCallback<void(net::Error)> callback)
      : callback_(std::move(callback)) {}

 private:
  // storage::MojoBlobReader::Delegate
  RequestSideData DidCalculateSize(uint64_t total_size,
                                   uint64_t content_size) override {
    return DONT_REQUEST_SIDE_DATA;
  }

  void OnComplete(net::Error result, uint64_t total_written_bytes) override {
    std::move(callback_).Run(result);
  }

  base::OnceCallback<void(net::Error)> callback_;
};

// A URLLoader which returns a synthesized redirect response for signed
// exchange's outer URL request.
class RedirectResponseURLLoader : public network::mojom::URLLoader {
 public:
  RedirectResponseURLLoader(const network::ResourceRequest& url_request,
                            const GURL& inner_url,
                            const network::ResourceResponseHead& outer_response,
                            network::mojom::URLLoaderClientPtr client)
      : client_(std::move(client)) {
    network::ResourceResponseHead response_head =
        signed_exchange_utils::CreateRedirectResponseHead(
            outer_response, false /* is_fallback_redirect */);
    response_head.was_in_prefetch_cache = true;
    UpdateRequestResponseStartTime(&response_head);
    client_->OnReceiveRedirect(signed_exchange_utils::CreateRedirectInfo(
                                   inner_url, url_request, outer_response,
                                   false /* is_fallback_redirect */),
                               response_head);
  }
  ~RedirectResponseURLLoader() override {}

 private:
  // network::mojom::URLLoader overrides:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override {
    NOTREACHED();
  }
  void ProceedWithResponse() override { NOTREACHED(); }
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {
    // There is nothing to do, because this class just calls OnReceiveRedirect.
  }
  void PauseReadingBodyFromNet() override {
    // There is nothing to do, because we don't fetch the resource from the
    // network.
  }
  void ResumeReadingBodyFromNet() override {
    // There is nothing to do, because we don't fetch the resource from the
    // network.
  }

  network::mojom::URLLoaderClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(RedirectResponseURLLoader);
};

// A URLLoader which returns the inner response of signed exchange.
class InnerResponseURLLoader : public network::mojom::URLLoader {
 public:
  InnerResponseURLLoader(
      const network::ResourceRequest& request,
      const network::ResourceResponseHead& inner_response,
      const url::Origin& request_initiator_site_lock,
      std::unique_ptr<const storage::BlobDataHandle> blob_data_handle,
      const network::URLLoaderCompletionStatus& completion_status,
      network::mojom::URLLoaderClientPtr client,
      bool is_navigation_request)
      : response_(inner_response),
        blob_data_handle_(std::move(blob_data_handle)),
        completion_status_(completion_status),
        client_(std::move(client)),
        weak_factory_(this) {
    DCHECK(response_.headers);
    DCHECK(request.request_initiator);

    UpdateRequestResponseStartTime(&response_);
    response_.encoded_data_length = 0;
    if (is_navigation_request) {
      client_->OnReceiveResponse(response_);
      // When Network Service is not enabled, we need to wait
      // ProceedWithResponse for navigation request. See
      // https://crbug.com/791049.
      if (base::FeatureList::IsEnabled(network::features::kNetworkService))
        SendResponseBody();
      return;
    }

    if (network::cors::ShouldCheckCors(request.url, request.request_initiator,
                                       request.fetch_request_mode)) {
      const auto error_status = network::cors::CheckAccess(
          request.url, response_.headers->response_code(),
          GetHeaderString(
              response_,
              network::cors::header_names::kAccessControlAllowOrigin),
          GetHeaderString(
              response_,
              network::cors::header_names::kAccessControlAllowCredentials),
          request.fetch_credentials_mode, *request.request_initiator);
      if (error_status) {
        client_->OnComplete(network::URLLoaderCompletionStatus(*error_status));
        return;
      }
    }

    corb_checker_ = std::make_unique<CrossOriginReadBlockingChecker>(
        request, response_, request_initiator_site_lock, *blob_data_handle_,
        base::BindOnce(
            &InnerResponseURLLoader::OnCrossOriginReadBlockingCheckComplete,
            base::Unretained(this)));
  }
  ~InnerResponseURLLoader() override {}

 private:
  static base::Optional<std::string> GetHeaderString(
      const network::ResourceResponseHead& response,
      const std::string& header_name) {
    DCHECK(response.headers);
    std::string header_value;
    if (!response.headers->GetNormalizedHeader(header_name, &header_value))
      return base::nullopt;
    return header_value;
  }

  void OnCrossOriginReadBlockingCheckComplete(
      CrossOriginReadBlockingChecker::Result result) {
    switch (result) {
      case CrossOriginReadBlockingChecker::Result::kAllowed:
        client_->OnReceiveResponse(response_);
        SendResponseBody();
        return;
      case CrossOriginReadBlockingChecker::Result::kNetError:
        client_->OnComplete(
            network::URLLoaderCompletionStatus(corb_checker_->GetNetError()));
        return;
      case CrossOriginReadBlockingChecker::Result::kBlocked_ShouldReport:
        break;
      case CrossOriginReadBlockingChecker::Result::kBlocked_ShouldNotReport:
        break;
    }

    // Send sanitized response.
    network::CrossOriginReadBlocking::SanitizeBlockedResponse(&response_);
    client_->OnReceiveResponse(response_);

    // Send an empty response's body.
    mojo::ScopedDataPipeProducerHandle pipe_producer_handle;
    mojo::ScopedDataPipeConsumerHandle pipe_consumer_handle;
    MojoResult rv = mojo::CreateDataPipe(nullptr, &pipe_producer_handle,
                                         &pipe_consumer_handle);
    if (rv != MOJO_RESULT_OK) {
      client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
      return;
    }
    client_->OnStartLoadingResponseBody(std::move(pipe_consumer_handle));

    // Send a dummy OnComplete message.
    network::URLLoaderCompletionStatus status =
        network::URLLoaderCompletionStatus(net::OK);
    status.should_report_corb_blocking =
        result == CrossOriginReadBlockingChecker::Result::kBlocked_ShouldReport;
    client_->OnComplete(status);
  }

  // network::mojom::URLLoader overrides:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override {
    NOTREACHED();
  }
  void ProceedWithResponse() override {
    DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
    SendResponseBody();
  }
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {
    // There is nothing to do, because there is no prioritization mechanism for
    // reading a blob.
  }
  void PauseReadingBodyFromNet() override {
    // There is nothing to do, because we don't fetch the resource from the
    // network.
  }
  void ResumeReadingBodyFromNet() override {
    // There is nothing to do, because we don't fetch the resource from the
    // network.
  }

  void SendResponseBody() {
    mojo::ScopedDataPipeProducerHandle pipe_producer_handle;
    mojo::ScopedDataPipeConsumerHandle pipe_consumer_handle;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = network::kDataPipeDefaultAllocationSize;
    MojoResult rv = mojo::CreateDataPipe(&options, &pipe_producer_handle,
                                         &pipe_consumer_handle);
    if (rv != MOJO_RESULT_OK) {
      client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
      return;
    }

    storage::MojoBlobReader::Create(
        blob_data_handle_.get(), net::HttpByteRange(),
        std::make_unique<MojoBlobReaderDelegate>(
            base::BindOnce(&InnerResponseURLLoader::BlobReaderComplete,
                           weak_factory_.GetWeakPtr())),
        std::move(pipe_producer_handle));

    client_->OnStartLoadingResponseBody(std::move(pipe_consumer_handle));
  }

  void BlobReaderComplete(net::Error result) {
    network::URLLoaderCompletionStatus status;
    if (result == net::OK) {
      status = completion_status_;
      status.exists_in_cache = true;
      status.completion_time = base::TimeTicks::Now();
      status.encoded_data_length = 0;
    } else {
      status = network::URLLoaderCompletionStatus(status);
    }
    client_->OnComplete(status);
  }

  network::ResourceResponseHead response_;
  std::unique_ptr<const storage::BlobDataHandle> blob_data_handle_;
  const network::URLLoaderCompletionStatus completion_status_;
  network::mojom::URLLoaderClientPtr client_;
  std::unique_ptr<CrossOriginReadBlockingChecker> corb_checker_;

  base::WeakPtrFactory<InnerResponseURLLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InnerResponseURLLoader);
};

// A URLLoaderFactory which handles a signed exchange subresource request from
// renderer process.
class SubresourceSignedExchangeURLLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  SubresourceSignedExchangeURLLoaderFactory(
      network::mojom::URLLoaderFactoryRequest request,
      std::unique_ptr<const PrefetchedSignedExchangeCache::Entry> entry,
      const url::Origin& request_initiator_site_lock)
      : entry_(std::move(entry)),
        request_initiator_site_lock_(request_initiator_site_lock) {
    bindings_.AddBinding(this, std::move(request));
    bindings_.set_connection_error_handler(base::BindRepeating(
        &SubresourceSignedExchangeURLLoaderFactory::OnConnectionError,
        base::Unretained(this)));
  }
  ~SubresourceSignedExchangeURLLoaderFactory() override {}

  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    DCHECK_EQ(request.url, entry_->inner_url());
    mojo::MakeStrongBinding(
        std::make_unique<InnerResponseURLLoader>(
            request, *entry_->inner_response(), request_initiator_site_lock_,
            std::make_unique<const storage::BlobDataHandle>(
                *entry_->blob_data_handle()),
            *entry_->completion_status(), std::move(client),
            false /* is_navigation_request */),
        std::move(loader));
  }
  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  void OnConnectionError() {
    if (!bindings_.empty())
      return;
    delete this;
  }

  std::unique_ptr<const PrefetchedSignedExchangeCache::Entry> entry_;
  const url::Origin request_initiator_site_lock_;
  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceSignedExchangeURLLoaderFactory);
};

// A NavigationLoaderInterceptor which handles a request which matches the
// prefetched signed exchange that has been stored to a
// PrefetchedSignedExchangeCache.
class PrefetchedNavigationLoaderInterceptor
    : public NavigationLoaderInterceptor {
 public:
  PrefetchedNavigationLoaderInterceptor(
      std::unique_ptr<const PrefetchedSignedExchangeCache::Entry> exchange,
      std::vector<PrefetchedSignedExchangeInfo> info_list)
      : exchange_(std::move(exchange)),
        info_list_(std::move(info_list)),
        weak_factory_(this) {}

  ~PrefetchedNavigationLoaderInterceptor() override {}

  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      ResourceContext* resource_context,
      LoaderCallback callback,
      FallbackCallback fallback_callback) override {
    // Currently we just check the URL matching. But we should check the Vary
    // header (eg: HttpVaryData::MatchesRequest()) and Cache-Control header.
    // And also we shuold check the expires parameter of the signed exchange's
    // signature. TODO(crbug.com/935267): Implement these checking logic.
    if (state_ == State::kInitial &&
        tentative_resource_request.url == exchange_->outer_url()) {
      state_ = State::kOuterRequestRequested;
      std::move(callback).Run(base::BindOnce(
          &PrefetchedNavigationLoaderInterceptor::StartRedirectResponse,
          weak_factory_.GetWeakPtr()));
      return;
    }
    if (tentative_resource_request.url == exchange_->inner_url()) {
      DCHECK_EQ(State::kOuterRequestRequested, state_);
      state_ = State::kInnerResponseRequested;
      std::move(callback).Run(base::BindOnce(
          &PrefetchedNavigationLoaderInterceptor::StartInnerResponse,
          weak_factory_.GetWeakPtr()));
      return;
    }
    NOTREACHED();
  }

  base::Optional<SubresourceLoaderParams> MaybeCreateSubresourceLoaderParams()
      override {
    if (state_ != State::kInnerResponseRequested)
      return base::nullopt;

    SubresourceLoaderParams params;
    params.prefetched_signed_exchanges = std::move(info_list_);
    return base::make_optional(std::move(params));
  }

 private:
  enum class State {
    kInitial,
    kOuterRequestRequested,
    kInnerResponseRequested
  };

  void StartRedirectResponse(const network::ResourceRequest& resource_request,
                             network::mojom::URLLoaderRequest request,
                             network::mojom::URLLoaderClientPtr client) {
    mojo::MakeStrongBinding(
        std::make_unique<RedirectResponseURLLoader>(
            resource_request, exchange_->inner_url(),
            *exchange_->outer_response(), std::move(client)),
        std::move(request));
  }

  void StartInnerResponse(const network::ResourceRequest& resource_request,
                          network::mojom::URLLoaderRequest request,
                          network::mojom::URLLoaderClientPtr client) {
    mojo::MakeStrongBinding(
        std::make_unique<InnerResponseURLLoader>(
            resource_request, *exchange_->inner_response(),
            url::Origin::Create(exchange_->inner_url()),
            std::make_unique<const storage::BlobDataHandle>(
                *exchange_->blob_data_handle()),
            *exchange_->completion_status(), std::move(client),
            true /* is_navigation_request */),
        std::move(request));
  }

  State state_ = State::kInitial;
  std::unique_ptr<const PrefetchedSignedExchangeCache::Entry> exchange_;
  std::vector<PrefetchedSignedExchangeInfo> info_list_;

  base::WeakPtrFactory<PrefetchedNavigationLoaderInterceptor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchedNavigationLoaderInterceptor);
};

}  // namespace

PrefetchedSignedExchangeCache::Entry::Entry() = default;
PrefetchedSignedExchangeCache::Entry::~Entry() = default;

void PrefetchedSignedExchangeCache::Entry::SetOuterUrl(const GURL& outer_url) {
  outer_url_ = outer_url;
}
void PrefetchedSignedExchangeCache::Entry::SetOuterResponse(
    std::unique_ptr<const network::ResourceResponseHead> outer_response) {
  outer_response_ = std::move(outer_response);
}
void PrefetchedSignedExchangeCache::Entry::SetHeaderIntegrity(
    std::unique_ptr<const net::SHA256HashValue> header_integrity) {
  header_integrity_ = std::move(header_integrity);
}
void PrefetchedSignedExchangeCache::Entry::SetInnerUrl(const GURL& inner_url) {
  inner_url_ = inner_url;
}
void PrefetchedSignedExchangeCache::Entry::SetInnerResponse(
    std::unique_ptr<const network::ResourceResponseHead> inner_response) {
  inner_response_ = std::move(inner_response);
}
void PrefetchedSignedExchangeCache::Entry::SetCompletionStatus(
    std::unique_ptr<const network::URLLoaderCompletionStatus>
        completion_status) {
  completion_status_ = std::move(completion_status);
}
void PrefetchedSignedExchangeCache::Entry::SetBlobDataHandle(
    std::unique_ptr<const storage::BlobDataHandle> blob_data_handle) {
  blob_data_handle_ = std::move(blob_data_handle);
}

std::unique_ptr<const PrefetchedSignedExchangeCache::Entry>
PrefetchedSignedExchangeCache::Entry::Clone() const {
  DCHECK(outer_url().is_valid());
  DCHECK(outer_response());
  DCHECK(header_integrity());
  DCHECK(inner_url().is_valid());
  DCHECK(inner_response());
  DCHECK(completion_status());
  DCHECK(blob_data_handle());

  std::unique_ptr<Entry> clone = std::make_unique<Entry>();
  clone->SetOuterUrl(outer_url_);
  clone->SetOuterResponse(
      std::make_unique<const network::ResourceResponseHead>(*outer_response_));
  clone->SetHeaderIntegrity(
      std::make_unique<const net::SHA256HashValue>(*header_integrity_));
  clone->SetInnerUrl(inner_url_);
  clone->SetInnerResponse(
      std::make_unique<const network::ResourceResponseHead>(*inner_response_));
  clone->SetCompletionStatus(
      std::make_unique<const network::URLLoaderCompletionStatus>(
          *completion_status_));
  clone->SetBlobDataHandle(
      std::make_unique<const storage::BlobDataHandle>(*blob_data_handle_));
  return clone;
}

PrefetchedSignedExchangeCache::PrefetchedSignedExchangeCache() {
  DCHECK(base::FeatureList::IsEnabled(
      features::kSignedExchangeSubresourcePrefetch));
}

PrefetchedSignedExchangeCache::~PrefetchedSignedExchangeCache() {}

void PrefetchedSignedExchangeCache::Store(
    std::unique_ptr<const Entry> cached_exchange) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (exchanges_.size() > kMaxEntrySize)
    return;
  DCHECK(cached_exchange->outer_url().is_valid());
  DCHECK(cached_exchange->outer_response());
  DCHECK(cached_exchange->header_integrity());
  DCHECK(cached_exchange->inner_url().is_valid());
  DCHECK(cached_exchange->inner_response());
  DCHECK(cached_exchange->completion_status());
  DCHECK(cached_exchange->blob_data_handle());
  const GURL outer_url = cached_exchange->outer_url();
  exchanges_[outer_url] = std::move(cached_exchange);
}

std::unique_ptr<NavigationLoaderInterceptor>
PrefetchedSignedExchangeCache::MaybeCreateInterceptor(const GURL& outer_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const auto it = exchanges_.find(outer_url);
  if (it == exchanges_.end())
    return nullptr;
  return std::make_unique<PrefetchedNavigationLoaderInterceptor>(
      it->second->Clone(),
      GetInfoListForNavigation(outer_url, it->second->inner_url()));
}

std::vector<PrefetchedSignedExchangeInfo>
PrefetchedSignedExchangeCache::GetInfoListForNavigation(
    const GURL& outer_url,
    const GURL& inner_url) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::vector<PrefetchedSignedExchangeInfo> info_list;
  const url::Origin outer_url_origin = url::Origin::Create(outer_url);
  const url::Origin inner_url_origin = url::Origin::Create(inner_url);

  for (const auto& exchanges_it : exchanges_) {
    const std::unique_ptr<const Entry>& exchange = exchanges_it.second;
    if (!outer_url_origin.IsSameOriginWith(
            url::Origin::Create(exchange->outer_url()))) {
      // Restrict the main SXG and the subresources SXGs to be served from the
      // same origin.
      continue;
    }
    network::mojom::URLLoaderFactoryPtrInfo loader_factory_info;
    new SubresourceSignedExchangeURLLoaderFactory(
        mojo::MakeRequest(&loader_factory_info), exchange->Clone(),
        inner_url_origin);
    info_list.emplace_back(
        exchange->outer_url(), *exchange->header_integrity(),
        exchange->inner_url(), *exchange->inner_response(),
        std::move(loader_factory_info).PassHandle().release());
  }
  return info_list;
}

}  // namespace content
