// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/FetchRespondWithObserver.h"

#include <v8.h>
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
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
const String getMessageForResponseError(WebServiceWorkerResponseError error,
                                        const KURL& requestURL) {
  String errorMessage = "The FetchEvent for \"" + requestURL.getString() +
                        "\" resulted in a network error response: ";
  switch (error) {
    case WebServiceWorkerResponseErrorPromiseRejected:
      errorMessage = errorMessage + "the promise was rejected.";
      break;
    case WebServiceWorkerResponseErrorDefaultPrevented:
      errorMessage =
          errorMessage +
          "preventDefault() was called without calling respondWith().";
      break;
    case WebServiceWorkerResponseErrorNoV8Instance:
      errorMessage =
          errorMessage +
          "an object that was not a Response was passed to respondWith().";
      break;
    case WebServiceWorkerResponseErrorResponseTypeError:
      errorMessage = errorMessage +
                     "the promise was resolved with an error response object.";
      break;
    case WebServiceWorkerResponseErrorResponseTypeOpaque:
      errorMessage = errorMessage +
                     "an \"opaque\" response was used for a request whose type "
                     "is not no-cors";
      break;
    case WebServiceWorkerResponseErrorResponseTypeNotBasicOrDefault:
      NOTREACHED();
      break;
    case WebServiceWorkerResponseErrorBodyUsed:
      errorMessage = errorMessage +
                     "a Response whose \"bodyUsed\" is \"true\" cannot be used "
                     "to respond to a request.";
      break;
    case WebServiceWorkerResponseErrorResponseTypeOpaqueForClientRequest:
      errorMessage = errorMessage +
                     "an \"opaque\" response was used for a client request.";
      break;
    case WebServiceWorkerResponseErrorResponseTypeOpaqueRedirect:
      errorMessage = errorMessage +
                     "an \"opaqueredirect\" type response was used for a "
                     "request whose redirect mode is not \"manual\".";
      break;
    case WebServiceWorkerResponseErrorBodyLocked:
      errorMessage = errorMessage +
                     "a Response whose \"body\" is locked cannot be used to "
                     "respond to a request.";
      break;
    case WebServiceWorkerResponseErrorNoForeignFetchResponse:
      errorMessage = errorMessage +
                     "an object that was not a ForeignFetchResponse was passed "
                     "to respondWith().";
      break;
    case WebServiceWorkerResponseErrorForeignFetchHeadersWithoutOrigin:
      errorMessage =
          errorMessage +
          "headers were specified for a response without an explicit origin.";
      break;
    case WebServiceWorkerResponseErrorForeignFetchMismatchedOrigin:
      errorMessage =
          errorMessage + "origin in response does not match origin of request.";
      break;
    case WebServiceWorkerResponseErrorRedirectedResponseForNotFollowRequest:
      errorMessage = errorMessage +
                     "a redirected response was used for a request whose "
                     "redirect mode is not \"follow\".";
      break;
    case WebServiceWorkerResponseErrorUnknown:
    default:
      errorMessage = errorMessage + "an unexpected error occurred.";
      break;
  }
  return errorMessage;
}

const String getErrorMessageForRedirectedResponseForNavigationRequest(
    const KURL& requestURL,
    const Vector<KURL>& responseURLList) {
  String errorMessage =
      "In Chrome 59, the navigation to \"" + requestURL.getString() + "\" " +
      "will result in a network error, because FetchEvent.respondWith() was " +
      "called with a redirected response. See https://crbug.com/658249. The " +
      "url list of the response was: [\"" + responseURLList[0].getString() +
      "\"";
  for (size_t i = 1; i < responseURLList.size(); ++i) {
    errorMessage =
        errorMessage + ", \"" + responseURLList[i].getString() + "\"";
  }
  return errorMessage + "]";
}

bool isNavigationRequest(WebURLRequest::FrameType frameType) {
  return frameType != WebURLRequest::FrameTypeNone;
}

bool isClientRequest(WebURLRequest::FrameType frameType,
                     WebURLRequest::RequestContext requestContext) {
  return isNavigationRequest(frameType) ||
         requestContext == WebURLRequest::RequestContextSharedWorker ||
         requestContext == WebURLRequest::RequestContextWorker;
}

class NoopLoaderClient final
    : public GarbageCollectedFinalized<NoopLoaderClient>,
      public FetchDataLoader::Client {
  WTF_MAKE_NONCOPYABLE(NoopLoaderClient);
  USING_GARBAGE_COLLECTED_MIXIN(NoopLoaderClient);

 public:
  NoopLoaderClient() = default;
  void didFetchDataLoadedStream() override {}
  void didFetchDataLoadFailed() override {}
  DEFINE_INLINE_TRACE() { FetchDataLoader::Client::trace(visitor); }
};

}  // namespace

FetchRespondWithObserver* FetchRespondWithObserver::create(
    ExecutionContext* context,
    int fetchEventID,
    const KURL& requestURL,
    WebURLRequest::FetchRequestMode requestMode,
    WebURLRequest::FetchRedirectMode redirectMode,
    WebURLRequest::FrameType frameType,
    WebURLRequest::RequestContext requestContext,
    WaitUntilObserver* observer) {
  return new FetchRespondWithObserver(context, fetchEventID, requestURL,
                                      requestMode, redirectMode, frameType,
                                      requestContext, observer);
}

