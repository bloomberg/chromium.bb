// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ForeignFetchRespondWithObserver.h"

#include "modules/serviceworkers/ForeignFetchResponse.h"

namespace blink {

ForeignFetchRespondWithObserver* ForeignFetchRespondWithObserver::create(ExecutionContext* context, int eventID, const KURL& requestURL, WebURLRequest::FetchRequestMode requestMode, WebURLRequest::FrameType frameType, WebURLRequest::RequestContext requestContext)
{
    return new ForeignFetchRespondWithObserver(context, eventID, requestURL, requestMode, frameType, requestContext);
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

    // TODO(mek): Handle foreign fetch specific response parameters.
    Response* response = foreignFetchResponse.response();
    RespondWithObserver::responseWasFulfilled(ScriptValue::from(value.getScriptState(), response));
}

ForeignFetchRespondWithObserver::ForeignFetchRespondWithObserver(ExecutionContext* context, int eventID, const KURL& requestURL, WebURLRequest::FetchRequestMode requestMode, WebURLRequest::FrameType frameType, WebURLRequest::RequestContext requestContext)
    : RespondWithObserver(context, eventID, requestURL, requestMode, frameType, requestContext)
{
}

} // namespace blink
