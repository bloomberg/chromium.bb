/*
 * Copyright (C) 2010, 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/exported/WebAssociatedURLLoaderImpl.h"

#include <limits.h>
#include <memory>
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/loader/DocumentThreadableLoader.h"
#include "core/loader/DocumentThreadableLoaderClient.h"
#include "core/loader/ThreadableLoadingContext.h"
#include "platform/Timer.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/loader/fetch/CrossOriginAccessControl.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/network/HTTPParsers.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebHTTPHeaderVisitor.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include "public/web/WebAssociatedURLLoaderClient.h"
#include "public/web/WebDataSource.h"

namespace blink {

namespace {

class HTTPRequestHeaderValidator : public WebHTTPHeaderVisitor {
  WTF_MAKE_NONCOPYABLE(HTTPRequestHeaderValidator);

 public:
  HTTPRequestHeaderValidator() : is_safe_(true) {}
  ~HTTPRequestHeaderValidator() override {}

  void VisitHeader(const WebString& name, const WebString& value) override;
  bool IsSafe() const { return is_safe_; }

 private:
  bool is_safe_;
};

void HTTPRequestHeaderValidator::VisitHeader(const WebString& name,
                                             const WebString& value) {
  is_safe_ = is_safe_ && IsValidHTTPToken(name) &&
             !FetchUtils::IsForbiddenHeaderName(name) &&
             IsValidHTTPHeaderValue(value);
}

}  // namespace

// This class bridges the interface differences between WebCore and WebKit
// loader clients.
// It forwards its ThreadableLoaderClient notifications to a
// WebAssociatedURLLoaderClient.
class WebAssociatedURLLoaderImpl::ClientAdapter final
    : public DocumentThreadableLoaderClient {
  WTF_MAKE_NONCOPYABLE(ClientAdapter);

 public:
  static std::unique_ptr<ClientAdapter> Create(
      WebAssociatedURLLoaderImpl*,
      WebAssociatedURLLoaderClient*,
      const WebAssociatedURLLoaderOptions&,
      WebURLRequest::FetchRequestMode,
      RefPtr<WebTaskRunner>);

  // ThreadableLoaderClient
  void DidSendData(unsigned long long /*bytesSent*/,
                   unsigned long long /*totalBytesToBeSent*/) override;
  void DidReceiveResponse(unsigned long,
                          const ResourceResponse&,
                          std::unique_ptr<WebDataConsumerHandle>) override;
  void DidDownloadData(int /*dataLength*/) override;
  void DidReceiveData(const char*, unsigned /*dataLength*/) override;
  void DidReceiveCachedMetadata(const char*, int /*dataLength*/) override;
  void DidFinishLoading(unsigned long /*identifier*/,
                        double /*finishTime*/) override;
  void DidFail(const ResourceError&) override;
  void DidFailRedirectCheck() override;

  // DocumentThreadableLoaderClient
  bool WillFollowRedirect(
      const ResourceRequest& /*newRequest*/,
      const ResourceResponse& /*redirectResponse*/) override;

  // Sets an error to be reported back to the client, asychronously.
  void SetDelayedError(const ResourceError&);

  // Enables forwarding of error notifications to the
  // WebAssociatedURLLoaderClient. These
  // must be deferred until after the call to
  // WebAssociatedURLLoader::loadAsynchronously() completes.
  void EnableErrorNotifications();

  // Stops loading and releases the DocumentThreadableLoader as early as
  // possible.
  WebAssociatedURLLoaderClient* ReleaseClient() {
    WebAssociatedURLLoaderClient* client = client_;
    client_ = nullptr;
    return client;
  }

 private:
  ClientAdapter(WebAssociatedURLLoaderImpl*,
                WebAssociatedURLLoaderClient*,
                const WebAssociatedURLLoaderOptions&,
                WebURLRequest::FetchRequestMode,
                RefPtr<WebTaskRunner>);

  void NotifyError(TimerBase*);

  WebAssociatedURLLoaderImpl* loader_;
  WebAssociatedURLLoaderClient* client_;
  WebAssociatedURLLoaderOptions options_;
  WebURLRequest::FetchRequestMode fetch_request_mode_;
  WebURLError error_;

  TaskRunnerTimer<ClientAdapter> error_timer_;
  bool enable_error_notifications_;
  bool did_fail_;
};

std::unique_ptr<WebAssociatedURLLoaderImpl::ClientAdapter>
WebAssociatedURLLoaderImpl::ClientAdapter::Create(
    WebAssociatedURLLoaderImpl* loader,
    WebAssociatedURLLoaderClient* client,
    const WebAssociatedURLLoaderOptions& options,
    WebURLRequest::FetchRequestMode fetch_request_mode,
    RefPtr<WebTaskRunner> task_runner) {
  return WTF::WrapUnique(new ClientAdapter(loader, client, options,
                                           fetch_request_mode, task_runner));
}

