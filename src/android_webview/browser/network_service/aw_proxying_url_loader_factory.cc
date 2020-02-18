// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_proxying_url_loader_factory.h"

#include <utility>

#include "android_webview/browser/android_protocol_handler.h"
#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/input_stream.h"
#include "android_webview/browser/network_service/android_stream_reader_url_loader.h"
#include "android_webview/browser/network_service/aw_web_resource_response.h"
#include "android_webview/browser/network_service/net_helpers.h"
#include "android_webview/browser/renderer_host/auto_login_parser.h"
#include "android_webview/common/url_constants.h"
#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/common/safebrowsing_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_utils.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"

namespace android_webview {

namespace {

const char kAutoLoginHeaderName[] = "X-Auto-Login";

// Handles intercepted, in-progress requests/responses, so that they can be
// controlled and modified accordingly.
class InterceptedRequest : public network::mojom::URLLoader,
                           public network::mojom::URLLoaderClient {
 public:
  InterceptedRequest(
      int process_id,
      uint64_t request_id,
      int32_t routing_id,
      uint32_t options,
      const network::ResourceRequest& request,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      network::mojom::URLLoaderRequest loader_request,
      network::mojom::URLLoaderClientPtr client,
      network::mojom::URLLoaderFactoryPtr target_factory,
      bool intercept_only);
  ~InterceptedRequest() override;

  void Restart();

  // network::mojom::URLLoaderClient
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // network::mojom::URLLoader
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void ContinueAfterIntercept();
  void ContinueAfterInterceptWithOverride(
      std::unique_ptr<AwWebResourceResponse> response);

  void InterceptResponseReceived(
      std::unique_ptr<AwWebResourceResponse> response);

  // Returns true if the request was restarted or completed.
  bool InputStreamFailed(bool restart_needed);

 private:
  std::unique_ptr<AwContentsIoThreadClient> GetIoThreadClient();

  // This is called when the original URLLoaderClient has a connection error.
  void OnURLLoaderClientError();

  // This is called when the original URLLoader has a connection error.
  void OnURLLoaderError(uint32_t custom_reason, const std::string& description);

  // Call OnComplete on |target_client_|. If |wait_for_loader_error| is true
  // then this object will wait for |proxied_loader_binding_| to have a
  // connection error before destructing.
  void CallOnComplete(const network::URLLoaderCompletionStatus& status,
                      bool wait_for_loader_error);

  void SendErrorAndCompleteImmediately(int error_code);

  // TODO(timvolodine): consider factoring this out of this class.
  bool ShouldNotInterceptRequest();

  // Posts the error callback to the UI thread, ensuring that at most we send
  // only one.
  void SendErrorCallback(int error_code, bool safebrowsing_hit);

  const int process_id_;
  const uint64_t request_id_;
  const int32_t routing_id_;
  const uint32_t options_;
  bool input_stream_previously_failed_ = false;
  bool request_was_redirected_ = false;

  // To avoid sending multiple OnReceivedError callbacks.
  bool sent_error_callback_ = false;

  // When true, the loader will not not proceed unless the
  // shouldInterceptRequest callback provided a non-null response.
  bool intercept_only_ = false;

  // If the |target_loader_| called OnComplete with an error this stores it.
  // That way the destructor can send it to OnReceivedError if safe browsing
  // error didn't occur.
  int error_status_ = net::OK;

  network::ResourceRequest request_;

  const net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  mojo::Binding<network::mojom::URLLoader> proxied_loader_binding_;
  network::mojom::URLLoaderClientPtr target_client_;

  mojo::Binding<network::mojom::URLLoaderClient> proxied_client_binding_;
  network::mojom::URLLoaderPtr target_loader_;
  network::mojom::URLLoaderFactoryPtr target_factory_;

  base::WeakPtrFactory<InterceptedRequest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InterceptedRequest);
};

// A ResponseDelegate for responses returned by shouldInterceptRequest.
class InterceptResponseDelegate
    : public AndroidStreamReaderURLLoader::ResponseDelegate {
 public:
  explicit InterceptResponseDelegate(
      std::unique_ptr<AwWebResourceResponse> response,
      base::WeakPtr<InterceptedRequest> request)
      : response_(std::move(response)), request_(request) {}

  std::unique_ptr<android_webview::InputStream> OpenInputStream(
      JNIEnv* env) override {
    return response_->GetInputStream(env);
  }

  bool OnInputStreamOpenFailed() override {
    // return true if there is no valid request, meaning it has completed or
    // deleted.
    return request_ ? request_->InputStreamFailed(false /* restart_needed */)
                    : true;
  }

  bool GetMimeType(JNIEnv* env,
                   const GURL& url,
                   android_webview::InputStream* stream,
                   std::string* mime_type) override {
    return response_->GetMimeType(env, mime_type);
  }

  bool GetCharset(JNIEnv* env,
                  const GURL& url,
                  android_webview::InputStream* stream,
                  std::string* charset) override {
    return response_->GetCharset(env, charset);
  }

  void AppendResponseHeaders(JNIEnv* env,
                             net::HttpResponseHeaders* headers) override {
    int status_code;
    std::string reason_phrase;
    if (response_->GetStatusInfo(env, &status_code, &reason_phrase)) {
      std::string status_line("HTTP/1.1 ");
      status_line.append(base::NumberToString(status_code));
      status_line.append(" ");
      status_line.append(reason_phrase);
      headers->ReplaceStatusLine(status_line);
    }
    response_->GetResponseHeaders(env, headers);
  }

 private:
  std::unique_ptr<AwWebResourceResponse> response_;
  base::WeakPtr<InterceptedRequest> request_;
};

// A ResponseDelegate based on top of AndroidProtocolHandler for special
// protocols, such as content://, file:///android_asset, and file:///android_res
// URLs.
class ProtocolResponseDelegate
    : public AndroidStreamReaderURLLoader::ResponseDelegate {
 public:
  ProtocolResponseDelegate(const GURL& url,
                           base::WeakPtr<InterceptedRequest> request)
      : url_(url), request_(request) {}

  std::unique_ptr<android_webview::InputStream> OpenInputStream(
      JNIEnv* env) override {
    return CreateInputStream(env, url_);
  }

  bool OnInputStreamOpenFailed() override {
    // return true if there is no valid request, meaning it has completed or has
    // been deleted.
    return request_ ? request_->InputStreamFailed(true /* restart_needed */)
                    : true;
  }

  bool GetMimeType(JNIEnv* env,
                   const GURL& url,
                   android_webview::InputStream* stream,
                   std::string* mime_type) override {
    return GetInputStreamMimeType(env, url, stream, mime_type);
  }

  bool GetCharset(JNIEnv* env,
                  const GURL& url,
                  android_webview::InputStream* stream,
                  std::string* charset) override {
    // TODO: We should probably be getting this from the managed side.
    return false;
  }

  void AppendResponseHeaders(JNIEnv* env,
                             net::HttpResponseHeaders* headers) override {
    // no-op
  }

 private:
  GURL url_;
  base::WeakPtr<InterceptedRequest> request_;
};

InterceptedRequest::InterceptedRequest(
    int process_id,
    uint64_t request_id,
    int32_t routing_id,
    uint32_t options,
    const network::ResourceRequest& request,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    network::mojom::URLLoaderRequest loader_request,
    network::mojom::URLLoaderClientPtr client,
    network::mojom::URLLoaderFactoryPtr target_factory,
    bool intercept_only)
    : process_id_(process_id),
      request_id_(request_id),
      routing_id_(routing_id),
      options_(options),
      intercept_only_(intercept_only),
      request_(request),
      traffic_annotation_(traffic_annotation),
      proxied_loader_binding_(this, std::move(loader_request)),
      target_client_(std::move(client)),
      proxied_client_binding_(this),
      target_factory_(std::move(target_factory)) {
  // If there is a client error, clean up the request.
  target_client_.set_connection_error_handler(base::BindOnce(
      &InterceptedRequest::OnURLLoaderClientError, base::Unretained(this)));
  proxied_loader_binding_.set_connection_error_with_reason_handler(
      base::BindOnce(&InterceptedRequest::OnURLLoaderError,
                     base::Unretained(this)));
}

InterceptedRequest::~InterceptedRequest() {
  if (error_status_ != net::OK)
    SendErrorCallback(error_status_, false);
}

void InterceptedRequest::Restart() {
  std::unique_ptr<AwContentsIoThreadClient> io_thread_client =
      GetIoThreadClient();

  if (ShouldBlockURL(request_.url, io_thread_client.get())) {
    SendErrorAndCompleteImmediately(net::ERR_ACCESS_DENIED);
    return;
  }

  request_.load_flags =
      UpdateLoadFlags(request_.load_flags, io_thread_client.get());
  if (!io_thread_client || ShouldNotInterceptRequest()) {
    // equivalent to no interception
    InterceptResponseReceived(nullptr);
  } else {
    if (request_.referrer.is_valid()) {
      // intentionally override if referrer header already exists
      request_.headers.SetHeader(net::HttpRequestHeaders::kReferer,
                                 request_.referrer.spec());
    }

    // TODO: verify the case when WebContents::RenderFrameDeleted is called
    // before network request is intercepted (i.e. if that's possible and
    // whether it can result in any issues).
    io_thread_client->ShouldInterceptRequestAsync(
        AwWebResourceRequest(request_),
        base::BindOnce(&InterceptedRequest::InterceptResponseReceived,
                       weak_factory_.GetWeakPtr()));
  }
}

// logic for when not to invoke shouldInterceptRequest callback
bool InterceptedRequest::ShouldNotInterceptRequest() {
  if (request_was_redirected_)
    return true;

  // Do not call shouldInterceptRequest callback for special android urls,
  // unless they fail to load on first attempt. Special android urls are urls
  // such as "file:///android_asset/", "file:///android_res/" urls or
  // "content:" scheme urls.
  return !input_stream_previously_failed_ &&
         (request_.url.SchemeIs(url::kContentScheme) ||
          android_webview::IsAndroidSpecialFileUrl(request_.url));
}

void InterceptedRequest::InterceptResponseReceived(
    std::unique_ptr<AwWebResourceResponse> response) {
  // We send the application's package name in the X-Requested-With header for
  // compatibility with previous WebView versions. This should not be visible to
  // shouldInterceptRequest. It should also not trigger CORS prefetch if
  // OOR-CORS is enabled.
  if (!request_.headers.HasHeader(
          content::kCorsExemptRequestedWithHeaderName)) {
    request_.cors_exempt_headers.SetHeader(
        content::kCorsExemptRequestedWithHeaderName,
        base::android::BuildInfo::GetInstance()->host_package_name());
  }

  if (response) {
    // non-null response: make sure to use it as an override for the
    // normal network data.
    ContinueAfterInterceptWithOverride(std::move(response));
  } else {
    // Request was not intercepted/overridden. Proceed with loading
    // from network, unless this is a special |intercept_only_| loader,
    // which happens for external schemes: e.g. unsupported schemes and
    // cid: schemes.
    if (intercept_only_) {
      SendErrorAndCompleteImmediately(net::ERR_UNKNOWN_URL_SCHEME);
      return;
    }
    ContinueAfterIntercept();
  }
}

// returns true if the request has been restarted or was completed.
bool InterceptedRequest::InputStreamFailed(bool restart_needed) {
  DCHECK(!input_stream_previously_failed_);

  if (intercept_only_) {
    // This can happen for unsupported schemes, when no proper
    // response from shouldInterceptRequest() is received, i.e.
    // the provided input stream in response failed to load. In
    // this case we send and error and stop loading.
    SendErrorAndCompleteImmediately(net::ERR_UNKNOWN_URL_SCHEME);
    return true;  // request completed
  }

  if (!restart_needed) {
    // request will not be restarted, error reporting will be done
    // via other means e.g. setting appropriate response header status.
    return false;
  }

  input_stream_previously_failed_ = true;
  proxied_client_binding_.Unbind();
  Restart();
  return true;  // request restarted
}

void InterceptedRequest::ContinueAfterIntercept() {
  // For WebViewClassic compatibility this job can only accept URLs that can be
  // opened. URLs that cannot be opened should be resolved by the next handler.
  //
  // If a request is initially handled here but the job fails due to it being
  // unable to open the InputStream for that request the request is marked as
  // previously failed and restarted.
  // Restarting a request involves creating a new job for that request. This
  // handler will ignore requests known to have previously failed to 1) prevent
  // an infinite loop, 2) ensure that the next handler in line gets the
  // opportunity to create a job for the request.
  if (!input_stream_previously_failed_ &&
      (request_.url.SchemeIs(url::kContentScheme) ||
       android_webview::IsAndroidSpecialFileUrl(request_.url))) {
    network::mojom::URLLoaderClientPtr proxied_client;
    proxied_client_binding_.Bind(mojo::MakeRequest(&proxied_client));
    AndroidStreamReaderURLLoader* loader = new AndroidStreamReaderURLLoader(
        request_, std::move(proxied_client), traffic_annotation_,
        std::make_unique<ProtocolResponseDelegate>(request_.url,
                                                   weak_factory_.GetWeakPtr()));
    loader->Start();
    return;
  }

  if (!target_loader_ && target_factory_) {
    network::mojom::URLLoaderClientPtr proxied_client;
    proxied_client_binding_.Bind(mojo::MakeRequest(&proxied_client));
    target_factory_->CreateLoaderAndStart(
        mojo::MakeRequest(&target_loader_), routing_id_, request_id_, options_,
        request_, std::move(proxied_client), traffic_annotation_);
  }
}

void InterceptedRequest::ContinueAfterInterceptWithOverride(
    std::unique_ptr<AwWebResourceResponse> response) {
  network::mojom::URLLoaderClientPtr proxied_client;
  proxied_client_binding_.Bind(mojo::MakeRequest(&proxied_client));
  AndroidStreamReaderURLLoader* loader = new AndroidStreamReaderURLLoader(
      request_, std::move(proxied_client), traffic_annotation_,
      std::make_unique<InterceptResponseDelegate>(std::move(response),
                                                  weak_factory_.GetWeakPtr()));
  loader->Start();
}

namespace {
// TODO(timvolodine): consider factoring this out of this file.

AwContentsClientBridge* GetAwContentsClientBridgeFromID(int process_id,
                                                        int render_frame_id) {
  content::WebContents* wc =
      process_id
          ? content::WebContents::FromRenderFrameHost(
                content::RenderFrameHost::FromID(process_id, render_frame_id))
          : content::WebContents::FromFrameTreeNodeId(render_frame_id);
  return AwContentsClientBridge::FromWebContents(wc);
}

void OnReceivedHttpErrorOnUiThread(
    int process_id,
    int render_frame_id,
    const AwWebResourceRequest& request,
    std::unique_ptr<AwContentsClientBridge::HttpErrorInfo> http_error_info) {
  auto* client = GetAwContentsClientBridgeFromID(process_id, render_frame_id);
  if (!client) {
    DLOG(WARNING) << "client is null, onReceivedHttpError dropped for "
                  << request.url;
    return;
  }
  client->OnReceivedHttpError(request, std::move(http_error_info));
}

void OnReceivedErrorOnUiThread(int process_id,
                               int render_frame_id,
                               const AwWebResourceRequest& request,
                               int error_code,
                               bool safebrowsing_hit) {
  auto* client = GetAwContentsClientBridgeFromID(process_id, render_frame_id);
  if (!client) {
    DLOG(WARNING) << "client is null, onReceivedError dropped for "
                  << request.url;
    return;
  }
  client->OnReceivedError(request, error_code, safebrowsing_hit);
}

void OnNewLoginRequestOnUiThread(int process_id,
                                 int render_frame_id,
                                 const std::string& realm,
                                 const std::string& account,
                                 const std::string& args) {
  auto* client = GetAwContentsClientBridgeFromID(process_id, render_frame_id);
  if (!client) {
    return;
  }
  client->NewLoginRequest(realm, account, args);
}

}  // namespace

// URLLoaderClient methods.

void InterceptedRequest::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head) {
  // intercept response headers here
  // pause/resume proxied_client_binding_ if necessary

  if (head->headers && head->headers->response_code() >= 400) {
    // In Android WebView the WebViewClient.onReceivedHttpError callback
    // is invoked for any resource (main page, iframe, image, etc.) with
    // status code >= 400.
    std::unique_ptr<AwContentsClientBridge::HttpErrorInfo> error_info =
        AwContentsClientBridge::ExtractHttpErrorInfo(head->headers.get());

    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&OnReceivedHttpErrorOnUiThread, process_id_,
                       request_.render_frame_id, AwWebResourceRequest(request_),
                       std::move(error_info)));
  }

  if (request_.resource_type ==
      static_cast<int>(content::ResourceType::kMainFrame)) {
    // Check for x-auto-login-header
    HeaderData header_data;
    std::string header_string;
    if (head->headers && head->headers->GetNormalizedHeader(
                             kAutoLoginHeaderName, &header_string)) {
      if (ParseHeader(header_string, ALLOW_ANY_REALM, &header_data)) {
        // TODO(timvolodine): consider simplifying this and above callback
        // code, crbug.com/897149.
        base::PostTask(
            FROM_HERE, {content::BrowserThread::UI},
            base::BindOnce(&OnNewLoginRequestOnUiThread, process_id_,
                           request_.render_frame_id, header_data.realm,
                           header_data.account, header_data.args));
      }
    }
  }

  target_client_->OnReceiveResponse(std::move(head));
}

