// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_subresource_loader.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "content/common/fetch/fetch_request_type_converters.h"
#include "content/common/service_worker/service_worker_loader_helpers.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_features.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_type_converters.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/service_worker/dispatch_fetch_event_params.mojom.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_string.h"
#include "ui/base/page_transition_types.h"

namespace content {

namespace {

constexpr char kServiceWorkerSubresourceLoaderScope[] =
    "ServiceWorkerSubresourceLoader";

network::mojom::URLResponseHeadPtr RewriteServiceWorkerTime(
    base::TimeTicks service_worker_start_time,
    base::TimeTicks service_worker_ready_time,
    network::mojom::URLResponseHeadPtr response_head) {
  response_head->load_timing.service_worker_start_time =
      service_worker_start_time;
  response_head->load_timing.service_worker_ready_time =
      service_worker_ready_time;
  return response_head;
}

// A wrapper URLLoaderClient that invokes the given RewriteHeaderCallback
// whenever a response or redirect is received.
class HeaderRewritingURLLoaderClient : public network::mojom::URLLoaderClient {
 public:
  using RewriteHeaderCallback =
      base::RepeatingCallback<network::mojom::URLResponseHeadPtr(
          network::mojom::URLResponseHeadPtr)>;

  HeaderRewritingURLLoaderClient(
      mojo::Remote<network::mojom::URLLoaderClient> url_loader_client,
      RewriteHeaderCallback rewrite_header_callback)
      : url_loader_client_(std::move(url_loader_client)),
        rewrite_header_callback_(rewrite_header_callback) {}
  ~HeaderRewritingURLLoaderClient() override {}

 private:
  // network::mojom::URLLoaderClient implementation:
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnReceiveResponse(
        rewrite_header_callback_.Run(std::move(response_head)));
  }

  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnReceiveRedirect(
        redirect_info, rewrite_header_callback_.Run(std::move(response_head)));
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnUploadProgress(current_position, total_size,
                                         std::move(ack_callback));
  }

  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnReceiveCachedMetadata(std::move(data));
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnTransferSizeUpdated(transfer_size_diff);
  }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnStartLoadingResponseBody(std::move(body));
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnComplete(status);
  }

  mojo::Remote<network::mojom::URLLoaderClient> url_loader_client_;
  RewriteHeaderCallback rewrite_header_callback_;
};
}  // namespace

// A ServiceWorkerStreamCallback implementation which waits for completion of
// a stream response for subresource loading. It calls
// ServiceWorkerSubresourceLoader::CommitCompleted() upon completion of the
// response.
class ServiceWorkerSubresourceLoader::StreamWaiter
    : public blink::mojom::ServiceWorkerStreamCallback {
 public:
  StreamWaiter(
      ServiceWorkerSubresourceLoader* owner,
      mojo::PendingReceiver<blink::mojom::ServiceWorkerStreamCallback> receiver)
      : owner_(owner), receiver_(this, std::move(receiver)) {
    DCHECK(owner_);
    receiver_.set_disconnect_handler(
        base::BindOnce(&StreamWaiter::OnAborted, base::Unretained(this)));
  }

  // mojom::ServiceWorkerStreamCallback implementations:
  void OnCompleted() override { owner_->OnBodyReadingComplete(net::OK); }
  void OnAborted() override { owner_->OnBodyReadingComplete(net::ERR_ABORTED); }

 private:
  ServiceWorkerSubresourceLoader* owner_;
  mojo::Receiver<blink::mojom::ServiceWorkerStreamCallback> receiver_;

  DISALLOW_COPY_AND_ASSIGN(StreamWaiter);
};

// ServiceWorkerSubresourceLoader -------------------------------------------

