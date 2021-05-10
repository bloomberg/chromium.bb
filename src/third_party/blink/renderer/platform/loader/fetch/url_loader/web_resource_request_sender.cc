// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_resource_request_sender.h"

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/loader/inter_process_time_ticks_converter.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_back_forward_cache_loader_helper.h"
#include "third_party/blink/public/platform/web_request_peer.h"
#include "third_party/blink/public/platform/web_resource_request_sender_delegate.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/back_forward_cache_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/back_forward_cache_loader_helper.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/mojo_url_loader_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_response.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace WTF {

template <>
struct CrossThreadCopier<
    blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>>;
  static Type Copy(Type&& value) { return std::move(value); }
};

template <>
struct CrossThreadCopier<net::NetworkTrafficAnnotationTag>
    : public CrossThreadCopierPassThrough<net::NetworkTrafficAnnotationTag> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = net::NetworkTrafficAnnotationTag;
  static const Type& Copy(const Type& traffic_annotation) {
    return traffic_annotation;
  }
};

template <>
struct CrossThreadCopier<blink::WebVector<blink::WebString>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = blink::WebVector<blink::WebString>;
  static Type Copy(const Type& value) {
    Type result;
    result.reserve(value.size());
    for (const auto& element : value)
      result.emplace_back(element.IsolatedCopy());
    return result;
  }
};

}  // namespace WTF

namespace blink {

namespace {

// Converts |time| from a remote to local TimeTicks, overwriting the original
// value.
void RemoteToLocalTimeTicks(const InterProcessTimeTicksConverter& converter,
                            base::TimeTicks* time) {
  RemoteTimeTicks remote_time = RemoteTimeTicks::FromTimeTicks(*time);
  *time = converter.ToLocalTimeTicks(remote_time).ToTimeTicks();
}

void CheckSchemeForReferrerPolicy(const network::ResourceRequest& request) {
  if ((request.referrer_policy ==
           ReferrerUtils::GetDefaultNetReferrerPolicy() ||
       request.referrer_policy ==
           net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE) &&
      request.referrer.SchemeIsCryptographic() &&
      !url::Origin::Create(request.url).opaque() &&
      !network::IsUrlPotentiallyTrustworthy(request.url)) {
    LOG(FATAL) << "Trying to send secure referrer for insecure request "
               << "without an appropriate referrer policy.\n"
               << "URL = " << request.url << "\n"
               << "URL's Origin = "
               << url::Origin::Create(request.url).Serialize() << "\n"
               << "Referrer = " << request.referrer;
  }
}

int GetInitialRequestID() {
  // Starting with a random number speculatively avoids RDH_INVALID_REQUEST_ID
  // which are assumed to have been caused by restarting RequestID at 0 when
  // restarting a renderer after a crash - this would cause collisions if
  // requests from the previously crashed renderer are still active.  See
  // https://crbug.com/614281#c61 for more details about this hypothesis.
  //
  // To avoid increasing the likelihood of overflowing the range of available
  // RequestIDs, kMax is set to a relatively low value of 2^20 (rather than
  // to something higher like 2^31).
  const int kMin = 0;
  const int kMax = 1 << 20;
  return base::RandInt(kMin, kMax);
}

// Determines if the loader should be restarted on a redirect using
// ThrottlingURLLoader::FollowRedirectForcingRestart.
bool RedirectRequiresLoaderRestart(const GURL& original_url,
                                   const GURL& redirect_url) {
  // Restart is needed if the URL is no longer handled by network service.
  if (network_utils::IsURLHandledByNetworkService(original_url))
    return !network_utils::IsURLHandledByNetworkService(redirect_url);

  // If URL wasn't originally handled by network service, restart is needed if
  // schemes are different.
  return original_url.scheme_piece() != redirect_url.scheme_piece();
}

}  // namespace

// static
int WebResourceRequestSender::MakeRequestID() {
  static const int kInitialRequestID = GetInitialRequestID();
  static base::AtomicSequenceNumber sequence;
  return kInitialRequestID + sequence.GetNext();
}

WebResourceRequestSender::WebResourceRequestSender()
    : delegate_(Platform::Current()->GetResourceRequestSenderDelegate()) {}

WebResourceRequestSender::~WebResourceRequestSender() = default;

void WebResourceRequestSender::SendSync(
    std::unique_ptr<network::ResourceRequest> request,
    int routing_id,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    uint32_t loader_options,
    SyncLoadResponse* response,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    WebVector<std::unique_ptr<URLLoaderThrottle>> throttles,
    base::TimeDelta timeout,
    const WebVector<WebString>& cors_exempt_header_list,
    base::WaitableEvent* terminate_sync_load_event,
    mojo::PendingRemote<mojom::BlobRegistry> download_to_blob_registry,
    scoped_refptr<WebRequestPeer> peer,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    WebBackForwardCacheLoaderHelper back_forward_cache_loader_helper) {
  if (IsInflightNetworkRequestBackForwardCacheSupportEnabled()) {
    // Sync fetches are triggered by script, which should not run when a
    // document is in back-forward cache. If we somehow made it here, we should
    // trigger a back-forward cache eviction.
    auto* helper =
        back_forward_cache_loader_helper.GetBackForwardCacheLoaderHelper();
    if (helper) {
      helper->EvictFromBackForwardCache(
          mojom::RendererEvictionReason::kJavaScriptExecution);
    }
  }

  CheckSchemeForReferrerPolicy(*request);

  DCHECK(loader_options & network::mojom::kURLLoadOptionSynchronous);
  DCHECK(request->load_flags & net::LOAD_IGNORE_LIMITS);

  std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory =
      url_loader_factory->Clone();
  base::WaitableEvent redirect_or_response_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Prepare the configured throttles for use on a separate thread.
  for (const auto& throttle : throttles)
    throttle->DetachFromCurrentSequence();

  // A task is posted to a separate thread to execute the request so that
  // this thread may block on a waitable event. It is safe to pass raw
  // pointers to on-stack objects as this stack frame will
  // survive until the request is complete.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner({});
  SyncLoadContext* context_for_redirect = nullptr;
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      WTF::CrossThreadBindOnce(
          &SyncLoadContext::StartAsyncWithWaitableEvent, std::move(request),
          routing_id, task_runner, traffic_annotation, loader_options,
          std::move(pending_factory), std::move(throttles),
          CrossThreadUnretained(response),
          CrossThreadUnretained(&context_for_redirect),
          CrossThreadUnretained(&redirect_or_response_event),
          CrossThreadUnretained(terminate_sync_load_event), timeout,
          std::move(download_to_blob_registry), cors_exempt_header_list,
          std::move(resource_load_info_notifier_wrapper)));