void InterceptedRequest::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  // TODO(timvolodine): handle redirect override.
  request_was_redirected_ = true;
  target_client_->OnReceiveRedirect(redirect_info, std::move(head));
  request_.url = redirect_info.new_url;
  request_.method = redirect_info.new_method;
  request_.site_for_cookies = redirect_info.new_site_for_cookies;
  request_.referrer = GURL(redirect_info.new_referrer);
  request_.referrer_policy = redirect_info.new_referrer_policy;
}

void InterceptedRequest::OnUploadProgress(int64_t current_position,
                                          int64_t total_size,
                                          OnUploadProgressCallback callback) {
  target_client_->OnUploadProgress(current_position, total_size,
                                   std::move(callback));
}

void InterceptedRequest::OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  target_client_->OnReceiveCachedMetadata(std::move(data));
}

void InterceptedRequest::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  target_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void InterceptedRequest::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  target_client_->OnStartLoadingResponseBody(std::move(body));
}

void InterceptedRequest::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  // Only wait for the original loader to possibly have a custom error if the
  // target loader succeeded. If the target loader failed, then it was a race as
  // to whether that error or the safe browsing error would be reported.
  CallOnComplete(status, status.error_code == net::OK);
}

// URLLoader methods.

void InterceptedRequest::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {
  if (target_loader_)
    target_loader_->FollowRedirect(removed_headers, modified_headers, new_url);

  // If |OnURLLoaderClientError| was called then we're just waiting for the
  // connection error handler of |proxied_loader_binding_|. Don't restart the
  // job since that'll create another URLLoader
  if (!target_client_)
    return;

  Restart();
}

