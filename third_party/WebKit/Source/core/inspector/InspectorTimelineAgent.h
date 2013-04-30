/*
* Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef InspectorTimelineAgent_h
#define InspectorTimelineAgent_h


#include "InspectorFrontend.h"
#include "bindings/v8/ScriptGCEvent.h"
#include "core/dom/EventContext.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorValues.h"
#include "core/inspector/ScriptGCEventListener.h"
#include "core/platform/PlatformInstrumentation.h"
#include "core/platform/graphics/LayoutRect.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class DOMWindow;
class Document;
class DocumentLoader;
class Event;
class FloatQuad;
class Frame;
class GraphicsContext;
class InspectorClient;
class InspectorFrontend;
class InspectorMemoryAgent;
class InspectorPageAgent;
class InspectorState;
class InstrumentingAgents;
class IntRect;
class KURL;
class Node;
class Page;
class RenderObject;
class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;
class ScriptExecutionContext;
class TimelineTraceEventProcessor;
class WebSocketHandshakeRequest;
class WebSocketHandshakeResponse;
class XMLHttpRequest;

typedef String ErrorString;

namespace TimelineRecordType {
extern const char DecodeImage[];
extern const char Rasterize[];
};

class TimelineTimeConverter {
public:
    TimelineTimeConverter()
        : m_startOffset(0)
    {
    }
    double fromMonotonicallyIncreasingTime(double time) const  { return (time - m_startOffset) * 1000.0; }
    void reset();

private:
    double m_startOffset;
};

class InspectorTimelineAgent
    : public InspectorBaseAgent<InspectorTimelineAgent>,
      public ScriptGCEventListener,
      public InspectorBackendDispatcher::TimelineCommandHandler,
      public PlatformInstrumentationClient {
    WTF_MAKE_NONCOPYABLE(InspectorTimelineAgent);
public:
    enum InspectorType { PageInspector, WorkerInspector };

    static PassOwnPtr<InspectorTimelineAgent> create(InstrumentingAgents* instrumentingAgents, InspectorPageAgent* pageAgent, InspectorMemoryAgent* memoryAgent, InspectorCompositeState* state, InspectorType type, InspectorClient* client)
    {
        return adoptPtr(new InspectorTimelineAgent(instrumentingAgents, pageAgent, memoryAgent, state, type, client));
    }

    ~InspectorTimelineAgent();

    virtual void setFrontend(InspectorFrontend*);
    virtual void clearFrontend();
    virtual void restore();

    virtual void start(ErrorString*, const int* maxCallStackDepth, const bool* includeDomCounters, const bool* includeNativeMemoryStatistics);
    virtual void stop(ErrorString*);

    int id() const { return m_id; }

    void didCommitLoad();

    // Methods called from WebCore.
    bool willCallFunction(ScriptExecutionContext* context, const String& scriptName, int scriptLine);
    void didCallFunction();

    bool willDispatchEvent(Document* document, const Event& event, DOMWindow* window, Node* node, const EventPath& eventPath);
    bool willDispatchEventOnWindow(const Event& event, DOMWindow* window);
    void didDispatchEvent();
    void didDispatchEventOnWindow();

    void didBeginFrame();
    void didCancelFrame();

    void didInvalidateLayout(Frame*);
    bool willLayout(Frame*);
    void didLayout(RenderObject*);

    void didScheduleStyleRecalculation(Document*);
    bool willRecalculateStyle(Document*);
    void didRecalculateStyle();
    void didRecalculateStyleForElement();

    void willPaint(Frame*);
    void didPaint(RenderObject*, GraphicsContext*, const LayoutRect&);

    void willScrollLayer(Frame*);
    void didScrollLayer();

    void willComposite();
    void didComposite();

    bool willWriteHTML(Document*, unsigned startLine);
    void didWriteHTML(unsigned endLine);

    void didInstallTimer(ScriptExecutionContext* context, int timerId, int timeout, bool singleShot);
    void didRemoveTimer(ScriptExecutionContext* context, int timerId);
    bool willFireTimer(ScriptExecutionContext* context, int timerId);
    void didFireTimer();

    bool willDispatchXHRReadyStateChangeEvent(ScriptExecutionContext* context, XMLHttpRequest* request);
    void didDispatchXHRReadyStateChangeEvent();
    bool willDispatchXHRLoadEvent(ScriptExecutionContext* context, XMLHttpRequest* request);
    void didDispatchXHRLoadEvent();

    bool willEvaluateScript(Frame*, const String&, int);
    void didEvaluateScript();

    void didTimeStamp(Frame*, const String&);
    void didMarkDOMContentEvent(Frame*);
    void didMarkLoadEvent(Frame*);

    void time(Frame*, const String&);
    void timeEnd(Frame*, const String&);

    void didScheduleResourceRequest(Document*, const String& url);
    void willSendResourceRequest(unsigned long, DocumentLoader*, const ResourceRequest&, const ResourceResponse&);
    bool willReceiveResourceResponse(Frame*, unsigned long, const ResourceResponse&);
    void didReceiveResourceResponse(unsigned long, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    void didFinishLoadingResource(unsigned long, bool didFail, double finishTime, Frame*);
    void didFailLoading(unsigned long identifier, DocumentLoader* loader, const ResourceError& error);
    bool willReceiveResourceData(Frame*, unsigned long identifier, int length);
    void didReceiveResourceData();

    void didRequestAnimationFrame(Document*, int callbackId);
    void didCancelAnimationFrame(Document*, int callbackId);
    bool willFireAnimationFrame(Document*, int callbackId);
    void didFireAnimationFrame();

    void willProcessTask();
    void didProcessTask();

    void didCreateWebSocket(Document*, unsigned long identifier, const KURL&, const String& protocol);
    void willSendWebSocketHandshakeRequest(Document*, unsigned long identifier, const WebSocketHandshakeRequest&);
    void didReceiveWebSocketHandshakeResponse(Document*, unsigned long identifier, const WebSocketHandshakeResponse&);
    void didCloseWebSocket(Document*, unsigned long identifier);

    // ScriptGCEventListener methods.
    virtual void didGC(double, double, size_t);

    // PlatformInstrumentationClient methods.
    virtual void willDecodeImage(const String& imageType) OVERRIDE;
    virtual void didDecodeImage() OVERRIDE;
    virtual void willResizeImage(bool shouldCache) OVERRIDE;
    virtual void didResizeImage() OVERRIDE;

private:
    friend class TimelineRecordStack;
    friend class TimelineTraceEventProcessor;

    struct TimelineRecordEntry {
        TimelineRecordEntry(PassRefPtr<InspectorObject> record, PassRefPtr<InspectorObject> data, PassRefPtr<InspectorArray> children, const String& type, size_t usedHeapSizeAtStart)
            : record(record), data(data), children(children), type(type), usedHeapSizeAtStart(usedHeapSizeAtStart)
        {
        }
        RefPtr<InspectorObject> record;
        RefPtr<InspectorObject> data;
        RefPtr<InspectorArray> children;
        String type;
        size_t usedHeapSizeAtStart;
    };
        
    InspectorTimelineAgent(InstrumentingAgents*, InspectorPageAgent*, InspectorMemoryAgent*, InspectorCompositeState*, InspectorType, InspectorClient*);

    void sendEvent(PassRefPtr<InspectorObject>);
    void appendRecord(PassRefPtr<InspectorObject> data, const String& type, bool captureCallStack, Frame*);
    void pushCurrentRecord(PassRefPtr<InspectorObject>, const String& type, bool captureCallStack, Frame*, bool hasLowLevelDetails = false);

    void setDOMCounters(TypeBuilder::Timeline::TimelineEvent* record);
    void setNativeHeapStatistics(TypeBuilder::Timeline::TimelineEvent* record);
    void setFrameIdentifier(InspectorObject* record, Frame*);
    void pushGCEventRecords();

    void didCompleteCurrentRecord(const String& type);

    void setHeapSizeStatistics(InspectorObject* record);
    void commitFrameRecord();

    void addRecordToTimeline(PassRefPtr<InspectorObject>, const String& type);
    void innerAddRecordToTimeline(PassRefPtr<InspectorObject>, const String& type);
    void clearRecordStack();

    void localToPageQuad(const RenderObject& renderer, const LayoutRect&, FloatQuad*);
    const TimelineTimeConverter& timeConverter() const { return m_timeConverter; }
    double timestamp();
    Page* page();

    InspectorPageAgent* m_pageAgent;
    InspectorMemoryAgent* m_memoryAgent;
    TimelineTimeConverter m_timeConverter;

    InspectorFrontend::Timeline* m_frontend;
    double m_timestampOffset;

    Vector<TimelineRecordEntry> m_recordStack;

    int m_id;
    struct GCEvent {
        GCEvent(double startTime, double endTime, size_t collectedBytes)
            : startTime(startTime), endTime(endTime), collectedBytes(collectedBytes)
        {
        }
        double startTime;
        double endTime;
        size_t collectedBytes;
    };
    typedef Vector<GCEvent> GCEvents;
    GCEvents m_gcEvents;
    int m_maxCallStackDepth;
    unsigned m_platformInstrumentationClientInstalledAtStackDepth;
    RefPtr<InspectorObject> m_pendingFrameRecord;
    InspectorType m_inspectorType;
    InspectorClient* m_client;
    WeakPtrFactory<InspectorTimelineAgent> m_weakFactory;
    RefPtr<TimelineTraceEventProcessor> m_traceEventProcessor;
    unsigned m_styleRecalcElementCounter;
};

} // namespace WebCore

#endif // !defined(InspectorTimelineAgent_h)
