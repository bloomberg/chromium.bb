// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchManager.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/frame/Frame.h"
#include "core/frame/SubresourceIntegrity.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/probe/CoreProbes.h"
#include "modules/fetch/Body.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/BytesConsumer.h"
#include "modules/fetch/BytesConsumerForDataConsumerHandle.h"
#include "modules/fetch/FetchRequestData.h"
#include "modules/fetch/FormDataBytesConsumer.h"
#include "modules/fetch/Response.h"
#include "modules/fetch/ResponseInit.h"
#include "platform/HTTPNames.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/loader/fetch/CrossOriginAccessControl.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/network/NetworkUtils.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

namespace {

class SRIBytesConsumer final : public BytesConsumer {
 public:
  // BytesConsumer implementation
  Result BeginRead(const char** buffer, size_t* available) override {
    if (!underlying_) {
      *buffer = nullptr;
      *available = 0;
      return is_cancelled_ ? Result::kDone : Result::kShouldWait;
    }
    return underlying_->BeginRead(buffer, available);
  }
  Result EndRead(size_t read_size) override {
    DCHECK(underlying_);
    return underlying_->EndRead(read_size);
  }
  PassRefPtr<BlobDataHandle> DrainAsBlobDataHandle(
      BlobSizePolicy policy) override {
    return underlying_ ? underlying_->DrainAsBlobDataHandle(policy) : nullptr;
  }
  PassRefPtr<EncodedFormData> DrainAsFormData() override {
    return underlying_ ? underlying_->DrainAsFormData() : nullptr;
  }
  void SetClient(BytesConsumer::Client* client) override {
    DCHECK(!client_);
    DCHECK(client);
    if (underlying_)
      underlying_->SetClient(client);
    else
      client_ = client;
  }
  void ClearClient() override {
    if (underlying_)
      underlying_->ClearClient();
    else
      client_ = nullptr;
  }
  void Cancel() override {
    if (underlying_) {
      underlying_->Cancel();
    } else {
      is_cancelled_ = true;
      client_ = nullptr;
    }
  }
  PublicState GetPublicState() const override {
    return underlying_ ? underlying_->GetPublicState()
                       : is_cancelled_ ? PublicState::kClosed
                                       : PublicState::kReadableOrWaiting;
  }
  Error GetError() const override {
    DCHECK(underlying_);
    // We must not be in the errored state until we get updated.
    return underlying_->GetError();
  }
  String DebugName() const override { return "SRIBytesConsumer"; }

  // This function can be called at most once.
  void Update(BytesConsumer* consumer) {
    DCHECK(!underlying_);
    if (is_cancelled_) {
      // This consumer has already been closed.
      return;
    }

    underlying_ = consumer;
    if (client_) {
      Client* client = client_;
      client_ = nullptr;
      underlying_->SetClient(client);
      if (GetPublicState() != PublicState::kReadableOrWaiting)
        client->OnStateChange();
    }
  }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(underlying_);
    visitor->Trace(client_);
    BytesConsumer::Trace(visitor);
  }

 private:
  Member<BytesConsumer> underlying_;
  Member<Client> client_;
  bool is_cancelled_ = false;
};

}  // namespace

