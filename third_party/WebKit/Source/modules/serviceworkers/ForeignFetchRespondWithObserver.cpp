// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ForeignFetchRespondWithObserver.h"

#include "modules/fetch/Response.h"
#include "modules/serviceworkers/ForeignFetchResponse.h"
#include "platform/loader/fetch/CrossOriginAccessControl.h"

namespace blink {

ForeignFetchRespondWithObserver* ForeignFetchRespondWithObserver::create(
    ExecutionContext* context,
    int eventID,
    const KURL& requestURL,
    WebURLRequest::FetchRequestMode requestMode,
    WebURLRequest::FetchRedirectMode redirectMode,
    WebURLRequest::FrameType frameType,
    WebURLRequest::RequestContext requestContext,
    PassRefPtr<SecurityOrigin> requestOrigin,
    WaitUntilObserver* observer) {
  return new ForeignFetchRespondWithObserver(
      context, eventID, requestURL, requestMode, redirectMode, frameType,
      requestContext, std::move(requestOrigin), observer);
}

void ForeignFetchRespondWithObserver::responseWasFulfilled(
    const ScriptValue& value) {
  ASSERT(getExecutionContext());
  ExceptionState exceptionState(value.isolate(), ExceptionState::UnknownContext,
                                "ForeignFetchEvent", "respondWith");
  ForeignFetchResponse foreignFetchResponse =
      ScriptValue::to<ForeignFetchResponse>(toIsolate(getExecutionContext()),
                                            value, exceptionState);
  if (exceptionState.hadException()) {
    exceptionState.clearException();
    responseWasRejected(WebServiceWorkerResponseErrorNoForeignFetchResponse);
    return;
  }

  Response* response = foreignFetchResponse.response();
  const FetchResponseData* internalResponse = response->response();
  const bool isOpaque =
      internalResponse->getType() == FetchResponseData::OpaqueType ||
      internalResponse->getType() == FetchResponseData::OpaqueRedirectType;
  if (internalResponse->getType() != FetchResponseData::DefaultType)
    internalResponse = internalResponse->internalResponse();

  if (!foreignFetchResponse.hasOrigin()) {
    if (foreignFetchResponse.hasHeaders() &&
        !foreignFetchResponse.headers().isEmpty()) {
      responseWasRejected(
          WebServiceWorkerResponseErrorForeignFetchHeadersWithoutOrigin);
      return;
    }

    // If response isn't already opaque, make it opaque.
    if (!isOpaque) {
      FetchResponseData* opaqueData =
          internalResponse->createOpaqueFilteredResponse();
      response = Response::create(getExecutionContext(), opaqueData);
    }
  } else if (m_requestOrigin->toString() != foreignFetchResponse.origin()) {
    responseWasRejected(
        WebServiceWorkerResponseErrorForeignFetchMismatchedOrigin);
    return;
  } else if (!isOpaque) {
    HTTPHeaderSet headers;
    if (foreignFetchResponse.hasHeaders()) {
      for (const String& header : foreignFetchResponse.headers())
        headers.insert(header);
      if (response->response()->getType() == FetchResponseData::CORSType) {
        const HTTPHeaderSet& existingHeaders =
            response->response()->corsExposedHeaderNames();
        HTTPHeaderSet headersToRemove;
        for (HTTPHeaderSet::iterator it = headers.begin(); it != headers.end();
             ++it) {
          if (!existingHeaders.contains(*it))
            headersToRemove.insert(*it);
        }
        headers.removeAll(headersToRemove);
      }
    }
    FetchResponseData* responseData =
        internalResponse->createCORSFilteredResponse(headers);
    response = Response::create(getExecutionContext(), responseData);
  }

  RespondWithObserver::responseWasFulfilled(
      ScriptValue::from(value.getScriptState(), response));
}

ForeignFetchRespondWithObserver::ForeignFetchRespondWithObserver(
    ExecutionContext* context,
    int eventID,
    const KURL& requestURL,
    WebURLRequest::FetchRequestMode requestMode,
    WebURLRequest::FetchRedirectMode redirectMode,
    WebURLRequest::FrameType frameType,
    WebURLRequest::RequestContext requestContext,
    PassRefPtr<SecurityOrigin> requestOrigin,
    WaitUntilObserver* observer)
    : RespondWithObserver(context,
                          eventID,
                          requestURL,
                          requestMode,
                          redirectMode,
                          frameType,
                          requestContext,
                          observer),
      m_requestOrigin(requestOrigin) {}

}  // namespace blink
