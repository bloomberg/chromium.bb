// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/throttling_url_loader.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace blink {

namespace {

// Merges |removed_headers_B| into |removed_headers_A|.
void MergeRemovedHeaders(std::vector<std::string>* removed_headers_A,
                         const std::vector<std::string>& removed_headers_B) {
  for (auto& header : removed_headers_B) {
    if (!base::Contains(*removed_headers_A, header))
      removed_headers_A->emplace_back(std::move(header));
  }
}

}  // namespace

const char ThrottlingURLLoader::kFollowRedirectReason[] = "FollowRedirect";

class ThrottlingURLLoader::ForwardingThrottleDelegate
    : public URLLoaderThrottle::Delegate {
 public:
  ForwardingThrottleDelegate(ThrottlingURLLoader* loader,
                             URLLoaderThrottle* throttle)
      : loader_(loader), throttle_(throttle) {}
  ~ForwardingThrottleDelegate() override = default;

  // URLLoaderThrottle::Delegate:
  void CancelWithError(int error_code,
                       base::StringPiece custom_reason) override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->CancelWithError(error_code, custom_reason);
  }

  void Resume() override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->StopDeferringForThrottle(throttle_);
  }

  void SetPriority(net::RequestPriority priority) override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->SetPriority(priority);
  }

  void UpdateDeferredRequestHeaders(
      const net::HttpRequestHeaders& modified_request_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_request_headers)
      override {
    if (!loader_)
      return;
    ScopedDelegateCall scoped_delegate_call(this);
    loader_->UpdateDeferredRequestHeaders(modified_request_headers,
                                          modified_cors_exempt_request_headers);
  }

  void UpdateDeferredResponseHead(
      network::mojom::URLResponseHeadPtr new_response_head) override {
    if (!loader_)
      return;
    ScopedDelegateCall scoped_delegate_call(this);
    loader_->UpdateDeferredResponseHead(std::move(new_response_head));
  }

  void PauseReadingBodyFromNet() override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->PauseReadingBodyFromNet(throttle_);
  }

  void ResumeReadingBodyFromNet() override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->ResumeReadingBodyFromNet(throttle_);
  }

  void InterceptResponse(
      mojo::PendingRemote<network::mojom::URLLoader> new_loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>
          new_client_receiver,
      mojo::PendingRemote<network::mojom::URLLoader>* original_loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>*
          original_client_receiver) override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->InterceptResponse(std::move(new_loader),
                               std::move(new_client_receiver), original_loader,
                               original_client_receiver);
  }

  void RestartWithFlags(int additional_load_flags) override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->RestartWithFlags(additional_load_flags);
  }

  void RestartWithURLResetAndFlags(int additional_load_flags) override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->RestartWithURLResetAndFlags(additional_load_flags);
  }

  void Detach() { loader_ = nullptr; }

 private:
  // This class helps ThrottlingURLLoader to keep track of whether it is being
  // called by its throttles.
  // If ThrottlingURLLoader is destoyed while any of the throttles is calling
  // into it, it delays destruction of the throttles. That way throttles don't
  // need to worry about any delegate calls may destory them synchronously.
  class ScopedDelegateCall {
   public:
    explicit ScopedDelegateCall(ForwardingThrottleDelegate* owner)
        : owner_(owner) {
      DCHECK(owner_->loader_);

      owner_->loader_->inside_delegate_calls_++;
    }

    ~ScopedDelegateCall() {
      // The loader may have been detached and destroyed.
      if (owner_->loader_)
        owner_->loader_->inside_delegate_calls_--;
    }

   private:
    ForwardingThrottleDelegate* const owner_;
    DISALLOW_COPY_AND_ASSIGN(ScopedDelegateCall);
  };

  ThrottlingURLLoader* loader_;
  URLLoaderThrottle* const throttle_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingThrottleDelegate);
};

ThrottlingURLLoader::StartInfo::StartInfo(
    scoped_refptr<network::SharedURLLoaderFactory> in_url_loader_factory,
    int32_t in_routing_id,
    int32_t in_request_id,
    uint32_t in_options,
    network::ResourceRequest* in_url_request,
    scoped_refptr<base::SingleThreadTaskRunner> in_task_runner)
    : url_loader_factory(std::move(in_url_loader_factory)),
      routing_id(in_routing_id),
      request_id(in_request_id),
      options(in_options),
      url_request(*in_url_request),
      task_runner(std::move(in_task_runner)) {}