class FetchManager::Loader final
    : public GarbageCollectedFinalized<FetchManager::Loader>,
      public ThreadableLoaderClient {
  USING_PRE_FINALIZER(FetchManager::Loader, Dispose);

 public:
  static Loader* Create(ExecutionContext* execution_context,
                        FetchManager* fetch_manager,
                        ScriptPromiseResolver* resolver,
                        FetchRequestData* request,
                        bool is_isolated_world) {
    return new Loader(execution_context, fetch_manager, resolver, request,
                      is_isolated_world);
  }

  ~Loader() override;
  DECLARE_VIRTUAL_TRACE();

  void DidReceiveRedirectTo(const KURL&) override;
  void DidReceiveResponse(unsigned long,
                          const ResourceResponse&,
                          std::unique_ptr<WebDataConsumerHandle>) override;
  void DidFinishLoading(unsigned long, double) override;
  void DidFail(const ResourceError&) override;
  void DidFailAccessControlCheck(const ResourceError&) override;
  void DidFailRedirectCheck() override;

  void Start();
  void Dispose();

  class SRIVerifier final : public GarbageCollectedFinalized<SRIVerifier>,
                            public WebDataConsumerHandle::Client {
   public:
    // Promptly clear m_handle and m_reader.
    EAGERLY_FINALIZE();
    // SRIVerifier takes ownership of |handle| and |response|.
    // |updater| must be garbage collected. The other arguments
    // all must have the lifetime of the give loader.
    SRIVerifier(std::unique_ptr<WebDataConsumerHandle> handle,
                SRIBytesConsumer* updater,
                Response* response,
                FetchManager::Loader* loader,
                String integrity_metadata,
                const KURL& url)
        : handle_(std::move(handle)),
          updater_(updater),
          response_(response),
          loader_(loader),
          integrity_metadata_(integrity_metadata),
          url_(url),
          finished_(false) {
      reader_ = handle_->ObtainReader(this);
    }

    void DidGetReadable() override {
      DCHECK(reader_);
      DCHECK(loader_);
      DCHECK(response_);

      WebDataConsumerHandle::Result r = WebDataConsumerHandle::kOk;
      while (r == WebDataConsumerHandle::kOk) {
        const void* buffer;
        size_t size;
        r = reader_->BeginRead(&buffer, WebDataConsumerHandle::kFlagNone,
                               &size);
        if (r == WebDataConsumerHandle::kOk) {
          buffer_.Append(static_cast<const char*>(buffer), size);
          reader_->EndRead(size);
        }
      }
      if (r == WebDataConsumerHandle::kShouldWait)
        return;
      String error_message =
          "Unknown error occurred while trying to verify integrity.";
      finished_ = true;
      if (r == WebDataConsumerHandle::kDone) {
        if (SubresourceIntegrity::CheckSubresourceIntegrity(
                integrity_metadata_, buffer_.data(), buffer_.size(), url_,
                *loader_->GetExecutionContext(), error_message)) {
          updater_->Update(
              new FormDataBytesConsumer(buffer_.data(), buffer_.size()));
          loader_->resolver_->Resolve(response_);
          loader_->resolver_.Clear();
          // FetchManager::Loader::didFinishLoading() can
          // be called before didGetReadable() is called
          // when the data is ready. In that case,
          // didFinishLoading() doesn't clean up and call
          // notifyFinished(), so it is necessary to
          // explicitly finish the loader here.
          if (loader_->did_finish_loading_)
            loader_->LoadSucceeded();
          return;
        }
      }
      updater_->Update(
          BytesConsumer::CreateErrored(BytesConsumer::Error(error_message)));
      loader_->PerformNetworkError(error_message);
    }

    bool IsFinished() const { return finished_; }

    DEFINE_INLINE_TRACE() {
      visitor->Trace(updater_);
      visitor->Trace(response_);
      visitor->Trace(loader_);
    }

   private:
    std::unique_ptr<WebDataConsumerHandle> handle_;
    Member<SRIBytesConsumer> updater_;
    // We cannot store a Response because its JS wrapper can be collected.
    // TODO(yhirano): Fix this.
    Member<Response> response_;
    Member<FetchManager::Loader> loader_;
    String integrity_metadata_;
    KURL url_;
    std::unique_ptr<WebDataConsumerHandle::Reader> reader_;
    Vector<char> buffer_;
    bool finished_;
  };

 private:
  Loader(ExecutionContext*,
         FetchManager*,
         ScriptPromiseResolver*,
         FetchRequestData*,
         bool is_isolated_world);

  void PerformSchemeFetch();
  void PerformNetworkError(const String& message);
  void PerformHTTPFetch(bool cors_flag, bool cors_preflight_flag);
  void PerformDataFetch();
  void Failed(const String& message);
  void NotifyFinished();
  Document* GetDocument() const;
  ExecutionContext* GetExecutionContext() { return execution_context_; }
  void LoadSucceeded();

  Member<FetchManager> fetch_manager_;
  Member<ScriptPromiseResolver> resolver_;
  Member<FetchRequestData> request_;
  Member<ThreadableLoader> loader_;
  bool failed_;
  bool finished_;
  int response_http_status_code_;
  Member<SRIVerifier> integrity_verifier_;
  bool did_finish_loading_;
  bool is_isolated_world_;
  Vector<KURL> url_list_;
  Member<ExecutionContext> execution_context_;
};