void InterceptedRequest::SetPriority(net::RequestPriority priority,
                                     int32_t intra_priority_value) {
  if (target_loader_)
    target_loader_->SetPriority(priority, intra_priority_value);
}

void InterceptedRequest::PauseReadingBodyFromNet() {
  if (target_loader_)
    target_loader_->PauseReadingBodyFromNet();
}

void InterceptedRequest::ResumeReadingBodyFromNet() {
  if (target_loader_)
    target_loader_->ResumeReadingBodyFromNet();
}

std::unique_ptr<AwContentsIoThreadClient>
InterceptedRequest::GetIoThreadClient() {
  if (request_.originated_from_service_worker) {
    return AwContentsIoThreadClient::GetServiceWorkerIoThreadClient();
  }

  // |process_id_| == 0 indicates this is a navigation, and so we should use the
  // frame_tree_node_id API (with request_.render_frame_id).
  return process_id_
             ? AwContentsIoThreadClient::FromID(process_id_,
                                                request_.render_frame_id)
             : AwContentsIoThreadClient::FromID(request_.render_frame_id);
}

void InterceptedRequest::OnURLLoaderClientError() {
  // We set |wait_for_loader_error| to true because if the loader did have a
  // custom_reason error then the client would be reset as well and it would be
  // a race as to which connection error we saw first.
  CallOnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED),
                 true /* wait_for_loader_error */);
}