  // redirect_or_response_event will signal when each redirect completes, and
  // when the final response is complete.
  redirect_or_response_event.Wait();

  while (context_for_redirect) {
    DCHECK(response->redirect_info);
    bool follow_redirect = peer->OnReceivedRedirect(
        *response->redirect_info, response->head.Clone(),
        nullptr /* removed_headers */);
    redirect_or_response_event.Reset();
    if (follow_redirect) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(&SyncLoadContext::FollowRedirect,
                                    base::Unretained(context_for_redirect)));
    } else {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(&SyncLoadContext::CancelRedirect,
                                    base::Unretained(context_for_redirect)));
    }
    redirect_or_response_event.Wait();
  }
}

int WebResourceRequestSender::SendAsync(
    std::unique_ptr<network::ResourceRequest> request,
    int routing_id,
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    uint32_t loader_options,
    const WebVector<WebString>& cors_exempt_header_list,
    scoped_refptr<WebRequestPeer> peer,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    WebVector<std::unique_ptr<URLLoaderThrottle>> throttles,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    WebBackForwardCacheLoaderHelper back_forward_cache_loader_helper) {
  auto weak_this = weak_factory_.GetWeakPtr();

  CheckSchemeForReferrerPolicy(*request);

#if defined(OS_ANDROID)
  // Main frame shouldn't come here.
  DCHECK(!(request->is_main_frame &&
           IsRequestDestinationFrame(request->destination)));
  if (request->has_user_gesture) {
    resource_load_info_notifier_wrapper->NotifyUpdateUserGestureCarryoverInfo();
  }
#endif

  // Compute a unique request_id for this renderer process.
  int request_id = MakeRequestID();
  request_info_ = std::make_unique<PendingRequestInfo>(
      std::move(peer), request->destination, request->render_frame_id,
      request->url, std::move(resource_load_info_notifier_wrapper));

  request_info_->resource_load_info_notifier_wrapper
      ->NotifyResourceLoadInitiated(
          request_id, request->url, request->method, request->referrer,
          request_info_->request_destination, request->priority);

  request_info_->previews_state = request->previews_state;

  auto client = std::make_unique<MojoURLLoaderClient>(
      this, loading_task_runner, url_loader_factory->BypassRedirectChecks(),
      request->url, back_forward_cache_loader_helper);

  std::vector<std::string> std_cors_exempt_header_list(
      cors_exempt_header_list.size());
  std::transform(cors_exempt_header_list.begin(), cors_exempt_header_list.end(),
                 std_cors_exempt_header_list.begin(),
                 [](const WebString& h) { return h.Latin1(); });
  std::unique_ptr<ThrottlingURLLoader> url_loader =
      ThrottlingURLLoader::CreateLoaderAndStart(
          std::move(url_loader_factory), throttles.ReleaseVector(), routing_id,
          request_id, loader_options, request.get(), client.get(),
          traffic_annotation, std::move(loading_task_runner),
          base::make_optional(std_cors_exempt_header_list));
  // TODO(https://crbug.com/1175286): Remove this mitigation when we understand
  // why `this` or `request_info_` is being destroyed.
  if (!weak_this || !request_info_)
    return request_id;

  request_info_->url_loader = std::move(url_loader);
  request_info_->url_loader_client = std::move(client);

  return request_id;
}