FetchManager::Loader::Loader(ExecutionContext* execution_context,
                             FetchManager* fetch_manager,
                             ScriptPromiseResolver* resolver,
                             FetchRequestData* request,
                             bool is_isolated_world)
    : fetch_manager_(fetch_manager),
      resolver_(resolver),
      request_(request),
      failed_(false),
      finished_(false),
      response_http_status_code_(0),
      integrity_verifier_(nullptr),
      did_finish_loading_(false),
      is_isolated_world_(is_isolated_world),
      execution_context_(execution_context) {
  url_list_.push_back(request->Url());
}

FetchManager::Loader::~Loader() {
  DCHECK(!loader_);
}

DEFINE_TRACE(FetchManager::Loader) {
  visitor->Trace(fetch_manager_);
  visitor->Trace(resolver_);
  visitor->Trace(request_);
  visitor->Trace(loader_);
  visitor->Trace(integrity_verifier_);
  visitor->Trace(execution_context_);
}

void FetchManager::Loader::DidReceiveRedirectTo(const KURL& url) {
  url_list_.push_back(url);
}

void FetchManager::Loader::DidReceiveResponse(
    unsigned long,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(handle);
  // TODO(horo): This check could be false when we will use the response url
  // in service worker responses. (crbug.com/553535)
  DCHECK(response.Url() == url_list_.back());
  ScriptState* script_state = resolver_->GetScriptState();
  ScriptState::Scope scope(script_state);

  if (response.Url().ProtocolIs("blob") && response.HttpStatusCode() == 404) {
    // "If |blob| is null, return a network error."
    // https://fetch.spec.whatwg.org/#concept-scheme-fetch
    PerformNetworkError("Blob not found.");
    return;
  }

  if (response.Url().ProtocolIs("blob") && response.HttpStatusCode() == 405) {
    PerformNetworkError("Only 'GET' method is allowed for blob URLs.");
    return;
  }

  response_http_status_code_ = response.HttpStatusCode();
  FetchRequestData::Tainting tainting = request_->ResponseTainting();

  if (response.Url().ProtocolIsData()) {
    if (request_->Url() == response.Url()) {
      // A direct request to data.
      tainting = FetchRequestData::kBasicTainting;
    } else {
      // A redirect to data: scheme occured.
      // Redirects to data URLs are rejected by the spec because
      // same-origin data-URL flag is unset, except for no-cors mode.
      // TODO(hiroshige): currently redirects to data URLs in no-cors
      // mode is also rejected by Chromium side.
      switch (request_->Mode()) {
        case WebURLRequest::kFetchRequestModeNoCORS:
          tainting = FetchRequestData::kOpaqueTainting;
          break;
        case WebURLRequest::kFetchRequestModeSameOrigin:
        case WebURLRequest::kFetchRequestModeCORS:
        case WebURLRequest::kFetchRequestModeCORSWithForcedPreflight:
        case WebURLRequest::kFetchRequestModeNavigate:
          PerformNetworkError("Fetch API cannot load " +
                              request_->Url().GetString() +
                              ". Redirects to data: URL are allowed only when "
                              "mode is \"no-cors\".");
          return;
      }
    }
  } else if (!SecurityOrigin::Create(response.Url())
                  ->IsSameSchemeHostPort(request_->Origin().Get())) {
    // Recompute the tainting if the request was redirected to a different
    // origin.
    switch (request_->Mode()) {
      case WebURLRequest::kFetchRequestModeSameOrigin:
        NOTREACHED();
        break;
      case WebURLRequest::kFetchRequestModeNoCORS:
        tainting = FetchRequestData::kOpaqueTainting;
        break;
      case WebURLRequest::kFetchRequestModeCORS:
      case WebURLRequest::kFetchRequestModeCORSWithForcedPreflight:
        tainting = FetchRequestData::kCORSTainting;
        break;
      case WebURLRequest::kFetchRequestModeNavigate:
        LOG(FATAL);
        break;
    }
  }
  if (response.WasFetchedViaServiceWorker()) {
    switch (response.ServiceWorkerResponseType()) {
      case kWebServiceWorkerResponseTypeBasic:
      case kWebServiceWorkerResponseTypeDefault:
        tainting = FetchRequestData::kBasicTainting;
        break;
      case kWebServiceWorkerResponseTypeCORS:
        tainting = FetchRequestData::kCORSTainting;
        break;
      case kWebServiceWorkerResponseTypeOpaque:
        tainting = FetchRequestData::kOpaqueTainting;
        break;
      case kWebServiceWorkerResponseTypeOpaqueRedirect:
        DCHECK(
            NetworkUtils::IsRedirectResponseCode(response_http_status_code_));
        break;  // The code below creates an opaque-redirect filtered response.
      case kWebServiceWorkerResponseTypeError:
        LOG(FATAL) << "When ServiceWorker respond to the request from fetch() "
                      "with an error response, FetchManager::Loader::didFail() "
                      "must be called instead.";
        break;
    }
  }

  FetchResponseData* response_data = nullptr;
  SRIBytesConsumer* sri_consumer = nullptr;
  if (request_->Integrity().IsEmpty()) {
    response_data = FetchResponseData::CreateWithBuffer(new BodyStreamBuffer(
        script_state,
        new BytesConsumerForDataConsumerHandle(
            ExecutionContext::From(script_state), std::move(handle))));
  } else {
    sri_consumer = new SRIBytesConsumer();
    response_data = FetchResponseData::CreateWithBuffer(
        new BodyStreamBuffer(script_state, sri_consumer));
  }
  response_data->SetStatus(response.HttpStatusCode());
  response_data->SetStatusMessage(response.HttpStatusText());
  for (auto& it : response.HttpHeaderFields())
    response_data->HeaderList()->Append(it.key, it.value);
  if (response.UrlListViaServiceWorker().IsEmpty()) {
    // Note: |urlListViaServiceWorker| is empty, unless the response came from a
    // service worker, in which case it will only be empty if it was created
    // through new Response().
    response_data->SetURLList(url_list_);
  } else {
    DCHECK(response.WasFetchedViaServiceWorker());
    response_data->SetURLList(response.UrlListViaServiceWorker());
  }
  response_data->SetMIMEType(response.MimeType());
  response_data->SetResponseTime(response.ResponseTime());

  FetchResponseData* tainted_response = nullptr;

  DCHECK(!(NetworkUtils::IsRedirectResponseCode(response_http_status_code_) &&
           response_data->HeaderList()->Has(HTTPNames::Location) &&
           request_->Redirect() != WebURLRequest::kFetchRedirectModeManual));

  if (NetworkUtils::IsRedirectResponseCode(response_http_status_code_) &&
      request_->Redirect() == WebURLRequest::kFetchRedirectModeManual) {
    tainted_response = response_data->CreateOpaqueRedirectFilteredResponse();
  } else {
    switch (tainting) {
      case FetchRequestData::kBasicTainting:
        tainted_response = response_data->CreateBasicFilteredResponse();
        break;
      case FetchRequestData::kCORSTainting: {
        HTTPHeaderSet header_names;
        CrossOriginAccessControl::ExtractCorsExposedHeaderNamesList(
            response, header_names);
        tainted_response =
            response_data->CreateCORSFilteredResponse(header_names);
        break;
      }
      case FetchRequestData::kOpaqueTainting:
        tainted_response = response_data->CreateOpaqueFilteredResponse();
        break;
    }
  }

  Response* r =
      Response::Create(resolver_->GetExecutionContext(), tainted_response);
  if (response.Url().ProtocolIsData()) {
    // An "Access-Control-Allow-Origin" header is added for data: URLs
    // but no headers except for "Content-Type" should exist,
    // according to the spec:
    // https://fetch.spec.whatwg.org/#concept-scheme-fetch
    // "... return a response whose header list consist of a single header
    //  whose name is `Content-Type` and value is the MIME type and
    //  parameters returned from obtaining a resource"
    r->headers()->HeaderList()->Remove(HTTPNames::Access_Control_Allow_Origin);
  }
  r->headers()->SetGuard(Headers::kImmutableGuard);

  if (request_->Integrity().IsEmpty()) {
    resolver_->Resolve(r);
    resolver_.Clear();
  } else {
    DCHECK(!integrity_verifier_);
    integrity_verifier_ =
        new SRIVerifier(std::move(handle), sri_consumer, r, this,
                        request_->Integrity(), response.Url());
  }
}

