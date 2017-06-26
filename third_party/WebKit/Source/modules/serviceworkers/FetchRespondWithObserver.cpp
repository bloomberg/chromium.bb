// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/FetchRespondWithObserver.h"

#include <v8.h>
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/modules/v8/V8Response.h"
#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleTypes.h"
#include "core/streams/Stream.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/BytesConsumer.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"

namespace blink {
namespace {

// Returns the error message to let the developer know about the reason of the
// unusual failures.
const String GetMessageForResponseError(WebServiceWorkerResponseError error,
                                        const KURL& request_url) {
  String error_message = "The FetchEvent for \"" + request_url.GetString() +
                         "\" resulted in a network error response: ";
  switch (error) {
    case kWebServiceWorkerResponseErrorPromiseRejected:
      error_message = error_message + "the promise was rejected.";
      break;
    case kWebServiceWorkerResponseErrorDefaultPrevented:
      error_message =
          error_message +
          "preventDefault() was called without calling respondWith().";
      break;
    case kWebServiceWorkerResponseErrorNoV8Instance:
      error_message =
          error_message +
          "an object that was not a Response was passed to respondWith().";
      break;
    case kWebServiceWorkerResponseErrorResponseTypeError:
      error_message = error_message +
                      "the promise was resolved with an error response object.";
      break;
    case kWebServiceWorkerResponseErrorResponseTypeOpaque:
      error_message =
          error_message +
          "an \"opaque\" response was used for a request whose type "
          "is not no-cors";
      break;
    case kWebServiceWorkerResponseErrorResponseTypeNotBasicOrDefault:
      NOTREACHED();
      break;
    case kWebServiceWorkerResponseErrorBodyUsed:
      error_message =
          error_message +
          "a Response whose \"bodyUsed\" is \"true\" cannot be used "
          "to respond to a request.";
      break;
    case kWebServiceWorkerResponseErrorResponseTypeOpaqueForClientRequest:
      error_message = error_message +
                      "an \"opaque\" response was used for a client request.";
      break;
    case kWebServiceWorkerResponseErrorResponseTypeOpaqueRedirect:
      error_message = error_message +
                      "an \"opaqueredirect\" type response was used for a "
                      "request whose redirect mode is not \"manual\".";
      break;
    case kWebServiceWorkerResponseErrorBodyLocked:
      error_message = error_message +
                      "a Response whose \"body\" is locked cannot be used to "
                      "respond to a request.";
      break;
    case kWebServiceWorkerResponseErrorNoForeignFetchResponse:
      error_message =
          error_message +
          "an object that was not a ForeignFetchResponse was passed "
          "to respondWith().";
      break;
    case kWebServiceWorkerResponseErrorForeignFetchHeadersWithoutOrigin:
      error_message =
          error_message +
          "headers were specified for a response without an explicit origin.";
      break;
    case kWebServiceWorkerResponseErrorForeignFetchMismatchedOrigin:
      error_message = error_message +
                      "origin in response does not match origin of request.";
      break;
    case kWebServiceWorkerResponseErrorRedirectedResponseForNotFollowRequest:
      error_message = error_message +
                      "a redirected response was used for a request whose "
                      "redirect mode is not \"follow\".";
      break;
    case kWebServiceWorkerResponseErrorUnknown:
    default:
      error_message = error_message + "an unexpected error occurred.";
      break;
  }
  return error_message;
}

bool IsNavigationRequest(WebURLRequest::FrameType frame_type) {
  return frame_type != WebURLRequest::kFrameTypeNone;
}

bool IsClientRequest(WebURLRequest::FrameType frame_type,
                     WebURLRequest::RequestContext request_context) {
  return IsNavigationRequest(frame_type) ||
         request_context == WebURLRequest::kRequestContextSharedWorker ||
         request_context == WebURLRequest::kRequestContextWorker;
}

// Notifies the result of FetchDataLoader to |handle_|. |handle_| pass through
// the result to its observer which is outside of blink.
class FetchLoaderClient final
    : public GarbageCollectedFinalized<FetchLoaderClient>,
      public FetchDataLoader::Client {
  WTF_MAKE_NONCOPYABLE(FetchLoaderClient);
  USING_GARBAGE_COLLECTED_MIXIN(FetchLoaderClient);

 public:
  explicit FetchLoaderClient(
      std::unique_ptr<WebServiceWorkerStreamHandle> handle)
      : handle_(std::move(handle)) {}

  void DidFetchDataLoadedDataPipe() override { handle_->Completed(); }
  void DidFetchDataLoadFailed() override { handle_->Aborted(); }

  DEFINE_INLINE_TRACE() { FetchDataLoader::Client::Trace(visitor); }

 private:
  std::unique_ptr<WebServiceWorkerStreamHandle> handle_;
};

}  // namespace

FetchRespondWithObserver* FetchRespondWithObserver::Create(
    ExecutionContext* context,
    int fetch_event_id,
    const KURL& request_url,
    WebURLRequest::FetchRequestMode request_mode,
    WebURLRequest::FetchRedirectMode redirect_mode,
    WebURLRequest::FrameType frame_type,
    WebURLRequest::RequestContext request_context,
    WaitUntilObserver* observer) {
  return new FetchRespondWithObserver(context, fetch_event_id, request_url,
                                      request_mode, redirect_mode, frame_type,
                                      request_context, observer);
}

void FetchRespondWithObserver::OnResponseRejected(
    WebServiceWorkerResponseError error) {
  DCHECK(GetExecutionContext());
  GetExecutionContext()->AddConsoleMessage(
      ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel,
                             GetMessageForResponseError(error, request_url_)));