void InterceptedRequest::OnURLLoaderError(uint32_t custom_reason,
                                          const std::string& description) {
  if (custom_reason == network::mojom::URLLoader::kClientDisconnectReason) {
    if (description == safe_browsing::kCustomCancelReasonForURLLoader) {
      SendErrorCallback(safe_browsing::GetNetErrorCodeForSafeBrowsing(), true);
    } else {
      int parsed_error_code;
      if (base::StringToInt(base::StringPiece(description),
                            &parsed_error_code)) {
        SendErrorCallback(parsed_error_code, false);
      }
    }
  }

  // If CallOnComplete was already called, then this object is ready to be
  // deleted.
  if (!target_client_)
    delete this;
}

void InterceptedRequest::CallOnComplete(
    const network::URLLoaderCompletionStatus& status,
    bool wait_for_loader_error) {
  // Save an error status so that we call onReceiveError at destruction if there
  // was no safe browsing error.
  if (status.error_code != net::OK)
    error_status_ = status.error_code;

  if (target_client_)
    target_client_->OnComplete(status);

  if (proxied_loader_binding_ && wait_for_loader_error) {
    // Don't delete |this| yet, in case the |proxied_loader_binding_|'s
    // error_handler is called with a reason to indicate an error which we want
    // to send to the client bridge. Also reset |target_client_| so we don't
    // get its error_handler called and then delete |this|.
    target_client_.reset();

    // Since the original client is gone no need to continue loading the
    // request.
    proxied_client_binding_.Close();
    target_loader_.reset();

    // In case there are pending checks as to whether this request should be
    // intercepted, we don't want that causing |target_client_| to be used
    // later.
    weak_factory_.InvalidateWeakPtrs();
  } else {
    delete this;
  }
}

