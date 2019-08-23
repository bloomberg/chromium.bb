/*
 * Copyright (C) 2019 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "base/logging.h"
#include "content/renderer/loader/resource_loader_bridge.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/blink/public/web/web_local_frame.h"


namespace content {
ResourceRequestInfoProvider::ResourceRequestInfoProvider() = default;
ResourceRequestInfoProvider::~ResourceRequestInfoProvider() = default;

const GURL& ResourceRequestInfoProvider::url() const {
  return url_;
}

const GURL& ResourceRequestInfoProvider::firstPartyForCookies() const {
  return firstPartyForCookies_;
}

bool ResourceRequestInfoProvider::allowStoredCredentials() const {
  return allowStoredCredentials_;
}

int ResourceRequestInfoProvider::loadFlags() const {
  return loadFlags_;
}

const std::string& ResourceRequestInfoProvider::httpMethod() const {
  return httpMethod_;
}

const net::HttpRequestHeaders& ResourceRequestInfoProvider::requestHeaders()
    const {
  return requestHeaders_;
}
bool ResourceRequestInfoProvider::reportUploadProgress() const {
  return reportUploadProgress_;
}
bool ResourceRequestInfoProvider::reportRawHeaders() const {
  return reportRawHeaders_;
}

bool ResourceRequestInfoProvider::hasUserGesture() const {
  return hasUserGesture_;
}

int ResourceRequestInfoProvider::routingId() const {
  return routingId_;
}

base::Optional<base::UnguessableToken>
ResourceRequestInfoProvider::appCacheHostId() const {
  return appCacheHostId_;
}

net::RequestPriority ResourceRequestInfoProvider::priority() const {
  return priority_;
}

scoped_refptr<network::ResourceRequestBody>
ResourceRequestInfoProvider::requestBody() const {
  return requestBody_;
}

BodyLoaderRequestInfoProvider::BodyLoaderRequestInfoProvider(
    const CommonNavigationParams& common_params,
    const CommitNavigationParams& commit_params,
    const network::ResourceResponseHead& head,
    const network::mojom::URLLoaderClientEndpointsPtr& client_endpoints,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    int render_frame_id,
    const mojom::ResourceLoadInfoPtr& resource_load_info) {
  url_ = (common_params.url);
  firstPartyForCookies_ = (common_params.url);
  allowStoredCredentials_ = (false);
  loadFlags_ = (0);
  httpMethod_ = (resource_load_info->method);
  requestHeaders_ = {};
  reportUploadProgress_ = (false);
  reportRawHeaders_ = (false);
  hasUserGesture_ = common_params.has_user_gesture;
  routingId_ = (render_frame_id);
  appCacheHostId_ = (commit_params.appcache_host_id);
  priority_ = net::MEDIUM;
  requestBody_ = common_params.post_data;
}

BodyLoaderRequestInfoProvider::~BodyLoaderRequestInfoProvider() = default;

PeerRequestInfoProvider::PeerRequestInfoProvider(
    const network::ResourceRequest* request) {
  url_ = request->url;
  firstPartyForCookies_ = request->site_for_cookies;
  loadFlags_ = request->load_flags;
  httpMethod_ = request->method;
  reportUploadProgress_ = request->enable_upload_progress;
  reportRawHeaders_ = request->report_raw_headers;
  hasUserGesture_ = request->has_user_gesture;
  routingId_ = request->render_frame_id;
  appCacheHostId_ = request->appcache_host_id;
  priority_ = request->priority;
  requestBody_ = request->request_body;
  requestHeaders_.MergeFrom(request->headers);
}

PeerRequestInfoProvider::~PeerRequestInfoProvider() = default;

ResourceReceiver::ResourceReceiver() = default;
ResourceReceiver::~ResourceReceiver() = default;

BodyLoaderReceiver::BodyLoaderReceiver(
    int render_frame_id,
    blink::WebNavigationBodyLoader::Client* client)
    : render_frame_id_(render_frame_id), client_(client) {}

BodyLoaderReceiver::~BodyLoaderReceiver() = default;

void BodyLoaderReceiver::OnReceivedResponse(
    const network::ResourceResponseInfo& info) {}

void BodyLoaderReceiver::OnReceivedData(const char* data,
                                        std::size_t data_length) {
  if (!IsClientValid()) {
    return;
  }
  client_->BodyDataReceived(base::span<const char>(data, data_length));
}

void BodyLoaderReceiver::OnCompletedRequest(
    const network::URLLoaderCompletionStatus& completeStatus,
    const GURL& url) {
  if(!IsClientValid()) {
    return;
  }
  base::Optional<blink::WebURLError> web_errorCode;
  if (completeStatus.error_code) {
    web_errorCode = blink::WebURLError(completeStatus.error_code, url);
  }
  client_->BodyLoadingFinished(
      base::TimeTicks::Now(), completeStatus.encoded_data_length,
      completeStatus.encoded_body_length, completeStatus.decoded_body_length,
      false, std::move(web_errorCode));
}

bool BodyLoaderReceiver::IsClientValid() const {
  RenderFrameImpl* render_frame =
      RenderFrameImpl::FromRoutingID(render_frame_id_);
  if (!render_frame) {
    LOG(WARNING) << "Invalid RenderFrameImpl for id: " << render_frame_id_;
    return false;
  }
  const blink::WebLocalFrame* web_frame = render_frame->GetWebFrame();
  if (!web_frame) {
    LOG(WARNING) << "Invalid WebLocalFrame for id: " << render_frame_id_;
    return false;
  }
  if (!web_frame->GetDocumentLoader()) {
    LOG(WARNING) << "Invalid document loader for id: " << render_frame_id_;
    return false;
  }
  return true;
}

RequestPeerReceiver::RequestPeerReceiver(
    content::RequestPeer* peer,
    int request_id,
    scoped_refptr<base::SingleThreadTaskRunner> runner)
    : peer_(peer), request_id_(request_id), runner_(std::move(runner)) {}
RequestPeerReceiver::~RequestPeerReceiver() = default;

void RequestPeerReceiver::OnReceivedResponse(
    const network::ResourceResponseInfo& info) {
  peer_->OnReceivedResponse(info);
}

void RequestPeerReceiver::OnReceivedData(const char* data,
                                        std::size_t data_length) {
  mojo::DataPipe data_pipe(data_length);
  peer_->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));
  uint32_t len = data_length;
  MojoResult result = data_pipe.producer_handle->WriteData(data, &len, MOJO_WRITE_DATA_FLAG_NONE);
  DCHECK(result == MOJO_RESULT_OK);
  data_pipe.producer_handle.reset();
}

void RequestPeerReceiver::OnCompletedRequest(
    const network::URLLoaderCompletionStatus& completeStatus,
    const GURL&) {
  peer_->OnCompletedRequest(completeStatus);
  if (RenderThreadImpl* thread_impl = RenderThreadImpl::current()) {
    if (ResourceDispatcher* dispatcher = thread_impl->resource_dispatcher()) {
      dispatcher->RemovePendingRequest(request_id_, runner_);
    }
  }
}

ResourceLoaderBridge::ResourceLoaderBridge(
    const content::ResourceRequestInfoProvider&) {}

ResourceLoaderBridge::~ResourceLoaderBridge() = default;

}  // namespace content