void WebResourceRequestSender::Cancel(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  // Cancel the request if it didn't complete, and clean it up so the bridge
  // will receive no more messages.
  DeletePendingRequest(std::move(task_runner));
}

void WebResourceRequestSender::SetDefersLoading(WebURLLoader::DeferType value) {
  if (!request_info_) {
    DLOG(ERROR) << "unknown request";
    return;
  }
  if (value != WebURLLoader::DeferType::kNotDeferred) {
    request_info_->is_deferred = value;
    request_info_->url_loader_client->SetDefersLoading(value);
  } else if (request_info_->is_deferred !=
             WebURLLoader::DeferType::kNotDeferred) {
    request_info_->is_deferred = WebURLLoader::DeferType::kNotDeferred;
    request_info_->url_loader_client->SetDefersLoading(
        WebURLLoader::DeferType::kNotDeferred);

    FollowPendingRedirect(request_info_.get());
  }
}

void WebResourceRequestSender::DidChangePriority(
    net::RequestPriority new_priority,
    int intra_priority_value) {
  if (!request_info_) {
    DLOG(ERROR) << "unknown request";
    return;
  }

  request_info_->url_loader->SetPriority(new_priority, intra_priority_value);
}

void WebResourceRequestSender::DeletePendingRequest(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!request_info_)
    return;

  if (request_info_->net_error == net::ERR_IO_PENDING) {
    request_info_->net_error = net::ERR_ABORTED;
    request_info_->resource_load_info_notifier_wrapper
        ->NotifyResourceLoadCanceled(request_info_->net_error);
  }

  // Cancel loading.
  request_info_->url_loader.reset();

  // Clear URLLoaderClient to stop receiving further Mojo IPC from the browser
  // process.
  request_info_->url_loader_client.reset();

  // Always delete the `request_info_` asyncly so that cancelling the request
  // doesn't delete the request context which the `request_info_->peer` points
  // to while its response is still being handled.
  task_runner->DeleteSoon(FROM_HERE, request_info_.release());
}

WebResourceRequestSender::PendingRequestInfo::PendingRequestInfo(
    scoped_refptr<WebRequestPeer> peer,
    network::mojom::RequestDestination request_destination,
    int render_frame_id,
    const GURL& request_url,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper)
    : peer(std::move(peer)),
      request_destination(request_destination),
      render_frame_id(render_frame_id),
      url(request_url),
      response_url(request_url),
      local_request_start(base::TimeTicks::Now()),
      resource_load_info_notifier_wrapper(
          std::move(resource_load_info_notifier_wrapper)) {}

WebResourceRequestSender::PendingRequestInfo::~PendingRequestInfo() = default;

void WebResourceRequestSender::FollowPendingRedirect(
    PendingRequestInfo* request_info) {
  if (request_info->has_pending_redirect &&
      request_info->should_follow_redirect) {
    request_info->has_pending_redirect = false;
    // net::URLRequest clears its request_start on redirect, so should we.
    request_info->local_request_start = base::TimeTicks::Now();
    // Redirect URL may not be handled by the network service, so force a
    // restart in case another URLLoaderFactory should handle the URL.
    if (request_info->redirect_requires_loader_restart) {
      request_info->url_loader->FollowRedirectForcingRestart();
    } else {
      std::vector<std::string> removed_headers(
          request_info_->removed_headers.size());
      std::transform(request_info_->removed_headers.begin(),
                     request_info_->removed_headers.end(),
                     removed_headers.begin(),
                     [](const WebString& h) { return h.Ascii(); });
      request_info->url_loader->FollowRedirect(
          removed_headers, {} /* modified_headers */,
          {} /* modified_cors_exempt_headers */);
    }
  }
}

