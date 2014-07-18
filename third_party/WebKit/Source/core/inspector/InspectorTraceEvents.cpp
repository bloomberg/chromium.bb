// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/InspectorTraceEvents.h"

#include "bindings/core/v8/ScriptCallStackFactory.h"
#include "bindings/core/v8/ScriptGCEvent.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "core/events/Event.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorNodeIds.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/page/Page.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderObject.h"
#include "core/xml/XMLHttpRequest.h"
#include "platform/JSONValues.h"
#include "platform/TracedValue.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/KURL.h"
#include "wtf/Vector.h"
#include <inttypes.h>

namespace blink {

namespace {

class JSCallStack : public TraceEvent::ConvertableToTraceFormat  {
public:
    explicit JSCallStack(PassRefPtrWillBeRawPtr<ScriptCallStack> callstack)
    {
        m_serialized = callstack ? callstack->buildInspectorArray()->toJSONString() : "null";
        ASSERT(m_serialized.isSafeToSendToAnotherThread());
    }
    virtual String asTraceFormat() const
    {
        return m_serialized;
    }

private:
    String m_serialized;
};

String toHexString(void* p)
{
    return String::format("0x%" PRIx64, static_cast<uint64>(reinterpret_cast<intptr_t>(p)));
}

}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorLayoutEvent::beginData(FrameView* frameView)
{
    bool isPartial;
    unsigned needsLayoutObjects;
    unsigned totalObjects;
    LocalFrame& frame = frameView->frame();
    frame.countObjectsNeedingLayout(needsLayoutObjects, totalObjects, isPartial);

    TracedValue value;
    return value.setInteger("dirtyObjects", needsLayoutObjects)
        .setInteger("totalObjects", totalObjects)
        .setBoolean("partialLayout", isPartial)
        .setString("frame", toHexString(&frame))
        .finish();
}

static void createQuad(TracedValue& value, const char* name, const FloatQuad& quad)
{
    value.beginArray(name)
        .pushDouble(quad.p1().x())
        .pushDouble(quad.p1().y())
        .pushDouble(quad.p2().x())
        .pushDouble(quad.p2().y())
        .pushDouble(quad.p3().x())
        .pushDouble(quad.p3().y())
        .pushDouble(quad.p4().x())
        .pushDouble(quad.p4().y())
        .endArray();
}

static void setGeneratingNodeId(TracedValue& value, const char* fieldName, const RenderObject* renderer)
{
    Node* node = 0;
    for (; renderer && !node; renderer = renderer->parent())
        node = renderer->generatingNode();
    if (!node)
        return;
    value.setInteger(fieldName, InspectorNodeIds::idForNode(node));
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorLayoutEvent::endData(RenderObject* rootForThisLayout)
{
    Vector<FloatQuad> quads;
    rootForThisLayout->absoluteQuads(quads);

    TracedValue value;
    if (quads.size() >= 1) {
        createQuad(value, "root", quads[0]);
        setGeneratingNodeId(value, "rootNode", rootForThisLayout);
    } else {
        ASSERT_NOT_REACHED();
    }
    return value.finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorSendRequestEvent::data(unsigned long identifier, LocalFrame* frame, const ResourceRequest& request)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    TracedValue value;
    return value.setString("requestId", requestId)
        .setString("frame", toHexString(frame))
        .setString("url", request.url().string())
        .setString("requestMethod", request.httpMethod())
        .finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorReceiveResponseEvent::data(unsigned long identifier, LocalFrame* frame, const ResourceResponse& response)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    TracedValue value;
    return value.setString("requestId", requestId)
        .setString("frame", toHexString(frame))
        .setInteger("statusCode", response.httpStatusCode())
        .setString("mimeType", response.mimeType().string().isolatedCopy())
        .finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorReceiveDataEvent::data(unsigned long identifier, LocalFrame* frame, int encodedDataLength)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    TracedValue value;
    return value.setString("requestId", requestId)
        .setString("frame", toHexString(frame))
        .setInteger("encodedDataLength", encodedDataLength)
        .finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorResourceFinishEvent::data(unsigned long identifier, double finishTime, bool didFail)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    TracedValue value;
    value.setString("requestId", requestId);
    value.setBoolean("didFail", didFail);
    if (finishTime)
        value.setDouble("networkTime", finishTime);
    return value.finish();
}

static LocalFrame* frameForExecutionContext(ExecutionContext* context)
{
    LocalFrame* frame = 0;
    if (context->isDocument())
        frame = toDocument(context)->frame();
    return frame;
}

static PassOwnPtr<TracedValue> genericTimerData(ExecutionContext* context, int timerId)
{
    OwnPtr<TracedValue> value = adoptPtr(new TracedValue());
    value->setInteger("timerId", timerId);
    if (LocalFrame* frame = frameForExecutionContext(context))
        value->setString("frame", toHexString(frame));
    return value.release();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorTimerInstallEvent::data(ExecutionContext* context, int timerId, int timeout, bool singleShot)
{
    OwnPtr<TracedValue> value = genericTimerData(context, timerId);
    value->setInteger("timeout", timeout);
    value->setBoolean("singleShot", singleShot);
    return value->finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorTimerRemoveEvent::data(ExecutionContext* context, int timerId)
{
    return genericTimerData(context, timerId)->finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorTimerFireEvent::data(ExecutionContext* context, int timerId)
{
    return genericTimerData(context, timerId)->finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorAnimationFrameEvent::data(Document* document, int callbackId)
{
    TracedValue value;
    return value.setInteger("id", callbackId)
        .setString("frame", toHexString(document->frame()))
        .finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorWebSocketCreateEvent::data(Document* document, unsigned long identifier, const KURL& url, const String& protocol)
{
    TracedValue value;
    value.setInteger("identifier", identifier);
    value.setString("url", url.string());
    value.setString("frame", toHexString(document->frame()));
    if (!protocol.isNull())
        value.setString("webSocketProtocol", protocol);
    return value.finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorWebSocketEvent::data(Document* document, unsigned long identifier)
{
    TracedValue value;
    value.setInteger("identifier", identifier);
    value.setString("frame", toHexString(document->frame()));
    return value.finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorParseHtmlEvent::beginData(Document* document, unsigned startLine)
{
    TracedValue value;
    value.setInteger("startLine", startLine);
    value.setString("frame", toHexString(document->frame()));
    return value.finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorXhrReadyStateChangeEvent::data(ExecutionContext* context, XMLHttpRequest* request)
{
    TracedValue value;
    value.setString("url", request->url().string());
    value.setInteger("readyState", request->readyState());
    if (LocalFrame* frame = frameForExecutionContext(context))
        value.setString("frame", toHexString(frame));
    return value.finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorXhrLoadEvent::data(ExecutionContext* context, XMLHttpRequest* request)
{
    TracedValue value;
    value.setString("url", request->url().string());
    if (LocalFrame* frame = frameForExecutionContext(context))
        value.setString("frame", toHexString(frame));
    return value.finish();
}

static void localToPageQuad(const RenderObject& renderer, const LayoutRect& rect, FloatQuad* quad)
{
    LocalFrame* frame = renderer.frame();
    FrameView* view = frame->view();
    FloatQuad absolute = renderer.localToAbsoluteQuad(FloatQuad(rect));
    quad->setP1(view->contentsToRootView(roundedIntPoint(absolute.p1())));
    quad->setP2(view->contentsToRootView(roundedIntPoint(absolute.p2())));
    quad->setP3(view->contentsToRootView(roundedIntPoint(absolute.p3())));
    quad->setP4(view->contentsToRootView(roundedIntPoint(absolute.p4())));
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorPaintEvent::data(RenderObject* renderer, const LayoutRect& clipRect, const GraphicsLayer* graphicsLayer)
{
    TracedValue value;
    value.setString("frame", toHexString(renderer->frame()));
    FloatQuad quad;
    localToPageQuad(*renderer, clipRect, &quad);
    createQuad(value, "clip", quad);
    setGeneratingNodeId(value, "nodeId", renderer);
    int graphicsLayerId = graphicsLayer ? graphicsLayer->platformLayer()->id() : 0;
    value.setInteger("layerId", graphicsLayerId);
    return value.finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorMarkLoadEvent::data(LocalFrame* frame)
{
    TracedValue value;
    value.setString("frame", toHexString(frame));
    bool isMainFrame = frame && frame->page()->mainFrame() == frame;
    value.setBoolean("isMainFrame", isMainFrame);
    return value.finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorScrollLayerEvent::data(RenderObject* renderer)
{
    TracedValue value;
    value.setString("frame", toHexString(renderer->frame()));
    setGeneratingNodeId(value, "nodeId", renderer);
    return value.finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorEvaluateScriptEvent::data(LocalFrame* frame, const String& url, int lineNumber)
{
    TracedValue value;
    value.setString("frame", toHexString(frame));
    value.setString("url", url);
    value.setInteger("lineNumber", lineNumber);
    return value.finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorFunctionCallEvent::data(ExecutionContext* context, int scriptId, const String& scriptName, int scriptLine)
{
    TracedValue value;
    value.setString("scriptId", String::number(scriptId));
    value.setString("scriptName", scriptName);
    value.setInteger("scriptLine", scriptLine);
    if (LocalFrame* frame = frameForExecutionContext(context))
        value.setString("frame", toHexString(frame));
    return value.finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorPaintImageEvent::data(const RenderImage& renderImage)
{
    TracedValue value;
    setGeneratingNodeId(value, "nodeId", &renderImage);
    if (const ImageResource* resource = renderImage.cachedImage())
        value.setString("url", resource->url().string());
    return value.finish();
}

static size_t usedHeapSize()
{
    HeapInfo info;
    ScriptGCEvent::getHeapSize(info);
    return info.usedJSHeapSize;
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorUpdateCountersEvent::data()
{
    TracedValue value;
    if (isMainThread()) {
        value.setInteger("documents", InspectorCounters::counterValue(InspectorCounters::DocumentCounter));
        value.setInteger("nodes", InspectorCounters::counterValue(InspectorCounters::NodeCounter));
        value.setInteger("jsEventListeners", InspectorCounters::counterValue(InspectorCounters::JSEventListenerCounter));
    }
    value.setDouble("jsHeapSizeUsed", static_cast<double>(usedHeapSize()));
    return value.finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorCallStackEvent::currentCallStack()
{
    return adoptRef(new JSCallStack(createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture, true)));
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorEventDispatchEvent::data(const Event& event)
{
    return TracedValue().setString("type", event.type()).finish();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorTimeStampEvent::data(ExecutionContext* context, const String& message)
{
    TracedValue value;
    value.setString("message", message);
    if (LocalFrame* frame = frameForExecutionContext(context))
        value.setString("frame", toHexString(frame));
    return value.finish();
}

}