void FetchManager::Loader::DidFinishLoading(unsigned long, double) {
  did_finish_loading_ = true;
  // If there is an integrity verifier, and it has not already finished, it
  // will take care of finishing the load or performing a network error when
  // verification is complete.
  if (integrity_verifier_ && !integrity_verifier_->IsFinished())
    return;

  LoadSucceeded();
}

void FetchManager::Loader::DidFail(const ResourceError& error) {
  if (error.IsCancellation() || error.IsTimeout() ||
      error.Domain() != kErrorDomainBlinkInternal)
    Failed(String());
  else
    Failed("Fetch API cannot load " + error.FailingURL() + ". " +
           error.LocalizedDescription());
}

void FetchManager::Loader::DidFailAccessControlCheck(
    const ResourceError& error) {
  if (error.IsCancellation() || error.IsTimeout() ||
      error.Domain() != kErrorDomainBlinkInternal)
    Failed(String());
  else
    Failed("Fetch API cannot load " + error.FailingURL() + ". " +
           error.LocalizedDescription());
}

void FetchManager::Loader::DidFailRedirectCheck() {
  Failed("Fetch API cannot load " + request_->Url().GetString() +
         ". Redirect failed.");
}

Document* FetchManager::Loader::GetDocument() const {
  if (execution_context_->IsDocument()) {
    return ToDocument(execution_context_);
  }
  return nullptr;
}

