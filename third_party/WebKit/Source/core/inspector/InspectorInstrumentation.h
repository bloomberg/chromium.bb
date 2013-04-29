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

#include "bindings/v8/ScriptState.h"
#include "core/css/CSSImportRule.h"
#include "core/css/CSSRule.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSStyleSheet.h"
#include "core/dom/Element.h"
#include "core/dom/EventContext.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "core/page/ConsoleTypes.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/platform/network/FormData.h"
#include "core/rendering/HitTestResult.h"
#include "core/storage/StorageArea.h"
#include "modules/websockets/WebSocketFrame.h"
#include "modules/websockets/WebSocketHandshakeRequest.h"
#include "modules/websockets/WebSocketHandshakeResponse.h"
#include <wtf/RefPtr.h>
#include <wtf/UnusedParam.h>
#include <wtf/Vector.h>

namespace WebCore {

class CSSRule;
class CachedResource;
class CharacterData;
class DOMWindow;
class DOMWrapperWorld;
class Database;
class Document;
class Element;
class EventContext;
class DocumentLoader;
class DocumentStyleSheetCollection;
class DeviceOrientationData;
class GeolocationPosition;
class GraphicsContext;
class InspectorCSSAgent;
class InspectorCSSOMWrappers;
class InspectorTimelineAgent;
class InstrumentingAgents;
class KURL;
class Node;
class PseudoElement;
class RenderLayer;
class RenderLayerBacking;
class RenderObject;
class ResourceRequest;
class ResourceResponse;
class ScriptArguments;
class ScriptCallStack;
class ScriptExecutionContext;
class ScriptObject;
class ScriptProfile;
class SecurityOrigin;
class ShadowRoot;
class StorageArea;
class StyleResolver;
class StyleRule;
class StyleSheet;
class ThreadableLoaderClient;
class WorkerContext;
class WorkerContextProxy;
class XMLHttpRequest;

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

InstrumentingAgents* instrumentingAgentsForPage(Page*);
InstrumentingAgents* instrumentingAgentsForFrame(Frame*);
InstrumentingAgents* instrumentingAgentsForContext(ScriptExecutionContext*);
InstrumentingAgents* instrumentingAgentsForDocument(Document*);
InstrumentingAgents* instrumentingAgentsForRenderer(RenderObject*);
InstrumentingAgents* instrumentingAgentsForElement(Element*);

InstrumentingAgents* instrumentingAgentsForWorkerContext(WorkerContext*);
InstrumentingAgents* instrumentingAgentsForNonDocumentContext(ScriptExecutionContext*);

}  // namespace InspectorInstrumentation

namespace InstrumentationEvents {
extern const char PaintLayer[];
extern const char RasterTask[];
extern const char Paint[];
extern const char Layer[];
extern const char BeginFrame[];
};

namespace InstrumentationEventArguments {
extern const char LayerId[];
extern const char PageId[];
};

namespace InspectorInstrumentation {

inline InstrumentingAgents* instrumentingAgentsForContext(ScriptExecutionContext* context)
{
    if (!context)
        return 0;
    if (context->isDocument())
        return instrumentingAgentsForPage(toDocument(context)->page());
    return instrumentingAgentsForNonDocumentContext(context);
}

inline InstrumentingAgents* instrumentingAgentsForFrame(Frame* frame)
{
    if (frame)
        return instrumentingAgentsForPage(frame->page());
    return 0;
}

inline InstrumentingAgents* instrumentingAgentsForDocument(Document* document)
{
    if (document) {
        Page* page = document->page();
        if (!page && document->templateDocumentHost())
            page = document->templateDocumentHost()->page();
        return instrumentingAgentsForPage(page);
    }
    return 0;
}

inline InstrumentingAgents* instrumentingAgentsForElement(Element* element)
{
    return instrumentingAgentsForDocument(element->document());
}

} // namespace InspectorInstrumentation

} // namespace WebCore

// This file will soon be generated
#include "InspectorInstrumentationInl.h"

#include "InspectorInstrumentationCustomInl.h"

#include "InspectorOverridesInl.h"

#endif // !defined(InspectorInstrumentation_h)