void InterceptedRequest::SendErrorAndCompleteImmediately(int error_code) {
  auto status = network::URLLoaderCompletionStatus(error_code);
  SendErrorCallback(status.error_code, false);
  target_client_->OnComplete(status);
  delete this;
}

void InterceptedRequest::SendErrorCallback(int error_code,
                                           bool safebrowsing_hit) {
  // Ensure we only send one error callback, e.g. to avoid sending two if
  // there's both a networking error and safe browsing blocked the request.
  if (sent_error_callback_)
    return;

  sent_error_callback_ = true;
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&OnReceivedErrorOnUiThread, process_id_,
                     request_.render_frame_id, AwWebResourceRequest(request_),
                     error_code, safebrowsing_hit));
}

}  // namespace

//============================
// AwProxyingURLLoaderFactory
//============================

AwProxyingURLLoaderFactory::AwProxyingURLLoaderFactory(
    int process_id,
    network::mojom::URLLoaderFactoryRequest loader_request,
    network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
    bool intercept_only)
    : process_id_(process_id), intercept_only_(intercept_only) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!(intercept_only_ && target_factory_info));
  if (target_factory_info) {
    target_factory_.Bind(std::move(target_factory_info));
    target_factory_.set_connection_error_handler(
        base::BindOnce(&AwProxyingURLLoaderFactory::OnTargetFactoryError,
                       base::Unretained(this)));
  }
  proxy_bindings_.AddBinding(this, std::move(loader_request));
  proxy_bindings_.set_connection_error_handler(
      base::BindRepeating(&AwProxyingURLLoaderFactory::OnProxyBindingError,
                          base::Unretained(this)));
}

