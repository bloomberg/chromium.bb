// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorTraceEvents_h
#define InspectorTraceEvents_h

#include "platform/EventTracer.h"
#include "wtf/Forward.h"

namespace WebCore {

class Document;
class ExecutionContext;
class FrameView;
class GraphicsContext;
class GraphicsLayer;
class KURL;
class LayoutRect;
class LocalFrame;
class RenderObject;
class ResourceRequest;
class ResourceResponse;
class XMLHttpRequest;

class InspectorLayoutEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> beginData(FrameView*);
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> endData(RenderObject* rootForThisLayout);
};

class InspectorSendRequestEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(unsigned long identifier, LocalFrame*, const ResourceRequest&);
};

class InspectorReceiveResponseEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(unsigned long identifier, LocalFrame*, const ResourceResponse&);
};

class InspectorReceiveDataEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(unsigned long identifier, LocalFrame*, int encodedDataLength);
};

class InspectorResourceFinishEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(unsigned long identifier, double finishTime, bool didFail);
};

class InspectorTimerInstallEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, int timerId, int timeout, bool singleShot);
};

class InspectorTimerRemoveEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, int timerId);
};

class InspectorTimerFireEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, int timerId);
};

class InspectorAnimationFrameEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(Document*, int callbackId);
};

class InspectorWebSocketCreateEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(Document*, unsigned long identifier, const KURL&, const String& protocol);
};

class InspectorWebSocketEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(Document*, unsigned long identifier);
};

class InspectorParseHtmlEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> beginData(Document*, unsigned startLine);
};

class InspectorXhrReadyStateChangeEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, XMLHttpRequest*);
};

class InspectorXhrLoadEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, XMLHttpRequest*);
};

class InspectorPaintEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(RenderObject*, const LayoutRect& clipRect, const GraphicsLayer*);
};

class InspectorMarkLoadEvent {
public:
    static PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(LocalFrame*);
};

} // namespace WebCore


#endif // !defined(InspectorTraceEvents_h)