ThrottlingURLLoader::StartInfo::~StartInfo() = default;

ThrottlingURLLoader::ResponseInfo::ResponseInfo(
    network::mojom::URLResponseHeadPtr in_response_head)
    : response_head(std::move(in_response_head)) {}

ThrottlingURLLoader::ResponseInfo::~ResponseInfo() = default;

ThrottlingURLLoader::RedirectInfo::RedirectInfo(
    const net::RedirectInfo& in_redirect_info,
    network::mojom::URLResponseHeadPtr in_response_head)
    : redirect_info(in_redirect_info),
      response_head(std::move(in_response_head)) {}

ThrottlingURLLoader::RedirectInfo::~RedirectInfo() = default;

ThrottlingURLLoader::PriorityInfo::PriorityInfo(
    net::RequestPriority in_priority,
    int32_t in_intra_priority_value)
    : priority(in_priority), intra_priority_value(in_intra_priority_value) {}

// static
std::unique_ptr<ThrottlingURLLoader> ThrottlingURLLoader::CreateLoaderAndStart(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    network::ResourceRequest* url_request,
    network::mojom::URLLoaderClient* client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  std::unique_ptr<ThrottlingURLLoader> loader(new ThrottlingURLLoader(
      std::move(throttles), client, traffic_annotation));
  loader->Start(std::move(factory), routing_id, request_id, options,
                url_request, std::move(task_runner));
  return loader;
}

ThrottlingURLLoader::~ThrottlingURLLoader() {
  if (inside_delegate_calls_ > 0) {
    // A throttle is calling into this object. In this case, delay destruction
    // of the throttles, so that throttles don't need to worry about any
    // delegate calls may destory them synchronously.
    for (auto& entry : throttles_)
      entry.delegate->Detach();

    auto throttles =
        std::make_unique<std::vector<ThrottleEntry>>(std::move(throttles_));
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                    std::move(throttles));
  }
}

void ThrottlingURLLoader::FollowRedirectForcingRestart() {
  ResetForFollowRedirect();
  client_receiver_.reset();
  CHECK(throttle_will_redirect_redirect_url_.is_empty());

  for (const std::string& header : removed_headers_) {
    start_info_->url_request.headers.RemoveHeader(header);
    start_info_->url_request.cors_exempt_headers.RemoveHeader(header);
  }
  start_info_->url_request.headers.MergeFrom(modified_headers_);
  start_info_->url_request.cors_exempt_headers.MergeFrom(
      modified_cors_exempt_headers_);

  removed_headers_.clear();
  modified_headers_.Clear();
  modified_cors_exempt_headers_.Clear();

  StartNow();
}

void ThrottlingURLLoader::ResetForFollowRedirect() {
  url_loader_.ResetWithReason(
      network::mojom::URLLoader::kClientDisconnectReason,
      kFollowRedirectReason);
}

void ThrottlingURLLoader::RestartWithFactory(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    uint32_t url_loader_options) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);
  url_loader_.reset();
  client_receiver_.reset();
  start_info_->url_loader_factory = std::move(factory);
  start_info_->options = url_loader_options;
  StartNow();
}

void ThrottlingURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers) {
  MergeRemovedHeaders(&removed_headers_, removed_headers);
  modified_headers_.MergeFrom(modified_headers);
  modified_cors_exempt_headers_.MergeFrom(modified_cors_exempt_headers);

  if (!throttle_will_start_redirect_url_.is_empty()) {
    throttle_will_start_redirect_url_ = GURL();
    // This is a synthesized redirect, so no need to tell the URLLoader.
    StartNow();
    return;
  }

  if (url_loader_) {
    base::Optional<GURL> new_url;
    if (!throttle_will_redirect_redirect_url_.is_empty())
      new_url = throttle_will_redirect_redirect_url_;
    url_loader_->FollowRedirect(removed_headers_, modified_headers_,
                                modified_cors_exempt_headers_, new_url);
    throttle_will_redirect_redirect_url_ = GURL();
  }

  removed_headers_.clear();
  modified_headers_.Clear();
  modified_cors_exempt_headers_.Clear();
}