void FetchManager::Loader::LoadSucceeded() {
  DCHECK(!failed_);

  finished_ = true;

  if (GetDocument() && GetDocument()->GetFrame() &&
      GetDocument()->GetFrame()->GetPage() &&
      FetchUtils::IsOkStatus(response_http_status_code_)) {
    GetDocument()->GetFrame()->GetPage()->GetChromeClient().AjaxSucceeded(
        GetDocument()->GetFrame());
  }
  probe::didFinishFetch(execution_context_, this, request_->Method(),
                        request_->Url().GetString());
  NotifyFinished();
}

void FetchManager::Loader::Start() {
  // "1. If |request|'s url contains a Known HSTS Host, modify it per the
  // requirements of the 'URI [sic] Loading and Port Mapping' chapter of HTTP
  // Strict Transport Security."
  // FIXME: Implement this.

  // "2. If |request|'s referrer is not none, set |request|'s referrer to the
  // result of invoking determine |request|'s referrer."
  // We set the referrer using workerGlobalScope's URL in
  // WorkerThreadableLoader.

  // "3. If |request|'s synchronous flag is unset and fetch is not invoked
  // recursively, run the remaining steps asynchronously."
  // We don't support synchronous flag.

  // "4. Let response be the value corresponding to the first matching
  // statement:"

  // "- should fetching |request| be blocked as mixed content returns blocked"
  // We do mixed content checking in ResourceFetcher.

  // "- should fetching |request| be blocked as content security returns
  //    blocked"
  if (!ContentSecurityPolicy::ShouldBypassMainWorld(execution_context_) &&
      !execution_context_->GetContentSecurityPolicy()->AllowConnectToSource(
          request_->Url())) {
    // "A network error."
    PerformNetworkError(
        "Refused to connect to '" + request_->Url().ElidedString() +
        "' because it violates the document's Content Security Policy.");
    return;
  }

  // "- |request|'s url's origin is |request|'s origin and the |CORS flag| is
  //    unset"
  // "- |request|'s url's scheme is 'data' and |request|'s same-origin data
  //    URL flag is set"
  // "- |request|'s url's scheme is 'about'"
  // Note we don't support to call this method with |CORS flag|
  // "- |request|'s mode is |navigate|".
  if ((SecurityOrigin::Create(request_->Url())
           ->IsSameSchemeHostPortAndSuborigin(request_->Origin().Get())) ||
      (request_->Url().ProtocolIsData() && request_->SameOriginDataURLFlag()) ||
      (request_->Url().ProtocolIsAbout()) ||
      (request_->Mode() == WebURLRequest::kFetchRequestModeNavigate)) {
    // "The result of performing a scheme fetch using request."
    PerformSchemeFetch();
    return;
  }

  // "- |request|'s mode is |same-origin|"
  if (request_->Mode() == WebURLRequest::kFetchRequestModeSameOrigin) {
    // "A network error."
    PerformNetworkError("Fetch API cannot load " + request_->Url().GetString() +
                        ". Request mode is \"same-origin\" but the URL\'s "
                        "origin is not same as the request origin " +
                        request_->Origin()->ToString() + ".");
    return;
  }

  // "- |request|'s mode is |no CORS|"
  if (request_->Mode() == WebURLRequest::kFetchRequestModeNoCORS) {
    // "Set |request|'s response tainting to |opaque|."
    request_->SetResponseTainting(FetchRequestData::kOpaqueTainting);
    // "The result of performing a scheme fetch using |request|."
    PerformSchemeFetch();
    return;
  }

  // "- |request|'s url's scheme is not one of 'http' and 'https'"
  // This may include other HTTP-like schemes if the embedder has added them
  // to SchemeRegistry::registerURLSchemeAsSupportingFetchAPI.
  if (!SchemeRegistry::ShouldTreatURLSchemeAsSupportingFetchAPI(
          request_->Url().Protocol())) {
    // "A network error."
    PerformNetworkError(
        "Fetch API cannot load " + request_->Url().GetString() +
        ". URL scheme must be \"http\" or \"https\" for CORS request.");
    return;
  }

  // "- |request|'s mode is |CORS-with-forced-preflight|."
  // "- |request|'s unsafe request flag is set and either |request|'s method
  // is not a CORS-safelisted method or a header in |request|'s header list is
  // not a CORS-safelisted header"
  if (request_->Mode() ==
          WebURLRequest::kFetchRequestModeCORSWithForcedPreflight ||
      (request_->UnsafeRequestFlag() &&
       (!FetchUtils::IsCORSSafelistedMethod(request_->Method()) ||
        request_->HeaderList()->ContainsNonCORSSafelistedHeader()))) {
    // "Set |request|'s response tainting to |CORS|."
    request_->SetResponseTainting(FetchRequestData::kCORSTainting);
    // "The result of performing an HTTP fetch using |request| with the
    // |CORS flag| and |CORS preflight flag| set."
    PerformHTTPFetch(true, true);
    return;
  }

  // "- Otherwise
  //     Set |request|'s response tainting to |CORS|."
  request_->SetResponseTainting(FetchRequestData::kCORSTainting);
  // "The result of performing an HTTP fetch using |request| with the
  // |CORS flag| set."
  PerformHTTPFetch(true, false);
}

