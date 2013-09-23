/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/inspector/TimelineRecordFactory.h"

#include "bindings/v8/ScriptCallStackFactory.h"
#include "core/events/Event.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/platform/JSONValues.h"
#include "core/platform/graphics/FloatQuad.h"
#include "core/platform/graphics/LayoutRect.h"
#include "core/platform/network/ResourceRequest.h"
#include "core/platform/network/ResourceResponse.h"
#include "wtf/CurrentTime.h"

namespace WebCore {

PassRefPtr<JSONObject> TimelineRecordFactory::createGenericRecord(double startTime, int maxCallStackDepth, const String& type)
{
    RefPtr<JSONObject> record = JSONObject::create();
    record->setNumber("startTime", startTime);

    if (maxCallStackDepth) {
        RefPtr<ScriptCallStack> stackTrace = createScriptCallStack(maxCallStackDepth, true);
        if (stackTrace && stackTrace->size())
            record->setValue("stackTrace", stackTrace->buildInspectorArray());
    }
    record->setString("type", type);
    return record.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createBackgroundRecord(double startTime, const String& threadName)
{
    RefPtr<JSONObject> record = JSONObject::create();
    record->setNumber("startTime", startTime);
    record->setString("thread", threadName);
    return record.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createGCEventData(const size_t usedHeapSizeDelta)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("usedHeapSizeDelta", usedHeapSizeDelta);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createFunctionCallData(const String& scriptName, int scriptLine)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("scriptName", scriptName);
    data->setNumber("scriptLine", scriptLine);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createEventDispatchData(const Event& event)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("type", event.type().string());
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createGenericTimerData(int timerId)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("timerId", timerId);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createTimerInstallData(int timerId, int timeout, bool singleShot)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("timerId", timerId);
    data->setNumber("timeout", timeout);
    data->setBoolean("singleShot", singleShot);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createXHRReadyStateChangeData(const String& url, int readyState)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("url", url);
    data->setNumber("readyState", readyState);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createXHRLoadData(const String& url)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("url", url);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createEvaluateScriptData(const String& url, double lineNumber)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("url", url);
    data->setNumber("lineNumber", lineNumber);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createTimeStampData(const String& message)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("message", message);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createScheduleResourceRequestData(const String& url)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("url", url);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createResourceSendRequestData(const String& requestId, const ResourceRequest& request)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setString("url", request.url().string());
    data->setString("requestMethod", request.httpMethod());
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createResourceReceiveResponseData(const String& requestId, const ResourceResponse& response)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setNumber("statusCode", response.httpStatusCode());
    data->setString("mimeType", response.mimeType());
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createResourceFinishData(const String& requestId, bool didFail, double finishTime)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setBoolean("didFail", didFail);
    if (finishTime)
        data->setNumber("networkTime", finishTime);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createReceiveResourceData(const String& requestId, int length)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setNumber("encodedDataLength", length);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createLayoutData(unsigned dirtyObjects, unsigned totalObjects, bool partialLayout)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("dirtyObjects", dirtyObjects);
    data->setNumber("totalObjects", totalObjects);
    data->setBoolean("partialLayout", partialLayout);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createDecodeImageData(const String& imageType)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("imageType", imageType);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createResizeImageData(bool shouldCache)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setBoolean("cached", shouldCache);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createMarkData(bool isMainFrame)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setBoolean("isMainFrame", isMainFrame);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createParseHTMLData(unsigned startLine)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("startLine", startLine);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createAnimationFrameData(int callbackId)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("id", callbackId);
    return data.release();
}

static PassRefPtr<JSONArray> createQuad(const FloatQuad& quad)
{
    RefPtr<JSONArray> array = JSONArray::create();
    array->pushNumber(quad.p1().x());
    array->pushNumber(quad.p1().y());
    array->pushNumber(quad.p2().x());
    array->pushNumber(quad.p2().y());
    array->pushNumber(quad.p3().x());
    array->pushNumber(quad.p3().y());
    array->pushNumber(quad.p4().x());
    array->pushNumber(quad.p4().y());
    return array.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createLayerData(long long layerRootNodeId)
{
    RefPtr<JSONObject> data = JSONObject::create();
    if (layerRootNodeId)
        data->setNumber("layerRootNode", layerRootNodeId);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createPaintData(const FloatQuad& quad, long long layerRootNodeId)
{
    RefPtr<JSONObject> data = TimelineRecordFactory::createLayerData(layerRootNodeId);
    data->setArray("clip", createQuad(quad));
    return data.release();
}

void TimelineRecordFactory::appendLayoutRoot(JSONObject* data, const FloatQuad& quad, long long rootNodeId)
{
    data->setArray("root", createQuad(quad));
    if (rootNodeId)
        data->setNumber("rootNode", rootNodeId);
}

void TimelineRecordFactory::appendStyleRecalcDetails(JSONObject* data, unsigned elementCount)
{
    data->setNumber("elementCount", elementCount);
}

void TimelineRecordFactory::appendImageDetails(JSONObject* data, long long imageElementId, const String& url)
{
    if (imageElementId)
        data->setNumber("elementId", imageElementId);
    if (!url.isEmpty())
        data->setString("url", url);
}

} // namespace WebCore