void ThrottlingURLLoader::SetPriority(net::RequestPriority priority,
                                      int32_t intra_priority_value) {
  if (!url_loader_) {
    if (!loader_completed_) {
      // Only check |deferred_stage_| if this resource has not been redirected
      // by a throttle.
      if (throttle_will_start_redirect_url_.is_empty() &&
          throttle_will_redirect_redirect_url_.is_empty()) {
        DCHECK_EQ(DEFERRED_START, deferred_stage_);
      }

      priority_info_ =
          std::make_unique<PriorityInfo>(priority, intra_priority_value);
    }
    return;
  }

  url_loader_->SetPriority(priority, intra_priority_value);
}

void ThrottlingURLLoader::PauseReadingBodyFromNet() {
  PauseReadingBodyFromNet(/*throttle=*/nullptr);
}

void ThrottlingURLLoader::ResumeReadingBodyFromNet() {
  ResumeReadingBodyFromNet(/*throttle=*/nullptr);
}

network::mojom::URLLoaderClientEndpointsPtr ThrottlingURLLoader::Unbind() {
  return network::mojom::URLLoaderClientEndpoints::New(
      url_loader_.Unbind(), client_receiver_.Unbind());
}

ThrottlingURLLoader::ThrottlingURLLoader(
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    network::mojom::URLLoaderClient* client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : forwarding_client_(client), traffic_annotation_(traffic_annotation) {
  throttles_.reserve(throttles.size());
  for (auto& throttle : throttles)
    throttles_.emplace_back(this, std::move(throttle));
}

void ThrottlingURLLoader::Start(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    network::ResourceRequest* url_request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);

  bool deferred = false;
  DCHECK(deferring_throttles_.empty());
  if (!throttles_.empty()) {
    original_url_ = url_request->url;
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      bool throttle_deferred = false;
      throttle->WillStartRequest(url_request, &throttle_deferred);
      if (original_url_ != url_request->url) {
        DCHECK(throttle_will_start_redirect_url_.is_empty())
            << "ThrottlingURLLoader doesn't support multiple throttles "
               "changing the URL.";
        if (original_url_.SchemeIsHTTPOrHTTPS() &&
            !url_request->url.SchemeIsHTTPOrHTTPS() &&
            !throttle->makes_unsafe_redirect()) {
          NOTREACHED() << "A URLLoaderThrottle can't redirect from http(s) to "
                       << "a non http(s) scheme.";
        } else {
          throttle_will_start_redirect_url_ = url_request->url;
        }
        // Restore the original URL so that all throttles see the same original
        // URL.
        url_request->url = original_url_;
      }
      if (!HandleThrottleResult(throttle, throttle_deferred, &deferred))
        return;
    }
  }

  start_info_ =
      std::make_unique<StartInfo>(factory, routing_id, request_id, options,
                                  url_request, std::move(task_runner));
  if (deferred)
    deferred_stage_ = DEFERRED_START;
  else
    StartNow();
}