void WebResourceRequestSender::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  DCHECK_GT(transfer_size_diff, 0);
  if (!request_info_)
    return;

  // TODO(yhirano): Consider using int64_t in
  // WebRequestPeer::OnTransferSizeUpdated.
  request_info_->peer->OnTransferSizeUpdated(transfer_size_diff);
  if (!request_info_)
    return;
  request_info_->resource_load_info_notifier_wrapper
      ->NotifyResourceTransferSizeUpdated(transfer_size_diff);
}

void WebResourceRequestSender::OnUploadProgress(int64_t position,
                                                int64_t size) {
  if (!request_info_)
    return;

  request_info_->peer->OnUploadProgress(position, size);
}

void WebResourceRequestSender::OnReceivedResponse(
    network::mojom::URLResponseHeadPtr response_head) {
  TRACE_EVENT0("loading", "WebResourceRequestSender::OnReceivedResponse");
  if (!request_info_)
    return;
  request_info_->local_response_start = base::TimeTicks::Now();
  request_info_->remote_request_start =
      response_head->load_timing.request_start;
  // Now that response_start has been set, we can properly set the TimeTicks in
  // the URLResponseHead.
  ToLocalURLResponseHead(*request_info_, *response_head);
  request_info_->load_timing_info = response_head->load_timing;
  if (delegate_) {
    scoped_refptr<WebRequestPeer> new_peer = delegate_->OnReceivedResponse(
        std::move(request_info_->peer),
        WebString::FromUTF8(response_head->mime_type),
        KURL(request_info_->url));
    DCHECK(new_peer);
    request_info_->peer = std::move(new_peer);
  }

  request_info_->peer->OnReceivedResponse(response_head.Clone());
  if (!request_info_)
    return;

  request_info_->resource_load_info_notifier_wrapper
      ->NotifyResourceResponseReceived(std::move(response_head),
                                       request_info_->previews_state);
}

void WebResourceRequestSender::OnReceivedCachedMetadata(
    mojo_base::BigBuffer data) {
  if (!request_info_)
    return;

  if (data.size()) {
    request_info_->peer->OnReceivedCachedMetadata(std::move(data));
  }
}

void WebResourceRequestSender::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  TRACE_EVENT0("loading", "WebResourceRequestSender::OnReceivedRedirect");
  if (!request_info_)
    return;
  if (!request_info_->url_loader && request_info_->should_follow_redirect) {
    // This is a redirect that synchronously came as the loader is being
    // constructed, due to a URLLoaderThrottle that changed the starting
    // URL. Handle this in a posted task, as we don't have the loader
    // pointer yet.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&WebResourceRequestSender::OnReceivedRedirect,
                                  weak_factory_.GetWeakPtr(), redirect_info,
                                  std::move(response_head), task_runner));
    return;
  }

  request_info_->local_response_start = base::TimeTicks::Now();
  request_info_->remote_request_start =
      response_head->load_timing.request_start;
  request_info_->redirect_requires_loader_restart =
      RedirectRequiresLoaderRestart(request_info_->response_url,
                                    redirect_info.new_url);

  ToLocalURLResponseHead(*request_info_, *response_head);
  std::vector<std::string> removed_headers;
  if (request_info_->peer->OnReceivedRedirect(
          redirect_info, response_head.Clone(), &removed_headers)) {
    // Double-check if the request is still around. The call above could
    // potentially remove it.
    if (!request_info_)
      return;
    // TODO(yoav): If request_info doesn't change above, we could avoid this
    // copy.
    WebVector<WebString> vector(removed_headers.size());
    std::transform(
        removed_headers.begin(), removed_headers.end(), vector.begin(),
        [](const std::string& h) { return WebString::FromASCII(h); });
    request_info_->removed_headers = vector;
    request_info_->response_url = redirect_info.new_url;
    request_info_->has_pending_redirect = true;
    request_info_->resource_load_info_notifier_wrapper
        ->NotifyResourceRedirectReceived(redirect_info,
                                         std::move(response_head));

    if (request_info_->is_deferred == WebURLLoader::DeferType::kNotDeferred)
      FollowPendingRedirect(request_info_.get());
  } else {
    Cancel(std::move(task_runner));
  }
}