ServiceWorkerSubresourceLoader::ServiceWorkerSubresourceLoader(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<ControllerServiceWorkerConnector> controller_connector,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_factory,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::WeakPtr<ServiceWorkerSubresourceLoaderFactory>
        service_worker_subresource_loader_factory)
    : redirect_limit_(net::URLRequest::kMaxRedirects),
      url_loader_client_(std::move(client)),
      url_loader_receiver_(this, std::move(receiver)),
      body_as_blob_size_(blink::BlobUtils::kUnknownSize),
      controller_connector_(std::move(controller_connector)),
      fetch_request_restarted_(false),
      body_reading_complete_(false),
      side_data_reading_complete_(false),
      routing_id_(routing_id),
      request_id_(request_id),
      options_(options),
      traffic_annotation_(traffic_annotation),
      resource_request_(resource_request),
      fallback_factory_(std::move(fallback_factory)),
      task_runner_(std::move(task_runner)),
      service_worker_subresource_loader_factory_(
          std::move(service_worker_subresource_loader_factory)),
      response_source_(network::mojom::FetchResponseSource::kUnspecified) {
  DCHECK(controller_connector_);
  response_head_->request_start = base::TimeTicks::Now();
  response_head_->load_timing.request_start = base::TimeTicks::Now();
  response_head_->load_timing.request_start_time = base::Time::Now();
  // base::Unretained() is safe since |url_loader_receiver_| is owned by |this|.
  url_loader_receiver_.set_disconnect_handler(
      base::BindOnce(&ServiceWorkerSubresourceLoader::OnMojoDisconnect,
                     base::Unretained(this)));
  StartRequest(resource_request);
}

ServiceWorkerSubresourceLoader::~ServiceWorkerSubresourceLoader() = default;

void ServiceWorkerSubresourceLoader::OnMojoDisconnect() {
  delete this;
}

void ServiceWorkerSubresourceLoader::StartRequest(
    const network::ResourceRequest& resource_request) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerSubresourceLoader::StartRequest",
      TRACE_ID_WITH_SCOPE(kServiceWorkerSubresourceLoaderScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_OUT, "url", resource_request.url.spec());
  TransitionToStatus(Status::kStarted);

  DCHECK(!controller_connector_observer_.IsObservingSources());
  controller_connector_observer_.Add(controller_connector_.get());
  fetch_request_restarted_ = false;

  // |service_worker_start_time| becomes web-exposed
  // PerformanceResourceTiming#workerStart, which is the time before starting
  // the worker or just before firing a fetch event. The idea is (fetchStart -
  // workerStart) is the time taken to start service worker. In our case, we
  // don't really know if the worker is started or not yet, but here is a good
  // time to set workerStart, since it will either started soon or the fetch
  // event will be dispatched soon.
  // https://w3c.github.io/resource-timing/#dom-performanceresourcetiming-workerstart
  response_head_->load_timing.service_worker_start_time =
      base::TimeTicks::Now();
  DispatchFetchEvent();
}

void ServiceWorkerSubresourceLoader::DispatchFetchEvent() {
  blink::mojom::ControllerServiceWorker* controller =
      controller_connector_->GetControllerServiceWorker(
          blink::mojom::ControllerServiceWorkerPurpose::FETCH_SUB_RESOURCE);

  response_head_->load_timing.send_start = base::TimeTicks::Now();
  response_head_->load_timing.send_end = base::TimeTicks::Now();

  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerSubresourceLoader::DispatchFetchEvent",
               "controller", (controller ? "exists" : "does not exist"));
  if (!controller) {
    auto controller_state = controller_connector_->state();
    if (controller_state ==
        ControllerServiceWorkerConnector::State::kNoController) {
      // The controller was lost after this loader or its loader factory was
      // created.
      fallback_factory_->CreateLoaderAndStart(
          url_loader_receiver_.Unbind(), routing_id_, request_id_, options_,
          resource_request_, url_loader_client_.Unbind(), traffic_annotation_);
      delete this;
      return;
    }

    // When kNoContainerHost, the network request will be aborted soon since the
    // network provider has already been discarded. In that case, we don't need
    // to return an error as the client must be shutting down.
    DCHECK_EQ(ControllerServiceWorkerConnector::State::kNoContainerHost,
              controller_state);
    SettleFetchEventDispatch(base::nullopt);
    return;
  }

  // Enable the service worker to access the files to be uploaded before
  // dispatching a fetch event.
  if (resource_request_.request_body) {
    const auto& files = resource_request_.request_body->GetReferencedFiles();
    if (!files.empty()) {
      controller_connector_->EnsureFileAccess(
          files,
          base::BindOnce(
              &ServiceWorkerSubresourceLoader::DispatchFetchEventForSubresource,
              weak_factory_.GetWeakPtr()));
      return;
    }
  }

  DispatchFetchEventForSubresource();
}

