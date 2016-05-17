// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ForeignFetchRespondWithObserver.h"

#include "modules/serviceworkers/ForeignFetchResponse.h"

namespace blink {

ForeignFetchRespondWithObserver* ForeignFetchRespondWithObserver::create(ExecutionContext* context, int eventID, const KURL& requestURL, WebURLRequest::FetchRequestMode requestMode, WebURLRequest::FrameType frameType, WebURLRequest::RequestContext requestContext, PassRefPtr<SecurityOrigin> requestOrigin)
{
    return new ForeignFetchRespondWithObserver(context, eventID, requestURL, requestMode, frameType, requestContext, requestOrigin);
}

void ForeignFetchRespondWithObserver::responseWasFulfilled(const ScriptValue& value)
{
    ASSERT(getExecutionContext());
    TrackExceptionState exceptionState;
    ForeignFetchResponse foreignFetchResponse = ScriptValue::to<ForeignFetchResponse>(toIsolate(getExecutionContext()), value, exceptionState);
    if (exceptionState.hadException()) {
        responseWasRejected(WebServiceWorkerResponseErrorNoForeignFetchResponse);
        return;
    }

    Response* response = foreignFetchResponse.response();
    const FetchResponseData* internalResponse = response->response();
    const bool isOpaque = internalResponse->getType() == FetchResponseData::OpaqueType || internalResponse->getType() == FetchResponseData::OpaqueRedirectType;
    if (internalResponse->getType() != FetchResponseData::DefaultType)
        internalResponse = internalResponse->internalResponse();

    if (!foreignFetchResponse.hasOrigin()) {
        if (foreignFetchResponse.hasHeaders() && !foreignFetchResponse.headers().isEmpty()) {
            responseWasRejected(WebServiceWorkerResponseErrorForeignFetchHeadersWithoutOrigin);
            return;
        }

        // If response isn't already opaque, make it opaque.
        if (!isOpaque) {
            FetchResponseData* opaqueData = internalResponse->createOpaqueFilteredResponse();
            response = Response::create(getExecutionContext(), opaqueData);
        }
    } else if (m_requestOrigin->toString() != foreignFetchResponse.origin()) {
        responseWasRejected(WebServiceWorkerResponseErrorForeignFetchMismatchedOrigin);
        return;
    } else if (!isOpaque) {
        // TODO(mek): Handle |headers| response attribute, and properly filter response.
    }

    RespondWithObserver::responseWasFulfilled(ScriptValue::from(value.getScriptState(), response));
}

ForeignFetchRespondWithObserver::ForeignFetchRespondWithObserver(ExecutionContext* context, int eventID, const KURL& requestURL, WebURLRequest::FetchRequestMode requestMode, WebURLRequest::FrameType frameType, WebURLRequest::RequestContext requestContext, PassRefPtr<SecurityOrigin> requestOrigin)
    : RespondWithObserver(context, eventID, requestURL, requestMode, frameType, requestContext)
    , m_requestOrigin(requestOrigin)
{
}

} // namespace blink