void WebResourceRequestSender::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  TRACE_EVENT0("loading",
               "WebResourceRequestSender::OnStartLoadingResponseBody");

  if (!request_info_)
    return;
  request_info_->peer->OnStartLoadingResponseBody(std::move(body));
}

void WebResourceRequestSender::OnRequestComplete(
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT0("loading", "WebResourceRequestSender::OnRequestComplete");

  if (!request_info_)
    return;
  request_info_->net_error = status.error_code;

  request_info_->resource_load_info_notifier_wrapper
      ->NotifyResourceLoadCompleted(status);

  WebRequestPeer* peer = request_info_->peer.get();

  if (delegate_) {
    delegate_->OnRequestComplete();
  }

  network::URLLoaderCompletionStatus renderer_status(status);
  if (status.completion_time.is_null()) {
    // No completion timestamp is provided, leave it as is.
  } else if (request_info_->remote_request_start.is_null() ||
             request_info_->load_timing_info.request_start.is_null()) {
    // We cannot convert the remote time to a local time, let's use the current
    // timestamp. This happens when
    //  - We get an error before OnReceivedRedirect or OnReceivedResponse is
    //    called, or
    //  - Somehow such a timestamp was missing in the LoadTimingInfo.
    renderer_status.completion_time = base::TimeTicks::Now();
  } else {
    // We have already converted the request start timestamp, let's use that
    // conversion information.
    // Note: We cannot create a InterProcessTimeTicksConverter with
    // (local_request_start, now, remote_request_start, remote_completion_time)
    // as that may result in inconsistent timestamps.
    renderer_status.completion_time =
        std::min(status.completion_time - request_info_->remote_request_start +
                     request_info_->load_timing_info.request_start,
                 base::TimeTicks::Now());
  }
  // The request ID will be removed from our pending list in the destructor.
  // Normally, dispatching this message causes the reference-counted request to
  // die immediately.
  // TODO(kinuko): Revisit here. This probably needs to call request_info_->peer
  // but the past attempt to change it seems to have caused crashes.
  // (crbug.com/547047)
  peer->OnCompletedRequest(renderer_status);
}

void WebResourceRequestSender::ToLocalURLResponseHead(
    const PendingRequestInfo& request_info,
    network::mojom::URLResponseHead& response_head) const {
  if (base::TimeTicks::IsConsistentAcrossProcesses() ||
      request_info.local_request_start.is_null() ||
      request_info.local_response_start.is_null() ||
      response_head.request_start.is_null() ||
      response_head.response_start.is_null() ||
      response_head.load_timing.request_start.is_null()) {
    return;
  }
  InterProcessTimeTicksConverter converter(
      LocalTimeTicks::FromTimeTicks(request_info.local_request_start),
      LocalTimeTicks::FromTimeTicks(request_info.local_response_start),
      RemoteTimeTicks::FromTimeTicks(response_head.request_start),
      RemoteTimeTicks::FromTimeTicks(response_head.response_start));

  net::LoadTimingInfo* load_timing = &response_head.load_timing;
  RemoteToLocalTimeTicks(converter, &load_timing->request_start);
  RemoteToLocalTimeTicks(converter, &load_timing->proxy_resolve_start);
  RemoteToLocalTimeTicks(converter, &load_timing->proxy_resolve_end);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.dns_start);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.dns_end);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.connect_start);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.connect_end);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.ssl_start);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.ssl_end);
  RemoteToLocalTimeTicks(converter, &load_timing->send_start);
  RemoteToLocalTimeTicks(converter, &load_timing->send_end);
  RemoteToLocalTimeTicks(converter, &load_timing->receive_headers_start);
  RemoteToLocalTimeTicks(converter, &load_timing->receive_headers_end);
  RemoteToLocalTimeTicks(converter, &load_timing->push_start);
  RemoteToLocalTimeTicks(converter, &load_timing->push_end);
  RemoteToLocalTimeTicks(converter, &load_timing->service_worker_start_time);
  RemoteToLocalTimeTicks(converter, &load_timing->service_worker_ready_time);
  RemoteToLocalTimeTicks(converter, &load_timing->service_worker_fetch_start);
  RemoteToLocalTimeTicks(converter,
                         &load_timing->service_worker_respond_with_settled);
}

}  // namespace blink