void ServiceWorkerSubresourceLoader::DispatchFetchEventForSubresource() {
  mojo::PendingRemote<blink::mojom::ServiceWorkerFetchResponseCallback>
      response_callback;
  response_callback_receiver_.Bind(
      response_callback.InitWithNewPipeAndPassReceiver());

  blink::mojom::ControllerServiceWorker* controller =
      controller_connector_->GetControllerServiceWorker(
          blink::mojom::ControllerServiceWorkerPurpose::FETCH_SUB_RESOURCE);

  if (!controller) {
    SettleFetchEventDispatch(base::nullopt);
    return;
  }

  auto params = blink::mojom::DispatchFetchEventParams::New();
  params->request = blink::mojom::FetchAPIRequest::From(resource_request_);
  params->client_id = controller_connector_->client_id();

  if (service_worker_subresource_loader_factory_) {
    service_worker_subresource_loader_factory_->AddPendingWorkerTimingReceiver(
        request_id_,
        params->worker_timing_remote.InitWithNewPipeAndPassReceiver());
  }

  // TODO(falken): Grant the controller service worker's process access to files
  // in the body, like ServiceWorkerFetchDispatcher::DispatchFetchEvent() does.
  controller->DispatchFetchEventForSubresource(
      std::move(params), std::move(response_callback),
      base::BindOnce(&ServiceWorkerSubresourceLoader::OnFetchEventFinished,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerSubresourceLoader::OnFetchEventFinished(
    blink::mojom::ServiceWorkerEventStatus status) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerSubresourceLoader::OnFetchEventFinished",
      TRACE_ID_WITH_SCOPE(kServiceWorkerSubresourceLoaderScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));

  // Stop restarting logic here since OnFetchEventFinished() indicates that the
  // fetch event dispatch reached the renderer.
  SettleFetchEventDispatch(
      mojo::ConvertTo<blink::ServiceWorkerStatusCode>(status));

  switch (status) {
    case blink::mojom::ServiceWorkerEventStatus::COMPLETED:
      // ServiceWorkerFetchResponseCallback interface (OnResponse*() or
      // OnFallback() below) is expected to be called normally and handle this
      // request.
      break;
    case blink::mojom::ServiceWorkerEventStatus::REJECTED:
      // OnResponse() is expected to called with an error about the rejected
      // promise, and handle this request.
      break;
    case blink::mojom::ServiceWorkerEventStatus::ABORTED:
    case blink::mojom::ServiceWorkerEventStatus::TIMEOUT:
      // Fetch event dispatch did not complete, possibly due to timeout of
      // respondWith() or waitUntil(). Return network error.

      // TODO(falken): This seems racy. respondWith() may have been called
      // already and we could have an outstanding stream or blob in progress,
      // and we might hit CommitCompleted() twice once that settles.
      CommitCompleted(net::ERR_FAILED);
  }
}

void ServiceWorkerSubresourceLoader::OnConnectionClosed() {
  response_callback_receiver_.reset();

  // If the connection to the service worker gets disconnected after dispatching
  // a fetch event and before getting the response of the fetch event, restart
  // the fetch event again. If it has already been restarted, that means
  // starting worker failed. In that case, abort the request.
  if (fetch_request_restarted_) {
    SettleFetchEventDispatch(
        blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed);
    CommitCompleted(net::ERR_FAILED);
    return;
  }
  fetch_request_restarted_ = true;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerSubresourceLoader::DispatchFetchEvent,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerSubresourceLoader::SettleFetchEventDispatch(
    base::Optional<blink::ServiceWorkerStatusCode> status) {
  if (!controller_connector_observer_.IsObservingSources()) {
    // Already settled.
    return;
  }
  controller_connector_observer_.RemoveAll();

  if (status) {
    blink::ServiceWorkerStatusCode value = status.value();
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.FetchEvent.Subresource.Status",
                              value);
  }
}

void ServiceWorkerSubresourceLoader::OnResponse(
    blink::mojom::FetchAPIResponsePtr response,
    blink::mojom::ServiceWorkerFetchEventTimingPtr timing) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerSubresourceLoader::OnResponse",
      TRACE_ID_WITH_SCOPE(kServiceWorkerSubresourceLoaderScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  SettleFetchEventDispatch(blink::ServiceWorkerStatusCode::kOk);
  UpdateResponseTiming(std::move(timing));
  StartResponse(std::move(response), nullptr /* body_as_stream */);
}

void ServiceWorkerSubresourceLoader::OnResponseStream(
    blink::mojom::FetchAPIResponsePtr response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    blink::mojom::ServiceWorkerFetchEventTimingPtr timing) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerSubresourceLoader::OnResponseStream",
      TRACE_ID_WITH_SCOPE(kServiceWorkerSubresourceLoaderScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  SettleFetchEventDispatch(blink::ServiceWorkerStatusCode::kOk);
  UpdateResponseTiming(std::move(timing));
  StartResponse(std::move(response), std::move(body_as_stream));
}

void ServiceWorkerSubresourceLoader::OnFallback(
    blink::mojom::ServiceWorkerFetchEventTimingPtr timing) {
  SettleFetchEventDispatch(blink::ServiceWorkerStatusCode::kOk);
  UpdateResponseTiming(std::move(timing));
  // When the request mode is CORS or CORS-with-forced-preflight and the origin
  // of the request URL is different from the security origin of the document,
  // we can't simply fallback to the network here. It is because the CORS
  // preflight logic is implemented in Blink. So we return a "fallback required"
  // response to Blink.
  // TODO(falken): Remove this mechanism after OOB-CORS ships.
  if ((base::CommandLine::ForCurrentProcess()->HasSwitch(
           network::switches::kForceToDisableOutOfBlinkCors) ||
       !base::FeatureList::IsEnabled(network::features::kOutOfBlinkCors)) &&
      ((resource_request_.mode == network::mojom::RequestMode::kCors ||
        resource_request_.mode ==
            network::mojom::RequestMode::kCorsWithForcedPreflight) &&
       (!resource_request_.request_initiator.has_value() ||
        !resource_request_.request_initiator->IsSameOriginWith(
            url::Origin::Create(resource_request_.url))))) {
    TRACE_EVENT_WITH_FLOW0(
        "ServiceWorker",
        "ServiceWorkerSubresourceLoader::OnFallback - CORS workaround",
        TRACE_ID_WITH_SCOPE(kServiceWorkerSubresourceLoaderScope,
                            TRACE_ID_LOCAL(request_id_)),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
    //  Add "Service Worker Fallback Required" which DevTools knows means to not
    //  show the response in the Network tab as it's just an internal
    //  implementation mechanism.
    response_head_->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        "HTTP/1.1 400 Service Worker Fallback Required");
    response_head_->was_fetched_via_service_worker = true;
    response_head_->was_fallback_required_by_service_worker = true;
    CommitResponseHeaders();
    CommitEmptyResponseAndComplete();
    return;
  }
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerSubresourceLoader::OnFallback",
      TRACE_ID_WITH_SCOPE(kServiceWorkerSubresourceLoaderScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN);

  // Hand over to the network loader.
  mojo::PendingRemote<network::mojom::URLLoaderClient> client;
  auto client_impl = std::make_unique<HeaderRewritingURLLoaderClient>(
      std::move(url_loader_client_),
      base::BindRepeating(
          &RewriteServiceWorkerTime,
          response_head_->load_timing.service_worker_start_time,
          response_head_->load_timing.service_worker_ready_time));
  mojo::MakeSelfOwnedReceiver(std::move(client_impl),
                              client.InitWithNewPipeAndPassReceiver());

  fallback_factory_->CreateLoaderAndStart(
      url_loader_receiver_.Unbind(), routing_id_, request_id_, options_,
      resource_request_, std::move(client), traffic_annotation_);

  // Per spec, redirects after this point are not intercepted by the service
  // worker again (https://crbug.com/517364). So this loader is done.
  //
  // It's OK to destruct this loader here. This loader may be the only one who
  // has a ref to fallback_factory_ but in that case the web context that made
  // the request is dead so the request is moot.
  RecordTimingMetrics(false /* handled */);
  delete this;
}

void ServiceWorkerSubresourceLoader::UpdateResponseTiming(
    blink::mojom::ServiceWorkerFetchEventTimingPtr timing) {
  // |service_worker_ready_time| becomes web-exposed
  // PerformanceResourceTiming#fetchStart, which is the time just before
  // dispatching the fetch event, so set it to |dispatch_event_time|.
  response_head_->load_timing.service_worker_ready_time =
      timing->dispatch_event_time;
  fetch_event_timing_ = std::move(timing);
}

void ServiceWorkerSubresourceLoader::StartResponse(
    blink::mojom::FetchAPIResponsePtr response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream) {
  // A response with status code 0 is Blink telling us to respond with network
  // error.
  if (response->status_code == 0) {
    CommitCompleted(net::ERR_FAILED);
    return;
  }

  ServiceWorkerLoaderHelpers::SaveResponseInfo(*response, response_head_.get());
  ServiceWorkerLoaderHelpers::SaveResponseHeaders(
      response->status_code, response->status_text, response->headers,
      response_head_.get());
  response_head_->response_start = base::TimeTicks::Now();
  response_head_->load_timing.receive_headers_start = base::TimeTicks::Now();
  response_head_->load_timing.receive_headers_end =
      response_head_->load_timing.receive_headers_start;
  response_source_ = response->response_source;

  // Handle a redirect response. ComputeRedirectInfo returns non-null redirect
  // info if the given response is a redirect.
  redirect_info_ = ServiceWorkerLoaderHelpers::ComputeRedirectInfo(
      resource_request_, *response_head_);
  if (redirect_info_) {
    if (redirect_limit_-- == 0) {
      CommitCompleted(net::ERR_TOO_MANY_REDIRECTS);
      return;
    }
    response_head_->encoded_data_length = 0;
    url_loader_client_->OnReceiveRedirect(*redirect_info_,
                                          response_head_.Clone());
    TransitionToStatus(Status::kSentRedirect);
    return;
  }

  // We have a non-redirect response. Send the headers to the client.
  CommitResponseHeaders();

  bool body_stream_is_valid =
      !body_as_stream.is_null() && body_as_stream->stream.is_valid();

  // Handle the case where there is no body content.
  if (!body_stream_is_valid && !response->blob) {
    CommitEmptyResponseAndComplete();
    return;
  }

  mojo::ScopedDataPipeConsumerHandle data_pipe;

  // Handle a stream response body.
  if (body_stream_is_valid) {
    DCHECK(!response->blob);
    if (response->side_data_blob)
      DCHECK(base::FeatureList::IsEnabled(features::kCacheStorageEagerReading));
    DCHECK(url_loader_client_.is_bound());
    stream_waiter_ = std::make_unique<StreamWaiter>(
        this, std::move(body_as_stream->callback_receiver));
    data_pipe = std::move(body_as_stream->stream);
  }

  // Handle a blob response body.
  if (response->blob) {
    DCHECK(!body_as_stream);
    DCHECK(response->blob->blob.is_valid());

    body_as_blob_.Bind(std::move(response->blob->blob));
    body_as_blob_size_ = response->blob->size;

    // Start reading the body blob immediately. This will allow the body to
    // start buffering in the pipe while the side data is read.
    int error = StartBlobReading(&data_pipe);
    if (error != net::OK) {
      CommitCompleted(error);
      return;
    }
  }

  DCHECK(data_pipe.is_valid());

  // Read side data if necessary.  We only do this if both the
  // |side_data_blob| is available to read and the request is destined
  // for a script.
  auto resource_type =
      static_cast<blink::mojom::ResourceType>(resource_request_.resource_type);
  if (response->side_data_blob &&
      resource_type == blink::mojom::ResourceType::kScript) {
    side_data_as_blob_.Bind(std::move(response->side_data_blob->blob));
    side_data_as_blob_->ReadSideData(base::BindOnce(
        &ServiceWorkerSubresourceLoader::OnSideDataReadingComplete,
        weak_factory_.GetWeakPtr(), std::move(data_pipe)));
    return;
  }

  // Otherwise we can immediately complete side data reading so that the
  // entire resource completes when the main body is read.
  OnSideDataReadingComplete(std::move(data_pipe),
                            base::Optional<mojo_base::BigBuffer>());
}

void ServiceWorkerSubresourceLoader::CommitResponseHeaders() {
  TransitionToStatus(Status::kSentHeader);
  DCHECK(url_loader_client_.is_bound());
  // TODO(kinuko): Fill the ssl_info.
  url_loader_client_->OnReceiveResponse(response_head_.Clone());
}

void ServiceWorkerSubresourceLoader::CommitResponseBody(
    mojo::ScopedDataPipeConsumerHandle response_body) {
  TransitionToStatus(Status::kSentBody);
  url_loader_client_->OnStartLoadingResponseBody(std::move(response_body));
}

void ServiceWorkerSubresourceLoader::CommitEmptyResponseAndComplete() {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  if (CreateDataPipe(nullptr, &producer_handle, &consumer_handle) !=
      MOJO_RESULT_OK) {
    CommitCompleted(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }

  producer_handle.reset();  // The data pipe is empty.
  CommitResponseBody(std::move(consumer_handle));
  CommitCompleted(net::OK);
}

void ServiceWorkerSubresourceLoader::CommitCompleted(int error_code) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerSubresourceLoader::CommitCompleted",
      TRACE_ID_WITH_SCOPE(kServiceWorkerSubresourceLoaderScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN, "error_code", net::ErrorToString(error_code));

  if (error_code == net::OK) {
    bool handled = !response_head_->was_fallback_required_by_service_worker;
    RecordTimingMetrics(handled);
  }

  TransitionToStatus(Status::kCompleted);
  DCHECK(url_loader_client_.is_bound());
  body_as_blob_.reset();
  stream_waiter_.reset();
  network::URLLoaderCompletionStatus status;
  status.error_code = error_code;
  status.completion_time = base::TimeTicks::Now();
  url_loader_client_->OnComplete(status);

  // Invalidate weak pointers to prevent callbacks after commit.  This can
  // occur if an error code is encountered which forces an early commit.
  weak_factory_.InvalidateWeakPtrs();
}

void ServiceWorkerSubresourceLoader::RecordTimingMetrics(bool handled) {
  DCHECK(fetch_event_timing_);

  // |report_raw_headers| is true when DevTools is attached. Don't record
  // metrics when DevTools is attached to reduce noise.
  // TODO(bashi): Relying on |report_raw_header| to detect DevTools existence
  // is brittle. Figure out a better way to check DevTools is attached.
  if (resource_request_.report_raw_headers)
    return;

  // |fetch_event_timing_| can be recorded in different process. We can get
  // reasonable metrics only when TimeTicks are consistent across processes.
  if (!base::TimeTicks::IsHighResolution() ||
      !base::TimeTicks::IsConsistentAcrossProcesses())
    return;

  base::TimeTicks completion_time = base::TimeTicks::Now();

  // Time spent for service worker startup including mojo message delay.
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.LoadTiming.Subresource."
      "ForwardServiceWorkerToWorkerReady",
      response_head_->load_timing.service_worker_ready_time -
          response_head_->load_timing.service_worker_start_time);

  // Time spent by fetch handlers.
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.LoadTiming.Subresource."
      "WorkerReadyToFetchHandlerEnd",
      fetch_event_timing_->respond_with_settled_time -
          response_head_->load_timing.service_worker_ready_time);

  if (handled) {
    // Mojo message delay. If the controller service worker lives in the same
    // process this captures service worker thread -> background thread delay.
    // Otherwise, this captures IPC delay (this renderer process -> other
    // renderer process).
    UMA_HISTOGRAM_TIMES(
        "ServiceWorker.LoadTiming.Subresource."
        "FetchHandlerEndToResponseReceived",
        response_head_->load_timing.receive_headers_end -
            fetch_event_timing_->respond_with_settled_time);

    // Time spent reading response body.
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "ServiceWorker.LoadTiming.Subresource."
        "ResponseReceivedToCompleted2",
        completion_time - response_head_->load_timing.receive_headers_end);
    // Same as above, breakdown by response source.
    base::UmaHistogramMediumTimes(
        base::StrCat({"ServiceWorker.LoadTiming.Subresource."
                      "ResponseReceivedToCompleted2",
                      ServiceWorkerUtils::FetchResponseSourceToSuffix(
                          response_source_)}),
        completion_time - response_head_->load_timing.receive_headers_end);
  } else {
    // Mojo message delay (network fallback case). See above for the detail.
    UMA_HISTOGRAM_TIMES(
        "ServiceWorker.LoadTiming.Subresource."
        "FetchHandlerEndToFallbackNetwork",
        completion_time - fetch_event_timing_->respond_with_settled_time);
  }
}