void ThrottlingURLLoader::StartNow() {
  DCHECK(start_info_);
  if (throttle_will_start_original_url_) {
    throttle_will_start_redirect_url_ = original_url_;
    throttle_will_start_original_url_ = false;
  }
  if (!throttle_will_start_redirect_url_.is_empty()) {
    auto first_party_url_policy =
        start_info_->url_request.update_first_party_url_on_redirect
            ? net::URLRequest::FirstPartyURLPolicy::
                  UPDATE_FIRST_PARTY_URL_ON_REDIRECT
            : net::URLRequest::FirstPartyURLPolicy::
                  NEVER_CHANGE_FIRST_PARTY_URL;

    net::RedirectInfo redirect_info = net::RedirectInfo::ComputeRedirectInfo(
        start_info_->url_request.method, start_info_->url_request.url,
        start_info_->url_request.site_for_cookies, first_party_url_policy,
        start_info_->url_request.referrer_policy,
        start_info_->url_request.referrer.spec(),
        // Use status code 307 to preserve the method, so POST requests work.
        net::HTTP_TEMPORARY_REDIRECT, throttle_will_start_redirect_url_,
        base::nullopt, false, false, false);

    bool should_clear_upload = false;
    net::RedirectUtil::UpdateHttpRequest(
        start_info_->url_request.url, start_info_->url_request.method,
        redirect_info, base::nullopt, base::nullopt,
        &start_info_->url_request.headers, &should_clear_upload);

    if (should_clear_upload)
      start_info_->url_request.request_body = nullptr;

    // Set the new URL in the ResourceRequest struct so that it is the URL
    // that's requested.
    start_info_->url_request.url = throttle_will_start_redirect_url_;

    auto response_head = network::mojom::URLResponseHead::New();
    std::string header_string = base::StringPrintf(
        "HTTP/1.1 %i Internal Redirect\n"
        "Location: %s",
        net::HTTP_TEMPORARY_REDIRECT,
        throttle_will_start_redirect_url_.spec().c_str());

    // This is only needed when CORS is running in the renderer.
    // TODO(crbug.com/1001450): Remove following code once OOR-CORS is fully
    // enabled.
    std::string http_origin;
    if (start_info_->url_request.headers.GetHeader("Origin", &http_origin)) {
      // If this redirect is used in a cross-origin request, add CORS headers
      // to make sure that the redirect gets through. Note that the
      // destination URL is still subject to the usual CORS policy, i.e. the
      // resource will only be available to web pages if the server serves the
      // response with the required CORS response headers.
      header_string += base::StringPrintf(
          "\n"
          "Access-Control-Allow-Origin: %s\n"
          "Access-Control-Allow-Credentials: true",
          http_origin.c_str());
    }

    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(header_string));
    response_head->encoded_data_length = header_string.size();
    start_info_->task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&ThrottlingURLLoader::OnReceiveRedirect,
                       weak_factory_.GetWeakPtr(), std::move(redirect_info),
                       std::move(response_head)));
    return;
  }

  DCHECK(start_info_->url_loader_factory);
  start_info_->url_loader_factory->CreateLoaderAndStart(
      url_loader_.BindNewPipeAndPassReceiver(), start_info_->routing_id,
      start_info_->request_id, start_info_->options, start_info_->url_request,
      client_receiver_.BindNewPipeAndPassRemote(start_info_->task_runner),
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation_));

  // TODO(https://crbug.com/919736): Remove this call.
  client_receiver_.internal_state()->EnableBatchDispatch();

  client_receiver_.set_disconnect_handler(base::BindOnce(
      &ThrottlingURLLoader::OnClientConnectionError, base::Unretained(this)));

  if (!pausing_reading_body_from_net_throttles_.empty())
    url_loader_->PauseReadingBodyFromNet();

  if (priority_info_) {
    auto priority_info = std::move(priority_info_);
    url_loader_->SetPriority(priority_info->priority,
                             priority_info->intra_priority_value);
  }

  // Initialize with the request URL, may be updated when on redirects
  response_url_ = start_info_->url_request.url;
}

void ThrottlingURLLoader::RestartWithFlagsNow() {
  DCHECK(has_pending_restart_);
  url_loader_.reset();
  client_receiver_.reset();
  start_info_->url_request.load_flags |= pending_restart_flags_;
  has_pending_restart_ = false;
  pending_restart_flags_ = 0;
  StartNow();
}

bool ThrottlingURLLoader::HandleThrottleResult(URLLoaderThrottle* throttle,
                                               bool throttle_deferred,
                                               bool* should_defer) {
  DCHECK(!deferring_throttles_.count(throttle));
  if (loader_completed_)
    return false;
  *should_defer |= throttle_deferred;
  if (throttle_deferred)
    deferring_throttles_.insert(throttle);
  return true;
}

void ThrottlingURLLoader::StopDeferringForThrottle(
    URLLoaderThrottle* throttle) {
  if (deferring_throttles_.find(throttle) == deferring_throttles_.end())
    return;

  deferring_throttles_.erase(throttle);
  if (deferring_throttles_.empty() && !loader_completed_)
    Resume();
}

