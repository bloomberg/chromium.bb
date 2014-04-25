// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/InspectorTraceEvents.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorNodeIds.h"
#include "core/rendering/RenderObject.h"
#include "platform/JSONValues.h"
#include "platform/TracedValue.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "wtf/Vector.h"
#include <inttypes.h>

namespace WebCore {

static String toHexString(void* p)
{
    return String::format("0x%" PRIx64, static_cast<uint64>(reinterpret_cast<intptr_t>(p)));
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorLayoutEvent::beginData(FrameView* frameView)
{
    bool isPartial;
    unsigned needsLayoutObjects;
    unsigned totalObjects;
    LocalFrame& frame = frameView->frame();
    frame.countObjectsNeedingLayout(needsLayoutObjects, totalObjects, isPartial);

    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("dirtyObjects", needsLayoutObjects);
    data->setNumber("totalObjects", totalObjects);
    data->setBoolean("partialLayout", isPartial);
    data->setString("frame", toHexString(&frame));
    return TracedValue::fromJSONValue(data);
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

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorLayoutEvent::endData(RenderObject* rootForThisLayout)
{
    Vector<FloatQuad> quads;
    rootForThisLayout->absoluteQuads(quads);

    RefPtr<JSONObject> data = JSONObject::create();
    if (quads.size() >= 1) {
        data->setArray("root", createQuad(quads[0]));
        int rootNodeId = InspectorNodeIds::idForNode(rootForThisLayout->generatingNode());
        data->setNumber("rootNode", rootNodeId);
    } else {
        ASSERT_NOT_REACHED();
    }
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorSendRequestEvent::data(unsigned long identifier, LocalFrame* frame, const ResourceRequest& request)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setString("frame", toHexString(frame));
    data->setString("url", request.url().string());
    data->setString("requestMethod", request.httpMethod());
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorReceiveResponseEvent::data(unsigned long identifier, LocalFrame* frame, const ResourceResponse& response)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setString("frame", toHexString(frame));
    data->setNumber("statusCode", response.httpStatusCode());
    data->setString("mimeType", response.mimeType());
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorReceiveDataEvent::data(unsigned long identifier, LocalFrame* frame, int encodedDataLength)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setString("frame", toHexString(frame));
    data->setNumber("encodedDataLength", encodedDataLength);
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorResourceFinishEvent::data(unsigned long identifier, double finishTime, bool didFail)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setBoolean("didFail", didFail);
    if (finishTime)
        data->setNumber("networkTime", finishTime);
    return TracedValue::fromJSONValue(data);
}

}