// ServiceWorkerSubresourceLoader: URLLoader implementation -----------------

void ServiceWorkerSubresourceLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const base::Optional<GURL>& new_url) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerSubresourceLoader::FollowRedirect",
      TRACE_ID_WITH_SCOPE(kServiceWorkerSubresourceLoaderScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "new_url",
      redirect_info_->new_url.spec());
  // TODO(arthursonzogni, juncai): This seems to be correctly implemented, but
  // not used so far. Add tests and remove this DCHECK to support this feature
  // if needed. See https://crbug.com/845683.
  DCHECK(modified_headers.IsEmpty() && modified_cors_exempt_headers.IsEmpty())
      << "Redirect with modified headers is not supported yet. See "
         "https://crbug.com/845683";
  DCHECK(!new_url.has_value()) << "Redirect with modified url was not "
                                  "supported yet. crbug.com/845683";
  DCHECK(redirect_info_);

  bool should_clear_upload = false;
  net::RedirectUtil::UpdateHttpRequest(
      resource_request_.url, resource_request_.method, *redirect_info_,
      removed_headers, modified_headers, &resource_request_.headers,
      &should_clear_upload);
  resource_request_.cors_exempt_headers.MergeFrom(modified_cors_exempt_headers);
  for (const std::string& name : removed_headers)
    resource_request_.cors_exempt_headers.RemoveHeader(name);

  if (should_clear_upload)
    resource_request_.request_body = nullptr;

  resource_request_.url = redirect_info_->new_url;
  resource_request_.method = redirect_info_->new_method;
  resource_request_.site_for_cookies = redirect_info_->new_site_for_cookies;
  resource_request_.referrer = GURL(redirect_info_->new_referrer);
  resource_request_.referrer_policy = redirect_info_->new_referrer_policy;

  // Restart the request.
  TransitionToStatus(Status::kNotStarted);
  redirect_info_.reset();
  response_callback_receiver_.reset();
  StartRequest(resource_request_);
}