void ThrottlingURLLoader::RestartWithFlags(int additional_load_flags) {
  pending_restart_flags_ |= additional_load_flags;
  has_pending_restart_ = true;
}

void ThrottlingURLLoader::RestartWithURLResetAndFlags(
    int additional_load_flags) {
  pending_restart_flags_ |= additional_load_flags;
  throttle_will_start_original_url_ = true;
  has_pending_restart_ = true;
}

void ThrottlingURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);
  DCHECK(deferring_throttles_.empty());
  TRACE_EVENT1("loading", "ThrottlingURLLoader::OnReceiveResponse", "url",
               response_url_.possibly_invalid_spec());

  // Dispatch BeforeWillProcessResponse().
  if (!throttles_.empty()) {
    pending_restart_flags_ = 0;
    has_pending_restart_ = false;
    bool deferred = false;
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      bool throttle_deferred = false;
      throttle->BeforeWillProcessResponse(response_url_, *response_head,
                                          &throttle_deferred);
      if (!HandleThrottleResult(throttle, throttle_deferred, &deferred))
        return;
    }

    if (deferred) {
      deferred_stage_ = DEFERRED_BEFORE_RESPONSE;
      client_receiver_.Pause();
      return;
    }

    if (has_pending_restart_) {
      RestartWithFlagsNow();
      return;
    }
  }

  // Dispatch WillProcessResponse().
  if (!throttles_.empty()) {
    bool deferred = false;
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      bool throttle_deferred = false;
      throttle->WillProcessResponse(response_url_, response_head.get(),
                                    &throttle_deferred);
      if (!HandleThrottleResult(throttle, throttle_deferred, &deferred))
        return;
    }

    if (deferred) {
      deferred_stage_ = DEFERRED_RESPONSE;
      response_info_ = std::make_unique<ResponseInfo>(std::move(response_head));
      client_receiver_.Pause();
      return;
    }
  }

  forwarding_client_->OnReceiveResponse(std::move(response_head));
}

void ThrottlingURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);
  DCHECK(deferring_throttles_.empty());

  if (!throttles_.empty()) {
    bool deferred = false;
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      bool throttle_deferred = false;
      auto weak_ptr = weak_factory_.GetWeakPtr();
      std::vector<std::string> removed_headers;
      net::HttpRequestHeaders modified_headers;
      net::HttpRequestHeaders modified_cors_exempt_headers;
      net::RedirectInfo redirect_info_copy = redirect_info;
      throttle->WillRedirectRequest(
          &redirect_info_copy, *response_head, &throttle_deferred,
          &removed_headers, &modified_headers, &modified_cors_exempt_headers);
      if (redirect_info_copy.new_url != redirect_info.new_url) {
        DCHECK(throttle_will_redirect_redirect_url_.is_empty())
            << "ThrottlingURLLoader doesn't support multiple throttles "
               "changing the URL.";
        throttle_will_redirect_redirect_url_ = redirect_info_copy.new_url;
      }

      if (!weak_ptr)
        return;
      if (!HandleThrottleResult(throttle, throttle_deferred, &deferred))
        return;

      MergeRemovedHeaders(&removed_headers_, removed_headers);
      modified_headers_.MergeFrom(modified_headers);
      modified_cors_exempt_headers_.MergeFrom(modified_cors_exempt_headers);
    }

    if (deferred) {
      deferred_stage_ = DEFERRED_REDIRECT;
      redirect_info_ = std::make_unique<RedirectInfo>(redirect_info,
                                                      std::move(response_head));
      // |client_receiver_| can be unbound if the redirect came from a
      // throttle.
      if (client_receiver_.is_bound())
        client_receiver_.Pause();
      return;
    }
  }

  // Update the request in case |FollowRedirectForcingRestart()| is called, and
  // needs to use the request updated for the redirect.
  network::ResourceRequest& request = start_info_->url_request;
  request.url = redirect_info.new_url;
  request.method = redirect_info.new_method;
  request.site_for_cookies = redirect_info.new_site_for_cookies;
  request.referrer = GURL(redirect_info.new_referrer);
  request.referrer_policy = redirect_info.new_referrer_policy;

  // TODO(dhausknecht) at this point we do not actually know if we commit to the
  // redirect or if it will be cancelled. FollowRedirect would be a more
  // suitable place to set this URL but there we do not have the data.
  response_url_ = redirect_info.new_url;
  forwarding_client_->OnReceiveRedirect(redirect_info,
                                        std::move(response_head));
}

void ThrottlingURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);

  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(ack_callback));
}

void ThrottlingURLLoader::OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);

  forwarding_client_->OnReceiveCachedMetadata(std::move(data));
}

void ThrottlingURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);

  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void ThrottlingURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);
  TRACE_EVENT1("loading", "ThrottlingURLLoader::OnStartLoadingResponseBody",
               "url", response_url_.possibly_invalid_spec());

  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
}

void ThrottlingURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);

  // Only dispatch WillOnCompleteWithError() if status is not OK.
  if (!throttles_.empty() && status.error_code != net::OK) {
    pending_restart_flags_ = 0;
    has_pending_restart_ = false;
    bool deferred = false;
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      bool throttle_deferred = false;
      throttle->WillOnCompleteWithError(status, &throttle_deferred);
      if (!HandleThrottleResult(throttle, throttle_deferred, &deferred))
        return;
    }

    if (deferred) {
      deferred_stage_ = DEFERRED_COMPLETE;
      client_receiver_.Pause();
      return;
    }

    if (has_pending_restart_) {
      RestartWithFlagsNow();
      return;
    }
  }

  // This is the last expected message. Pipe closure before this is an error
  // (see OnClientConnectionError). After this it is expected and should be
  // ignored. The owner of |this| is expected to destroy |this| when
  // OnComplete() and all data has been read. Destruction of |this| will
  // destroy |url_loader_| appropriately.
  loader_completed_ = true;
  forwarding_client_->OnComplete(status);
}

void ThrottlingURLLoader::OnClientConnectionError() {
  CancelWithError(net::ERR_ABORTED, "");
}

void ThrottlingURLLoader::CancelWithError(int error_code,
                                          base::StringPiece custom_reason) {
  if (loader_completed_)
    return;

  network::URLLoaderCompletionStatus status;
  status.error_code = error_code;
  status.completion_time = base::TimeTicks::Now();

  deferred_stage_ = DEFERRED_NONE;
  DisconnectClient(custom_reason);
  forwarding_client_->OnComplete(status);
}