AwProxyingURLLoaderFactory::~AwProxyingURLLoaderFactory() {}

// static
void AwProxyingURLLoaderFactory::CreateProxy(
    int process_id,
    network::mojom::URLLoaderFactoryRequest loader_request,
    network::mojom::URLLoaderFactoryPtrInfo target_factory_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // will manage its own lifetime
  new AwProxyingURLLoaderFactory(process_id, std::move(loader_request),
                                 std::move(target_factory_info), false);
}

void AwProxyingURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  // TODO(timvolodine): handle interception, modification (headers for
  // webview), blocking, callbacks etc..

  network::mojom::URLLoaderFactoryPtr target_factory_clone;
  if (target_factory_)
    target_factory_->Clone(MakeRequest(&target_factory_clone));

  bool global_cookie_policy =
      AwCookieAccessPolicy::GetInstance()->GetShouldAcceptCookies();
  // process_id == 0 means the render_frame_id is actually a valid
  // frame_tree_node_id, otherwise use it as a valid render_frame_id.
  int frame_tree_node_id = process_id_
                               ? content::RenderFrameHost::kNoFrameTreeNodeId
                               : request.render_frame_id;
  bool third_party_cookie_policy =
      AwCookieAccessPolicy::GetInstance()->GetShouldAcceptThirdPartyCookies(
          process_id_, request.render_frame_id, frame_tree_node_id);
  if (!global_cookie_policy) {
    options |= network::mojom::kURLLoadOptionBlockAllCookies;
  } else if (!third_party_cookie_policy && !request.url.SchemeIsFile()) {
    // Special case: if the application has asked that we allow file:// scheme
    // URLs to set cookies, we need to avoid setting a cookie policy (as file://
    // scheme URLs are third-party to everything).
    options |= network::mojom::kURLLoadOptionBlockThirdPartyCookies;
  }

  // manages its own lifecycle
  // TODO(timvolodine): consider keeping track of requests.
  InterceptedRequest* req = new InterceptedRequest(
      process_id_, request_id, routing_id, options, request, traffic_annotation,
      std::move(loader), std::move(client), std::move(target_factory_clone),
      intercept_only_);
  req->Restart();
}

void AwProxyingURLLoaderFactory::OnTargetFactoryError() {
  delete this;
}

void AwProxyingURLLoaderFactory::OnProxyBindingError() {
  if (proxy_bindings_.empty())
    delete this;
}

void AwProxyingURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest loader_request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  proxy_bindings_.AddBinding(this, std::move(loader_request));
}

}  // namespace android_webview