void ServiceWorkerSubresourceLoader::SetPriority(net::RequestPriority priority,
                                                 int intra_priority_value) {
  // Not supported (do nothing).
}

void ServiceWorkerSubresourceLoader::PauseReadingBodyFromNet() {}

void ServiceWorkerSubresourceLoader::ResumeReadingBodyFromNet() {}

int ServiceWorkerSubresourceLoader::StartBlobReading(
    mojo::ScopedDataPipeConsumerHandle* body_pipe) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerSubresourceLoader::StartBlobReading",
      TRACE_ID_WITH_SCOPE(kServiceWorkerSubresourceLoaderScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(body_pipe);
  DCHECK(!body_reading_complete_);

  return ServiceWorkerLoaderHelpers::ReadBlobResponseBody(
      &body_as_blob_, body_as_blob_size_,
      base::BindOnce(&ServiceWorkerSubresourceLoader::OnBodyReadingComplete,
                     weak_factory_.GetWeakPtr()),
      body_pipe);
}

void ServiceWorkerSubresourceLoader::OnSideDataReadingComplete(
    mojo::ScopedDataPipeConsumerHandle data_pipe,
    base::Optional<mojo_base::BigBuffer> metadata) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerSubresourceLoader::OnSideDataReadingComplete",
      TRACE_ID_WITH_SCOPE(kServiceWorkerSubresourceLoaderScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "metadata size",
      (metadata ? metadata->size() : 0));
  DCHECK(url_loader_client_);
  DCHECK(!side_data_reading_complete_);
  side_data_reading_complete_ = true;

  if (metadata.has_value())
    url_loader_client_->OnReceiveCachedMetadata(std::move(metadata.value()));

  DCHECK(data_pipe.is_valid());

  base::TimeDelta delay =
      base::TimeTicks::Now() - response_head_->response_start;
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.SubresourceNotifyStartLoadingResponseBodyDelay", delay);

  DCHECK(data_pipe.is_valid());
  CommitResponseBody(std::move(data_pipe));

  // If the blob reading completed before the side data reading, then we
  // must manually finalize the blob reading now.
  if (body_reading_complete_) {
    OnBodyReadingComplete(net::OK);
  }

  // Otherwise we asyncly continue in OnBlobReadingComplete().
}