void ThrottlingURLLoader::Resume() {
  if (loader_completed_ || deferred_stage_ == DEFERRED_NONE)
    return;

  auto prev_deferred_stage = deferred_stage_;
  deferred_stage_ = DEFERRED_NONE;
  switch (prev_deferred_stage) {
    case DEFERRED_START: {
      StartNow();
      break;
    }
    case DEFERRED_REDIRECT: {
      // |client_receiver_| can be unbound if the redirect came from a
      // throttle.
      if (client_receiver_.is_bound())
        client_receiver_.Resume();
      // TODO(dhausknecht) at this point we do not actually know if we commit to
      // the redirect or if it will be cancelled. FollowRedirect would be a more
      // suitable place to set this URL but there we do not have the data.
      response_url_ = redirect_info_->redirect_info.new_url;
      forwarding_client_->OnReceiveRedirect(
          redirect_info_->redirect_info,
          std::move(redirect_info_->response_head));
      // Note: |this| may be deleted here.
      break;
    }
    case DEFERRED_BEFORE_RESPONSE: {
      // TODO(eroman): For simplicity we require throttles that defer during
      // BeforeWillProcessResponse() to do a restart. We could support deferring
      // and choosing not to restart if needed, however the current consumers
      // don't need that.
      CHECK(has_pending_restart_);

      RestartWithFlagsNow();
      // Note: |this| may be deleted here.
      break;
    }
    case DEFERRED_RESPONSE: {
      client_receiver_.Resume();
      forwarding_client_->OnReceiveResponse(
          std::move(response_info_->response_head));
      // Note: |this| may be deleted here.
      break;
    }
    case DEFERRED_COMPLETE: {
      // TODO(eroman): For simplicity we require throttles that defer during
      // WillOnCompleteWithError() to do a restart. We could support deferring
      // and choosing not to restart if needed, however the current consumers
      // don't need that.
      CHECK(has_pending_restart_);

      RestartWithFlagsNow();
      // Note: |this| may be deleted here.
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void ThrottlingURLLoader::SetPriority(net::RequestPriority priority) {
  if (url_loader_)
    url_loader_->SetPriority(priority, -1);
}

void ThrottlingURLLoader::UpdateDeferredRequestHeaders(
    const net::HttpRequestHeaders& modified_request_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_request_headers) {
  if (deferred_stage_ == DEFERRED_START) {
    start_info_->url_request.headers.MergeFrom(modified_request_headers);
    start_info_->url_request.cors_exempt_headers.MergeFrom(
        modified_cors_exempt_request_headers);
  } else if (deferred_stage_ == DEFERRED_REDIRECT) {
    modified_headers_.MergeFrom(modified_request_headers);
    modified_cors_exempt_headers_.MergeFrom(
        modified_cors_exempt_request_headers);
  } else {
    NOTREACHED()
        << "Can only update headers of a request before it's sent out.";
  }
}

void ThrottlingURLLoader::UpdateDeferredResponseHead(
    network::mojom::URLResponseHeadPtr new_response_head) {
  DCHECK(response_info_);
  DCHECK_EQ(DEFERRED_RESPONSE, deferred_stage_);
  response_info_->response_head = std::move(new_response_head);
}

void ThrottlingURLLoader::PauseReadingBodyFromNet(URLLoaderThrottle* throttle) {
  if (pausing_reading_body_from_net_throttles_.empty() && url_loader_)
    url_loader_->PauseReadingBodyFromNet();

  pausing_reading_body_from_net_throttles_.insert(throttle);
}

void ThrottlingURLLoader::ResumeReadingBodyFromNet(
    URLLoaderThrottle* throttle) {
  auto iter = pausing_reading_body_from_net_throttles_.find(throttle);
  if (iter == pausing_reading_body_from_net_throttles_.end())
    return;

  pausing_reading_body_from_net_throttles_.erase(iter);
  if (pausing_reading_body_from_net_throttles_.empty() && url_loader_)
    url_loader_->ResumeReadingBodyFromNet();
}

void ThrottlingURLLoader::InterceptResponse(
    mojo::PendingRemote<network::mojom::URLLoader> new_loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient> new_client_receiver,
    mojo::PendingRemote<network::mojom::URLLoader>* original_loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>*
        original_client_receiver) {
  response_intercepted_ = true;

  if (original_loader)
    *original_loader = url_loader_.Unbind();
  url_loader_.Bind(std::move(new_loader));

  if (original_client_receiver)
    *original_client_receiver = client_receiver_.Unbind();
  client_receiver_.Bind(std::move(new_client_receiver),
                        start_info_->task_runner);
  client_receiver_.set_disconnect_handler(base::BindOnce(
      &ThrottlingURLLoader::OnClientConnectionError, base::Unretained(this)));
}

void ThrottlingURLLoader::DisconnectClient(base::StringPiece custom_reason) {
  client_receiver_.reset();

  if (!custom_reason.empty()) {
    url_loader_.ResetWithReason(
        network::mojom::URLLoader::kClientDisconnectReason,
        custom_reason.as_string());
  } else {
    url_loader_.reset();
  }

  loader_completed_ = true;
}

ThrottlingURLLoader::ThrottleEntry::ThrottleEntry(
    ThrottlingURLLoader* loader,
    std::unique_ptr<URLLoaderThrottle> the_throttle)
    : delegate(
          std::make_unique<ForwardingThrottleDelegate>(loader,
                                                       the_throttle.get())),
      throttle(std::move(the_throttle)) {
  throttle->set_delegate(delegate.get());
}

ThrottlingURLLoader::ThrottleEntry::ThrottleEntry(ThrottleEntry&& other) =
    default;

ThrottlingURLLoader::ThrottleEntry::~ThrottleEntry() = default;

ThrottlingURLLoader::ThrottleEntry& ThrottlingURLLoader::ThrottleEntry::
operator=(ThrottleEntry&& other) = default;

}  // namespace blink