void FetchManager::Loader::Dispose() {
  probe::detachClientRequest(execution_context_, this);
  // Prevent notification
  fetch_manager_ = nullptr;
  if (loader_) {
    loader_->Cancel();
    loader_ = nullptr;
  }
  execution_context_ = nullptr;
}

void FetchManager::Loader::PerformSchemeFetch() {
  // "To perform a scheme fetch using |request|, switch on |request|'s url's
  // scheme, and run the associated steps:"
  if (SchemeRegistry::ShouldTreatURLSchemeAsSupportingFetchAPI(
          request_->Url().Protocol())) {
    // "Return the result of performing an HTTP fetch using |request|."
    PerformHTTPFetch(false, false);
  } else if (request_->Url().ProtocolIsData()) {
    PerformDataFetch();
  } else if (request_->Url().ProtocolIs("blob")) {
    PerformHTTPFetch(false, false);
  } else {
    // FIXME: implement other protocols.
    PerformNetworkError("Fetch API cannot load " + request_->Url().GetString() +
                        ". URL scheme \"" + request_->Url().Protocol() +
                        "\" is not supported.");
  }
}

void FetchManager::Loader::PerformNetworkError(const String& message) {
  Failed(message);
}

void FetchManager::Loader::PerformHTTPFetch(bool cors_flag,
                                            bool cors_preflight_flag) {
  DCHECK(SchemeRegistry::ShouldTreatURLSchemeAsSupportingFetchAPI(
             request_->Url().Protocol()) ||
         (request_->Url().ProtocolIs("blob") && !cors_flag &&
          !cors_preflight_flag));
  // CORS preflight fetch procedure is implemented inside
  // DocumentThreadableLoader.

  // "1. Let |HTTPRequest| be a copy of |request|, except that |HTTPRequest|'s
  //  body is a tee of |request|'s body."
  // We use ResourceRequest class for HTTPRequest.
  // FIXME: Support body.
  ResourceRequest request(request_->Url());
  request.SetRequestContext(request_->Context());
  request.SetHTTPMethod(request_->Method());

  switch (request_->Mode()) {
    case WebURLRequest::kFetchRequestModeSameOrigin:
    case WebURLRequest::kFetchRequestModeNoCORS:
      request.SetFetchRequestMode(request_->Mode());
      break;
    case WebURLRequest::kFetchRequestModeCORS:
    case WebURLRequest::kFetchRequestModeCORSWithForcedPreflight:
      // TODO(tyoshino): Use only the flag or the mode enum inside the
      // FetchManager. Currently both are used due to ongoing refactoring.
      // See http://crbug.com/727596.
      if (cors_preflight_flag) {
        request.SetFetchRequestMode(
            WebURLRequest::kFetchRequestModeCORSWithForcedPreflight);
      } else {
        request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeCORS);
      }
      break;
    case WebURLRequest::kFetchRequestModeNavigate:
      // Using kFetchRequestModeSameOrigin here to reduce the security risk.
      // "navigate" request is only available in ServiceWorker.
      request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeSameOrigin);
      break;
  }

  request.SetFetchCredentialsMode(request_->Credentials());
  for (const auto& header : request_->HeaderList()->List()) {
    request.AddHTTPHeaderField(AtomicString(header.first),
                               AtomicString(header.second));
  }

  if (request_->Method() != HTTPNames::GET &&
      request_->Method() != HTTPNames::HEAD) {
    if (request_->Buffer())
      request.SetHTTPBody(request_->Buffer()->DrainAsFormData());
    if (request_->AttachedCredential())
      request.SetAttachedCredential(request_->AttachedCredential());
  }
  request.SetFetchRedirectMode(request_->Redirect());
  request.SetUseStreamOnResponse(true);
  request.SetExternalRequestStateFromRequestorAddressSpace(
      execution_context_->GetSecurityContext().AddressSpace());

  // "2. Append `Referer`/empty byte sequence, if |HTTPRequest|'s |referrer|
  // is none, and `Referer`/|HTTPRequest|'s referrer, serialized and utf-8
  // encoded, otherwise, to HTTPRequest's header list.
  //
  // The following code also invokes "determine request's referrer" which is
  // written in "Main fetch" operation.
  const ReferrerPolicy referrer_policy =
      request_->GetReferrerPolicy() == kReferrerPolicyDefault
          ? execution_context_->GetReferrerPolicy()
          : request_->GetReferrerPolicy();
  const String referrer_string =
      request_->ReferrerString() == FetchRequestData::ClientReferrerString()
          ? execution_context_->OutgoingReferrer()
          : request_->ReferrerString();
  // Note that generateReferrer generates |no-referrer| from |no-referrer|
  // referrer string (i.e. String()).
  request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
      referrer_policy, request_->Url(), referrer_string));
  request.SetServiceWorkerMode(is_isolated_world_
                                   ? WebURLRequest::ServiceWorkerMode::kNone
                                   : WebURLRequest::ServiceWorkerMode::kAll);

  // "3. Append `Host`, ..."
  // FIXME: Implement this when the spec is fixed.

  // "4.If |HTTPRequest|'s force Origin header flag is set, append `Origin`/
  // |HTTPRequest|'s origin, serialized and utf-8 encoded, to |HTTPRequest|'s
  // header list."
  // We set Origin header in updateRequestForAccessControl() called from
  // DocumentThreadableLoader::makeCrossOriginAccessRequest

  // "5. Let |credentials flag| be set if either |HTTPRequest|'s credentials
  // mode is |include|, or |HTTPRequest|'s credentials mode is |same-origin|
  // and the |CORS flag| is unset, and unset otherwise."

  ResourceLoaderOptions resource_loader_options;
  resource_loader_options.data_buffering_policy = kDoNotBufferData;
  resource_loader_options.security_origin = request_->Origin().Get();

  ThreadableLoaderOptions threadable_loader_options;

  probe::willStartFetch(execution_context_, this);
  loader_ = ThreadableLoader::Create(*execution_context_, this,
                                     threadable_loader_options,
                                     resource_loader_options);
  loader_->Start(request);
}

