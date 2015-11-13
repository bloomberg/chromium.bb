// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/fetch/InspectorFetchTraceEvents.h"

#include "bindings/core/v8/ScriptCallStackFactory.h"
#include "platform/TraceEvent.h"
#include "platform/TracedValue.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"

namespace blink {

namespace {

void setCallStack(TracedValue* value)
{
    static const unsigned char* traceCategoryEnabled = 0;
    WTF_ANNOTATE_BENIGN_RACE(&traceCategoryEnabled, "trace_event category");
    if (!traceCategoryEnabled)
        traceCategoryEnabled = TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.stack"));
    if (!*traceCategoryEnabled)
        return;
    RefPtrWillBeRawPtr<ScriptCallStack> scriptCallStack = currentScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture);
    if (scriptCallStack)
        scriptCallStack->toTracedValue(value, "stackTrace");
}

} // namespace

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorSendRequestEvent::data(unsigned long identifier, const ResourceRequest& request)
{
    RefPtr<TracedValue> value = TracedValue::create();
    value->setString("requestId", String::number(identifier));
    value->setString("url", request.url().string());
    value->setString("requestMethod", request.httpMethod());
    setCallStack(value.get());
    return value.release();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorReceiveResponseEvent::data(unsigned long identifier, const ResourceResponse& response)
{
    RefPtr<TracedValue> value = TracedValue::create();
    value->setString("requestId", String::number(identifier));
    value->setInteger("statusCode", response.httpStatusCode());
    value->setString("mimeType", response.mimeType().string().isolatedCopy());
    return value.release();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorReceiveDataEvent::data(unsigned long identifier, int encodedDataLength)
{
    RefPtr<TracedValue> value = TracedValue::create();
    value->setString("requestId", String::number(identifier));
    value->setInteger("encodedDataLength", encodedDataLength);
    return value.release();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorResourceFinishEvent::data(unsigned long identifier, double finishTime, bool didFail)
{
    RefPtr<TracedValue> value = TracedValue::create();
    value->setString("requestId", String::number(identifier));
    value->setBoolean("didFail", didFail);
    if (finishTime)
        value->setDouble("networkTime", finishTime);
    return value.release();
}

} // namespace blink
