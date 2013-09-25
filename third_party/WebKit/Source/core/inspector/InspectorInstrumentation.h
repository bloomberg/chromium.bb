/*
* Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef InspectorInstrumentation_h
#define InspectorInstrumentation_h

#include "bindings/v8/ScriptString.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSStyleSheet.h"
#include "core/dom/Element.h"
#include "core/events/EventContext.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/platform/network/FormData.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderImage.h"
#include "core/storage/StorageArea.h"
#include "modules/websockets/WebSocketFrame.h"
#include "modules/websockets/WebSocketHandshakeRequest.h"
#include "modules/websockets/WebSocketHandshakeResponse.h"
#include "wtf/RefPtr.h"

namespace WebCore {

struct CSSParserString;
class Document;
class Element;
class DeviceOrientationData;
class GeolocationPosition;
class GraphicsContext;
class InspectorTimelineAgent;
class InstrumentingAgents;
class RenderLayer;
class ScriptExecutionContext;
class ThreadableLoaderClient;
class WorkerGlobalScope;
class WorkerGlobalScopeProxy;

#define FAST_RETURN_IF_NO_FRONTENDS(value) if (!hasFrontends()) return value;

class InspectorInstrumentationCookie {
public:
    InspectorInstrumentationCookie();
    InspectorInstrumentationCookie(InstrumentingAgents*, int);
    InspectorInstrumentationCookie(const InspectorInstrumentationCookie&);
    InspectorInstrumentationCookie& operator=(const InspectorInstrumentationCookie&);
    ~InspectorInstrumentationCookie();

    InstrumentingAgents* instrumentingAgents() const { return m_instrumentingAgents.get(); }
    bool isValid() const { return !!m_instrumentingAgents; }
    bool hasMatchingTimelineAgentId(int id) const { return m_timelineAgentId == id; }

private:
    RefPtr<InstrumentingAgents> m_instrumentingAgents;
    int m_timelineAgentId;
};

namespace InspectorInstrumentation {

class FrontendCounter {
private:
    friend void frontendCreated();
    friend void frontendDeleted();
    friend bool hasFrontends();
    static int s_frontendCounter;
};

inline void frontendCreated() { FrontendCounter::s_frontendCounter += 1; }
inline void frontendDeleted() { FrontendCounter::s_frontendCounter -= 1; }
inline bool hasFrontends() { return FrontendCounter::s_frontendCounter; }

void registerInstrumentingAgents(InstrumentingAgents*);
void unregisterInstrumentingAgents(InstrumentingAgents*);

InspectorTimelineAgent* retrieveTimelineAgent(const InspectorInstrumentationCookie&);

// Called from generated instrumentation code.
InstrumentingAgents* instrumentingAgentsFor(Page*);
InstrumentingAgents* instrumentingAgentsFor(Frame*);
InstrumentingAgents* instrumentingAgentsFor(ScriptExecutionContext*);
InstrumentingAgents* instrumentingAgentsFor(Document*);
InstrumentingAgents* instrumentingAgentsFor(RenderObject*);
InstrumentingAgents* instrumentingAgentsFor(Element*);
InstrumentingAgents* instrumentingAgentsFor(WorkerGlobalScope*);

// Helper for the one above.
InstrumentingAgents* instrumentingAgentsForNonDocumentContext(ScriptExecutionContext*);

}  // namespace InspectorInstrumentation

namespace InstrumentationEvents {
extern const char PaintSetup[];
extern const char PaintLayer[];
extern const char RasterTask[];
extern const char ImageDecodeTask[];
extern const char Paint[];
extern const char Layer[];
extern const char BeginFrame[];
extern const char UpdateLayer[];
};

namespace InstrumentationEventArguments {
extern const char LayerId[];
extern const char LayerTreeId[];
extern const char NodeId[];
extern const char PageId[];
extern const char PixelRefId[];
};

namespace InspectorInstrumentation {

inline InstrumentingAgents* instrumentingAgentsFor(ScriptExecutionContext* context)
{
    if (!context)
        return 0;
    return context->isDocument() ? instrumentingAgentsFor(toDocument(context)) : instrumentingAgentsForNonDocumentContext(context);
}

inline InstrumentingAgents* instrumentingAgentsFor(Frame* frame)
{
    return frame ? instrumentingAgentsFor(frame->page()) : 0;
}

inline InstrumentingAgents* instrumentingAgentsFor(Document* document)
{
    if (document) {
        Page* page = document->page();
        if (!page && document->templateDocumentHost())
            page = document->templateDocumentHost()->page();
        return instrumentingAgentsFor(page);
    }
    return 0;
}

inline InstrumentingAgents* instrumentingAgentsFor(Element* element)
{
    return element ? instrumentingAgentsFor(&element->document()) : 0;
}

bool cssErrorFilter(const CSSParserString& content, int propertyId, int errorType);

} // namespace InspectorInstrumentation

InstrumentingAgents* instrumentationForPage(Page*);

InstrumentingAgents* instrumentationForWorkerGlobalScope(WorkerGlobalScope*);

} // namespace WebCore

// This file will soon be generated
#include "InspectorInstrumentationInl.h"

#include "InspectorInstrumentationCustomInl.h"

#include "InspectorOverridesInl.h"

#endif // !defined(InspectorInstrumentation_h)