void ServiceWorkerSubresourceLoader::OnBodyReadingComplete(int net_error) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerSubresourceLoader::OnBodyReadingComplete",
      TRACE_ID_WITH_SCOPE(kServiceWorkerSubresourceLoaderScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  body_reading_complete_ = true;
  // If the side data has not completed reading yet, then we need to delay
  // calling CommitCompleted.  This method will be called again from
  // OnSideDataReadingComplete().  Only delay for successful reads, though.
  // Abort immediately on error.
  if (!side_data_reading_complete_ && net_error == net::OK)
    return;
  CommitCompleted(net_error);
}

// ServiceWorkerSubresourceLoaderFactory ------------------------------------

// static
void ServiceWorkerSubresourceLoaderFactory::Create(
    scoped_refptr<ControllerServiceWorkerConnector> controller_connector,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_factory,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<base::SequencedTaskRunner> parent_task_runner,
    WorkerTimingCallback worker_timing_callback) {
  new ServiceWorkerSubresourceLoaderFactory(
      std::move(controller_connector), std::move(fallback_factory),
      std::move(receiver), std::move(task_runner),
      std::move(parent_task_runner), std::move(worker_timing_callback));
}

ServiceWorkerSubresourceLoaderFactory::ServiceWorkerSubresourceLoaderFactory(
    scoped_refptr<ControllerServiceWorkerConnector> controller_connector,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_factory,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<base::SequencedTaskRunner> parent_task_runner,
    WorkerTimingCallback worker_timing_callback)
    : controller_connector_(std::move(controller_connector)),
      fallback_factory_(std::move(fallback_factory)),
      task_runner_(std::move(task_runner)),
      parent_task_runner_(std::move(parent_task_runner)),
      worker_timing_callback_(std::move(worker_timing_callback)) {
  DCHECK(fallback_factory_);
  receivers_.Add(this, std::move(receiver));
  receivers_.set_disconnect_handler(base::BindRepeating(
      &ServiceWorkerSubresourceLoaderFactory::OnMojoDisconnect,
      base::Unretained(this)));
}