  // The default value of WebServiceWorkerResponse's status is 0, which maps
  // to a network error.
  WebServiceWorkerResponse web_response;
  web_response.SetError(error);
  ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
      ->RespondToFetchEvent(event_id_, web_response, event_dispatch_time_);
}

void FetchRespondWithObserver::OnResponseFulfilled(const ScriptValue& value) {
  DCHECK(GetExecutionContext());
  if (!V8Response::hasInstance(value.V8Value(),
                               ToIsolate(GetExecutionContext()))) {
    OnResponseRejected(kWebServiceWorkerResponseErrorNoV8Instance);
    return;
  }
  Response* response = V8Response::toImplWithTypeCheck(
      ToIsolate(GetExecutionContext()), value.V8Value());
  // "If one of the following conditions is true, return a network error:
  //   - |response|'s type is |error|.
  //   - |request|'s mode is not |no-cors| and response's type is |opaque|.
  //   - |request| is a client request and |response|'s type is neither
  //     |basic| nor |default|."
  const FetchResponseData::Type response_type =
      response->GetResponse()->GetType();
  if (response_type == FetchResponseData::kErrorType) {
    OnResponseRejected(kWebServiceWorkerResponseErrorResponseTypeError);
    return;
  }
  if (response_type == FetchResponseData::kOpaqueType) {
    if (request_mode_ != WebURLRequest::kFetchRequestModeNoCORS) {
      OnResponseRejected(kWebServiceWorkerResponseErrorResponseTypeOpaque);
      return;
    }

    // The request mode of client requests should be "same-origin" but it is
    // not explicitly stated in the spec yet. So we need to check here.
    // FIXME: Set the request mode of client requests to "same-origin" and
    // remove this check when the spec will be updated.
    // Spec issue: https://github.com/whatwg/fetch/issues/101
    if (IsClientRequest(frame_type_, request_context_)) {
      OnResponseRejected(
          kWebServiceWorkerResponseErrorResponseTypeOpaqueForClientRequest);
      return;
    }
  }
  if (redirect_mode_ != WebURLRequest::kFetchRedirectModeManual &&
      response_type == FetchResponseData::kOpaqueRedirectType) {
    OnResponseRejected(
        kWebServiceWorkerResponseErrorResponseTypeOpaqueRedirect);
    return;
  }
  if (redirect_mode_ != WebURLRequest::kFetchRedirectModeFollow &&
      response->redirected()) {
    OnResponseRejected(
        kWebServiceWorkerResponseErrorRedirectedResponseForNotFollowRequest);
    return;
  }
  if (response->IsBodyLocked()) {
    OnResponseRejected(kWebServiceWorkerResponseErrorBodyLocked);
    return;
  }
  if (response->bodyUsed()) {
    OnResponseRejected(kWebServiceWorkerResponseErrorBodyUsed);
    return;
  }

  WebServiceWorkerResponse web_response;
  response->PopulateWebServiceWorkerResponse(web_response);
  BodyStreamBuffer* buffer = response->InternalBodyBuffer();
  if (buffer) {
    RefPtr<BlobDataHandle> blob_data_handle = buffer->DrainAsBlobDataHandle(
        BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize);
    if (blob_data_handle) {
      // Handle the blob response.
      web_response.SetBlobDataHandle(blob_data_handle);
      ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
          ->RespondToFetchEvent(event_id_, web_response, event_dispatch_time_);
      return;
    }
    // Handle the stream response.
    mojo::DataPipe data_pipe;
    // Temporary CHECKs to debug https://crbug.com/734978.
    CHECK(data_pipe.producer_handle.is_valid());
    CHECK(data_pipe.consumer_handle.is_valid());

    std::unique_ptr<WebServiceWorkerStreamHandle> body_stream_handle =
        WTF::MakeUnique<WebServiceWorkerStreamHandle>(
            std::move(data_pipe.consumer_handle));
    ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
        ->RespondToFetchEventWithResponseStream(event_id_, web_response,
                                                body_stream_handle.get(),
                                                event_dispatch_time_);

    buffer->StartLoading(FetchDataLoader::CreateLoaderAsDataPipe(
                             std::move(data_pipe.producer_handle)),
                         new FetchLoaderClient(std::move(body_stream_handle)));
    return;
  }
  ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
      ->RespondToFetchEvent(event_id_, web_response, event_dispatch_time_);
}

void FetchRespondWithObserver::OnNoResponse() {
  ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
      ->RespondToFetchEventWithNoResponse(event_id_, event_dispatch_time_);
}

FetchRespondWithObserver::FetchRespondWithObserver(
    ExecutionContext* context,
    int fetch_event_id,
    const KURL& request_url,
    WebURLRequest::FetchRequestMode request_mode,
    WebURLRequest::FetchRedirectMode redirect_mode,
    WebURLRequest::FrameType frame_type,
    WebURLRequest::RequestContext request_context,
    WaitUntilObserver* observer)
    : RespondWithObserver(context, fetch_event_id, observer),
      request_url_(request_url),
      request_mode_(request_mode),
      redirect_mode_(redirect_mode),
      frame_type_(frame_type),
      request_context_(request_context) {}

DEFINE_TRACE(FetchRespondWithObserver) {
  RespondWithObserver::Trace(visitor);
}

}  // namespace blink