// performDataFetch() is almost the same as performHTTPFetch(), except for:
// - We set AllowCrossOriginRequests to allow requests to data: URLs in
//   'same-origin' mode.
// - We reject non-GET method.
void FetchManager::Loader::PerformDataFetch() {
  DCHECK(request_->Url().ProtocolIsData());

  ResourceRequest request(request_->Url());
  request.SetRequestContext(request_->Context());
  request.SetUseStreamOnResponse(true);
  request.SetHTTPMethod(request_->Method());
  request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);
  request.SetFetchRedirectMode(WebURLRequest::kFetchRedirectModeError);
  // We intentionally skip 'setExternalRequestStateFromRequestorAddressSpace',
  // as 'data:' can never be external.

  ResourceLoaderOptions resource_loader_options;
  resource_loader_options.data_buffering_policy = kDoNotBufferData;
  resource_loader_options.security_origin = request_->Origin().Get();

  ThreadableLoaderOptions threadable_loader_options;

  probe::willStartFetch(execution_context_, this);
  loader_ = ThreadableLoader::Create(*execution_context_, this,
                                     threadable_loader_options,
                                     resource_loader_options);
  loader_->Start(request);
}

void FetchManager::Loader::Failed(const String& message) {
  if (failed_ || finished_)
    return;
  failed_ = true;
  if (execution_context_->IsContextDestroyed())
    return;
  if (!message.IsEmpty())
    execution_context_->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kErrorMessageLevel, message));
  if (resolver_) {
    ScriptState* state = resolver_->GetScriptState();
    ScriptState::Scope scope(state);
    resolver_->Reject(V8ThrowException::CreateTypeError(state->GetIsolate(),
                                                        "Failed to fetch"));
  }
  probe::didFailFetch(execution_context_, this);
  NotifyFinished();
}

void FetchManager::Loader::NotifyFinished() {
  if (fetch_manager_)
    fetch_manager_->OnLoaderFinished(this);
}

FetchManager* FetchManager::Create(ExecutionContext* execution_context) {
  return new FetchManager(execution_context);
}

FetchManager::FetchManager(ExecutionContext* execution_context)
    : ContextLifecycleObserver(execution_context) {}

ScriptPromise FetchManager::Fetch(ScriptState* script_state,
                                  FetchRequestData* request) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  request->SetContext(WebURLRequest::kRequestContextFetch);

  Loader* loader =
      Loader::Create(GetExecutionContext(), this, resolver, request,
                     script_state->World().IsIsolatedWorld());
  loaders_.insert(loader);
  loader->Start();
  return promise;
}

void FetchManager::ContextDestroyed(ExecutionContext*) {
  for (auto& loader : loaders_)
    loader->Dispose();
}

void FetchManager::OnLoaderFinished(Loader* loader) {
  loaders_.erase(loader);
  loader->Dispose();
}

DEFINE_TRACE(FetchManager) {
  visitor->Trace(loaders_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