WebAssociatedURLLoaderImpl::ClientAdapter::ClientAdapter(
    WebAssociatedURLLoaderImpl* loader,
    WebAssociatedURLLoaderClient* client,
    const WebAssociatedURLLoaderOptions& options,
    WebURLRequest::FetchRequestMode fetch_request_mode,
    RefPtr<WebTaskRunner> task_runner)
    : loader_(loader),
      client_(client),
      options_(options),
      fetch_request_mode_(fetch_request_mode),
      error_timer_(std::move(task_runner), this, &ClientAdapter::NotifyError),
      enable_error_notifications_(false),
      did_fail_(false) {
  DCHECK(loader_);
  DCHECK(client_);
}

bool WebAssociatedURLLoaderImpl::ClientAdapter::WillFollowRedirect(
    const ResourceRequest& new_request,
    const ResourceResponse& redirect_response) {
  if (!client_)
    return true;

  WrappedResourceRequest wrapped_new_request(new_request);
  WrappedResourceResponse wrapped_redirect_response(redirect_response);
  return client_->WillFollowRedirect(wrapped_new_request,
                                     wrapped_redirect_response);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidSendData(
    unsigned long long bytes_sent,
    unsigned long long total_bytes_to_be_sent) {
  if (!client_)
    return;

  client_->DidSendData(bytes_sent, total_bytes_to_be_sent);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidReceiveResponse(
    unsigned long,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  ALLOW_UNUSED_LOCAL(handle);
  DCHECK(!handle);
  if (!client_)
    return;

  if (options_.expose_all_response_headers ||
      (fetch_request_mode_ != WebURLRequest::kFetchRequestModeCORS &&
       fetch_request_mode_ !=
           WebURLRequest::kFetchRequestModeCORSWithForcedPreflight)) {
    // Use the original ResourceResponse.
    client_->DidReceiveResponse(WrappedResourceResponse(response));
    return;
  }

  HTTPHeaderSet exposed_headers;
  CrossOriginAccessControl::ExtractCorsExposedHeaderNamesList(response,
                                                              exposed_headers);
  HTTPHeaderSet blocked_headers;
  for (const auto& header : response.HttpHeaderFields()) {
    if (FetchUtils::IsForbiddenResponseHeaderName(header.key) ||
        (!CrossOriginAccessControl::IsOnAccessControlResponseHeaderWhitelist(
             header.key) &&
         !exposed_headers.Contains(header.key)))
      blocked_headers.insert(header.key);
  }

  if (blocked_headers.IsEmpty()) {
    // Use the original ResourceResponse.
    client_->DidReceiveResponse(WrappedResourceResponse(response));
    return;
  }

  // If there are blocked headers, copy the response so we can remove them.
  WebURLResponse validated_response = WrappedResourceResponse(response);
  for (const auto& header : blocked_headers)
    validated_response.ClearHTTPHeaderField(header);
  client_->DidReceiveResponse(validated_response);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidDownloadData(
    int data_length) {
  if (!client_)
    return;

  client_->DidDownloadData(data_length);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidReceiveData(
    const char* data,
    unsigned data_length) {
  if (!client_)
    return;

  CHECK_LE(data_length, static_cast<unsigned>(std::numeric_limits<int>::max()));

  client_->DidReceiveData(data, data_length);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidReceiveCachedMetadata(
    const char* data,
    int data_length) {
  if (!client_)
    return;

  client_->DidReceiveCachedMetadata(data, data_length);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidFinishLoading(
    unsigned long identifier,
    double finish_time) {
  if (!client_)
    return;

  loader_->ClientAdapterDone();

  ReleaseClient()->DidFinishLoading(finish_time);
  // |this| may be dead here.
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidFail(
    const ResourceError& error) {
  if (!client_)
    return;

  loader_->ClientAdapterDone();

  did_fail_ = true;
  error_ = WebURLError(error);
  if (enable_error_notifications_)
    NotifyError(&error_timer_);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::DidFailRedirectCheck() {
  DidFail(ResourceError());
}

void WebAssociatedURLLoaderImpl::ClientAdapter::EnableErrorNotifications() {
  enable_error_notifications_ = true;
  // If an error has already been received, start a timer to report it to the
  // client after WebAssociatedURLLoader::loadAsynchronously has returned to the
  // caller.
  if (did_fail_)
    error_timer_.StartOneShot(0, BLINK_FROM_HERE);
}

void WebAssociatedURLLoaderImpl::ClientAdapter::NotifyError(TimerBase* timer) {
  DCHECK_EQ(timer, &error_timer_);

  if (client_)
    ReleaseClient()->DidFail(error_);
  // |this| may be dead here.
}

class WebAssociatedURLLoaderImpl::Observer final
    : public GarbageCollected<Observer>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(Observer);

 public:
  Observer(WebAssociatedURLLoaderImpl* parent, Document* document)
      : ContextLifecycleObserver(document), parent_(parent) {}

  void Dispose() {
    parent_ = nullptr;
    ClearContext();
  }

  void ContextDestroyed(ExecutionContext*) override {
    if (parent_)
      parent_->DocumentDestroyed();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { ContextLifecycleObserver::Trace(visitor); }

  WebAssociatedURLLoaderImpl* parent_;
};

WebAssociatedURLLoaderImpl::WebAssociatedURLLoaderImpl(
    Document* document,
    const WebAssociatedURLLoaderOptions& options)
    : client_(nullptr),
      options_(options),
      observer_(new Observer(this, document)) {}

WebAssociatedURLLoaderImpl::~WebAssociatedURLLoaderImpl() {
  Cancel();
}

STATIC_ASSERT_ENUM(WebAssociatedURLLoaderOptions::kConsiderPreflight,
                   kConsiderPreflight);
STATIC_ASSERT_ENUM(WebAssociatedURLLoaderOptions::kPreventPreflight,
                   kPreventPreflight);

void WebAssociatedURLLoaderImpl::LoadAsynchronously(
    const WebURLRequest& request,
    WebAssociatedURLLoaderClient* client) {
  DCHECK(!client_);
  DCHECK(!loader_);
  DCHECK(!client_adapter_);

  DCHECK(client);

  bool allow_load = true;
  WebURLRequest new_request(request);
  if (options_.untrusted_http) {
    WebString method = new_request.HttpMethod();
    allow_load = observer_ && IsValidHTTPToken(method) &&
                 !FetchUtils::IsForbiddenMethod(method);
    if (allow_load) {
      new_request.SetHTTPMethod(FetchUtils::NormalizeMethod(method));
      HTTPRequestHeaderValidator validator;
      new_request.VisitHTTPHeaderFields(&validator);
      allow_load = validator.IsSafe();
    }
  }

  RefPtr<WebTaskRunner> task_runner = TaskRunnerHelper::Get(
      TaskType::kUnspecedLoading,
      observer_ ? ToDocument(observer_->LifecycleContext()) : nullptr);
  client_ = client;
  client_adapter_ = ClientAdapter::Create(this, client, options_,
                                          request.GetFetchRequestMode(),
                                          std::move(task_runner));

  if (allow_load) {
    ThreadableLoaderOptions options;
    options.preflight_policy =
        static_cast<PreflightPolicy>(options_.preflight_policy);

    ResourceLoaderOptions resource_loader_options;
    resource_loader_options.data_buffering_policy = kDoNotBufferData;

    const ResourceRequest& webcore_request = new_request.ToResourceRequest();
    if (webcore_request.GetRequestContext() ==
        WebURLRequest::kRequestContextUnspecified) {
      // FIXME: We load URLs without setting a TargetType (and therefore a
      // request context) in several places in content/
      // (P2PPortAllocatorSession::AllocateLegacyRelaySession, for example).
      // Remove this once those places are patched up.
      new_request.SetRequestContext(WebURLRequest::kRequestContextInternal);
    }

    Document* document = ToDocument(observer_->LifecycleContext());
    DCHECK(document);
    loader_ = DocumentThreadableLoader::Create(
        *ThreadableLoadingContext::Create(*document), client_adapter_.get(),
        options, resource_loader_options);
    loader_->Start(webcore_request);
  }

  if (!loader_) {
    // FIXME: return meaningful error codes.
    client_adapter_->DidFail(ResourceError());
  }
  client_adapter_->EnableErrorNotifications();
}

void WebAssociatedURLLoaderImpl::Cancel() {
  DisposeObserver();
  CancelLoader();
  ReleaseClient();
}

void WebAssociatedURLLoaderImpl::ClientAdapterDone() {
  DisposeObserver();
  ReleaseClient();
}

void WebAssociatedURLLoaderImpl::CancelLoader() {
  if (!client_adapter_)
    return;

  // Prevent invocation of the WebAssociatedURLLoaderClient methods.
  client_adapter_->ReleaseClient();

  if (loader_) {
    loader_->Cancel();
    loader_ = nullptr;
  }
  client_adapter_.reset();
}

void WebAssociatedURLLoaderImpl::SetDefersLoading(bool defers_loading) {
  if (loader_)
    loader_->SetDefersLoading(defers_loading);
}

void WebAssociatedURLLoaderImpl::SetLoadingTaskRunner(blink::WebTaskRunner*) {
  // TODO(alexclarke): Maybe support this one day if it proves worthwhile.
}

void WebAssociatedURLLoaderImpl::DocumentDestroyed() {
  DisposeObserver();
  CancelLoader();

  if (!client_)
    return;

  ReleaseClient()->DidFail(ResourceError());
  // |this| may be dead here.
}

void WebAssociatedURLLoaderImpl::DisposeObserver() {
  if (!observer_)
    return;

  // TODO(tyoshino): Remove this assert once Document is fixed so that
  // contextDestroyed() is invoked for all kinds of Documents.
  //
  // Currently, the method of detecting Document destruction implemented here
  // doesn't work for all kinds of Documents. In case we reached here after
  // the Oilpan is destroyed, we just crash the renderer process to prevent
  // UaF.
  //
  // We could consider just skipping the rest of code in case
  // ThreadState::current() is null. However, the fact we reached here
  // without cancelling the loader means that it's possible there're some
  // non-Blink non-on-heap objects still facing on-heap Blink objects. E.g.
  // there could be a WebURLLoader instance behind the
  // DocumentThreadableLoader instance. So, for safety, we chose to just
  // crash here.
  CHECK(ThreadState::Current());

  observer_->Dispose();
  observer_ = nullptr;
}

}  // namespace blink