ServiceWorkerSubresourceLoaderFactory::
    ~ServiceWorkerSubresourceLoaderFactory() = default;

void ServiceWorkerSubresourceLoaderFactory::AddPendingWorkerTimingReceiver(
    int request_id,
    mojo::PendingReceiver<blink::mojom::WorkerTimingContainer> receiver) {
  parent_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(worker_timing_callback_, request_id, std::move(receiver)));
}

void ServiceWorkerSubresourceLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  // This loader destructs itself, as we want to transparently switch to the
  // network loader when fallback happens. When that happens the loader unbinds
  // the request, passes the request to the fallback factory, and
  // destructs itself (while the loader client continues to work).
  new ServiceWorkerSubresourceLoader(
      std::move(receiver), routing_id, request_id, options, resource_request,
      std::move(client), traffic_annotation, controller_connector_,
      fallback_factory_, task_runner_, weak_factory_.GetWeakPtr());
}

void ServiceWorkerSubresourceLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ServiceWorkerSubresourceLoaderFactory::OnMojoDisconnect() {
  if (!receivers_.empty())
    return;
  delete this;
}

void ServiceWorkerSubresourceLoader::TransitionToStatus(Status new_status) {
#if DCHECK_IS_ON()
  switch (new_status) {
    case Status::kNotStarted:
      DCHECK_EQ(status_, Status::kSentRedirect);
      break;
    case Status::kStarted:
      DCHECK_EQ(status_, Status::kNotStarted);
      break;
    case Status::kSentRedirect:
      DCHECK_EQ(status_, Status::kStarted);
      break;
    case Status::kSentHeader:
      DCHECK_EQ(status_, Status::kStarted);
      break;
    case Status::kSentBody:
      DCHECK_EQ(status_, Status::kSentHeader);
      break;
    case Status::kCompleted:
      DCHECK(
          // Network fallback before interception.
          status_ == Status::kNotStarted ||
          // Network fallback after interception.
          status_ == Status::kStarted ||
          // Pipe creation failure for empty response.
          status_ == Status::kSentHeader ||
          // Success case or error while sending the response's body.
          status_ == Status::kSentBody);
      break;
  }
#endif  // DCHECK_IS_ON()

  status_ = new_status;
}

}  // namespace content