void FetchRespondWithObserver::onResponseRejected(
    WebServiceWorkerResponseError error) {
  DCHECK(getExecutionContext());
  getExecutionContext()->addConsoleMessage(
      ConsoleMessage::create(JSMessageSource, WarningMessageLevel,
                             getMessageForResponseError(error, m_requestURL)));

  // The default value of WebServiceWorkerResponse's status is 0, which maps
  // to a network error.
  WebServiceWorkerResponse webResponse;
  webResponse.setError(error);
  ServiceWorkerGlobalScopeClient::from(getExecutionContext())
      ->respondToFetchEvent(m_eventID, webResponse, m_eventDispatchTime);
}

void FetchRespondWithObserver::onResponseFulfilled(const ScriptValue& value) {
  DCHECK(getExecutionContext());
  if (!V8Response::hasInstance(value.v8Value(),
                               toIsolate(getExecutionContext()))) {
    onResponseRejected(WebServiceWorkerResponseErrorNoV8Instance);
    return;
  }
  Response* response = V8Response::toImplWithTypeCheck(
      toIsolate(getExecutionContext()), value.v8Value());
  // "If one of the following conditions is true, return a network error:
  //   - |response|'s type is |error|.
  //   - |request|'s mode is not |no-cors| and response's type is |opaque|.
  //   - |request| is a client request and |response|'s type is neither
  //     |basic| nor |default|."
  const FetchResponseData::Type responseType = response->response()->getType();
  if (responseType == FetchResponseData::ErrorType) {
    onResponseRejected(WebServiceWorkerResponseErrorResponseTypeError);
    return;
  }
  if (responseType == FetchResponseData::OpaqueType) {
    if (m_requestMode != WebURLRequest::FetchRequestModeNoCORS) {
      onResponseRejected(WebServiceWorkerResponseErrorResponseTypeOpaque);
      return;
    }

    // The request mode of client requests should be "same-origin" but it is
    // not explicitly stated in the spec yet. So we need to check here.
    // FIXME: Set the request mode of client requests to "same-origin" and
    // remove this check when the spec will be updated.
    // Spec issue: https://github.com/whatwg/fetch/issues/101
    if (isClientRequest(m_frameType, m_requestContext)) {
      onResponseRejected(
          WebServiceWorkerResponseErrorResponseTypeOpaqueForClientRequest);
      return;
    }
  }
  if (m_redirectMode != WebURLRequest::FetchRedirectModeManual &&
      responseType == FetchResponseData::OpaqueRedirectType) {
    onResponseRejected(WebServiceWorkerResponseErrorResponseTypeOpaqueRedirect);
    return;
  }
  if (m_redirectMode != WebURLRequest::FetchRedirectModeFollow &&
      response->redirected()) {
    if (!isNavigationRequest(m_frameType)) {
      onResponseRejected(
          WebServiceWorkerResponseErrorRedirectedResponseForNotFollowRequest);
      return;
    }
    // TODO(horo): We should just reject even if the request was a navigation.
    // Currently we measure the impact of the restriction with the use counter
    // in DocumentLoader.
    getExecutionContext()->addConsoleMessage(ConsoleMessage::create(
        JSMessageSource, ErrorMessageLevel,
        getErrorMessageForRedirectedResponseForNavigationRequest(
            m_requestURL, response->internalURLList())));
  }
  if (response->isBodyLocked()) {
    onResponseRejected(WebServiceWorkerResponseErrorBodyLocked);
    return;
  }
  if (response->bodyUsed()) {
    onResponseRejected(WebServiceWorkerResponseErrorBodyUsed);
    return;
  }

  WebServiceWorkerResponse webResponse;
  response->populateWebServiceWorkerResponse(webResponse);
  BodyStreamBuffer* buffer = response->internalBodyBuffer();
  if (buffer) {
    RefPtr<BlobDataHandle> blobDataHandle = buffer->drainAsBlobDataHandle(
        BytesConsumer::BlobSizePolicy::AllowBlobWithInvalidSize);
    if (blobDataHandle) {
      webResponse.setBlobDataHandle(blobDataHandle);
    } else {
      Stream* outStream = Stream::create(getExecutionContext(), "");
      webResponse.setStreamURL(outStream->url());
      buffer->startLoading(FetchDataLoader::createLoaderAsStream(outStream),
                           new NoopLoaderClient);
    }
  }
  ServiceWorkerGlobalScopeClient::from(getExecutionContext())
      ->respondToFetchEvent(m_eventID, webResponse, m_eventDispatchTime);
}

void FetchRespondWithObserver::onNoResponse() {
  ServiceWorkerGlobalScopeClient::from(getExecutionContext())
      ->respondToFetchEvent(m_eventID, m_eventDispatchTime);
}

FetchRespondWithObserver::FetchRespondWithObserver(
    ExecutionContext* context,
    int fetchEventID,
    const KURL& requestURL,
    WebURLRequest::FetchRequestMode requestMode,
    WebURLRequest::FetchRedirectMode redirectMode,
    WebURLRequest::FrameType frameType,
    WebURLRequest::RequestContext requestContext,
    WaitUntilObserver* observer)
    : RespondWithObserver(context, fetchEventID, observer),
      m_requestURL(requestURL),
      m_requestMode(requestMode),
      m_redirectMode(redirectMode),
      m_frameType(frameType),
      m_requestContext(requestContext) {}

DEFINE_TRACE(FetchRespondWithObserver) {
  RespondWithObserver::trace(visitor);
}

}  // namespace blink
