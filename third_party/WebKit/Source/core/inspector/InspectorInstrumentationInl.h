/*
* Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef InspectorInstrumentation_inl_h
#define InspectorInstrumentation_inl_h

namespace WebCore {

namespace InspectorInstrumentation {

void didClearWindowObjectInWorldImpl(InstrumentingAgents*, Frame*, DOMWrapperWorld*);
void willInsertDOMNodeImpl(InstrumentingAgents*, Node* parent);
void didInsertDOMNodeImpl(InstrumentingAgents*, Node*);
void willModifyDOMAttrImpl(InstrumentingAgents*, Element*, const AtomicString& oldValue, const AtomicString& newValue);
void didModifyDOMAttrImpl(InstrumentingAgents*, Element*, const AtomicString& name, const AtomicString& value);
void didRemoveDOMAttrImpl(InstrumentingAgents*, Element*, const AtomicString& name);
void characterDataModifiedImpl(InstrumentingAgents*, CharacterData*);
void didInvalidateStyleAttrImpl(InstrumentingAgents*, Node*);
void activeStyleSheetsUpdatedImpl(InstrumentingAgents*, Document*, const Vector<RefPtr<StyleSheet> >&);
void frameWindowDiscardedImpl(InstrumentingAgents*, DOMWindow*);
void mediaQueryResultChangedImpl(InstrumentingAgents*);
void didCreateNamedFlowImpl(InstrumentingAgents*, Document*, NamedFlow*);
void willRemoveNamedFlowImpl(InstrumentingAgents*, Document*, NamedFlow*);
void didUpdateRegionLayoutImpl(InstrumentingAgents*, Document*, NamedFlow*);
void willSendXMLHttpRequestImpl(InstrumentingAgents*, const String& url);
void didScheduleResourceRequestImpl(InstrumentingAgents*, Document*, const String& url);
void didInstallTimerImpl(InstrumentingAgents*, ScriptExecutionContext*, int timerId, int timeout, bool singleShot);
void didRemoveTimerImpl(InstrumentingAgents*, ScriptExecutionContext*, int timerId);
InspectorInstrumentationCookie willCallFunctionImpl(InstrumentingAgents*, ScriptExecutionContext*, const String& scriptName, int scriptLine);
void didCallFunctionImpl(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willDispatchXHRReadyStateChangeEventImpl(InstrumentingAgents*, ScriptExecutionContext*, XMLHttpRequest*);
void didDispatchXHRReadyStateChangeEventImpl(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willDispatchEventImpl(InstrumentingAgents*, Document*, const Event&, DOMWindow*, Node*, const EventPath&);
void didDispatchEventImpl(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willHandleEventImpl(InstrumentingAgents*, Event*);
void didHandleEventImpl(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willDispatchEventOnWindowImpl(InstrumentingAgents*, const Event&, DOMWindow*);
void didDispatchEventOnWindowImpl(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willEvaluateScriptImpl(InstrumentingAgents*, Frame*, const String& url, int lineNumber);
void didEvaluateScriptImpl(const InspectorInstrumentationCookie&);
void scriptsEnabledImpl(InstrumentingAgents*, bool isEnabled);
void didCreateIsolatedContextImpl(InstrumentingAgents*, Frame*, ScriptState*, SecurityOrigin*);
InspectorInstrumentationCookie willFireTimerImpl(InstrumentingAgents*, ScriptExecutionContext*, int timerId);
void didFireTimerImpl(const InspectorInstrumentationCookie&);
void didInvalidateLayoutImpl(InstrumentingAgents*, Frame*);
InspectorInstrumentationCookie willLayoutImpl(InstrumentingAgents*, Frame*);
void didLayoutImpl(const InspectorInstrumentationCookie&, RenderObject*);
void didScrollImpl(InstrumentingAgents*);
void didResizeMainFrameImpl(InstrumentingAgents*);
InspectorInstrumentationCookie willDispatchXHRLoadEventImpl(InstrumentingAgents*, ScriptExecutionContext*, XMLHttpRequest*);
void didDispatchXHRLoadEventImpl(const InspectorInstrumentationCookie&);
void willScrollLayerImpl(InstrumentingAgents*, Frame*);
void didScrollLayerImpl(InstrumentingAgents*);
void willPaintImpl(InstrumentingAgents*, RenderObject*);
void didPaintImpl(InstrumentingAgents*, RenderObject*, GraphicsContext*, const LayoutRect&);
InspectorInstrumentationCookie willRecalculateStyleImpl(InstrumentingAgents*, Document*);
void didRecalculateStyleImpl(const InspectorInstrumentationCookie&);
void didRecalculateStyleForElementImpl(InstrumentingAgents*);
void didScheduleStyleRecalculationImpl(InstrumentingAgents*, Document*);
InspectorInstrumentationCookie willMatchRuleImpl(InstrumentingAgents*, StyleRule*, InspectorCSSOMWrappers&, DocumentStyleSheetCollection*);
void didMatchRuleImpl(const InspectorInstrumentationCookie&, bool matched);
void didProcessRuleImpl(const InspectorInstrumentationCookie&);
void applyUserAgentOverrideImpl(InstrumentingAgents*, String*);
void applyScreenWidthOverrideImpl(InstrumentingAgents*, long*);
void applyScreenHeightOverrideImpl(InstrumentingAgents*, long*);
void applyEmulatedMediaImpl(InstrumentingAgents*, String*);
void willSendRequestImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
void continueAfterPingLoaderImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse&);
void markResourceAsCachedImpl(InstrumentingAgents*, unsigned long identifier);
void didLoadResourceFromMemoryCacheImpl(InstrumentingAgents*, DocumentLoader*, CachedResource*);
InspectorInstrumentationCookie willReceiveResourceDataImpl(InstrumentingAgents*, Frame*, unsigned long identifier, int length);
void didReceiveResourceDataImpl(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willReceiveResourceResponseImpl(InstrumentingAgents*, Frame*, unsigned long identifier, const ResourceResponse&);
void didReceiveResourceResponseImpl(const InspectorInstrumentationCookie&, unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
void didReceiveDataImpl(InstrumentingAgents*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
void didFinishLoadingImpl(InstrumentingAgents*, DocumentLoader*, unsigned long identifier, double finishTime);
void didFailLoadingImpl(InstrumentingAgents*, DocumentLoader*, unsigned long identifier, const ResourceError&);
void documentThreadableLoaderStartedLoadingForClientImpl(InstrumentingAgents*, unsigned long identifier, ThreadableLoaderClient*);
void willLoadXHRImpl(InstrumentingAgents*, ThreadableLoaderClient*, const String&, const KURL&, bool, PassRefPtr<FormData>, const HTTPHeaderMap&, bool);
void didFailXHRLoadingImpl(InstrumentingAgents*, ThreadableLoaderClient*);
void didFinishXHRLoadingImpl(InstrumentingAgents*, ThreadableLoaderClient*, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber);
void didReceiveXHRResponseImpl(InstrumentingAgents*, unsigned long identifier);
void willLoadXHRSynchronouslyImpl(InstrumentingAgents*);
void didLoadXHRSynchronouslyImpl(InstrumentingAgents*);
void scriptImportedImpl(InstrumentingAgents*, unsigned long identifier, const String& sourceString);
void scriptExecutionBlockedByCSPImpl(InstrumentingAgents*, const String& directiveText);
void didReceiveScriptResponseImpl(InstrumentingAgents*, unsigned long identifier);
void domContentLoadedEventFiredImpl(InstrumentingAgents*, Frame*);
void loadEventFiredImpl(InstrumentingAgents*, Frame*);
void frameDetachedFromParentImpl(InstrumentingAgents*, Frame*);
void didCommitLoadImpl(InstrumentingAgents*, Frame*, DocumentLoader*);
void frameDocumentUpdatedImpl(InstrumentingAgents*, Frame*);
void loaderDetachedFromFrameImpl(InstrumentingAgents*, DocumentLoader*);
void frameStartedLoadingImpl(InstrumentingAgents*, Frame*);
void frameStoppedLoadingImpl(InstrumentingAgents*, Frame*);
void frameScheduledNavigationImpl(InstrumentingAgents*, Frame*, double delay);
void frameClearedScheduledNavigationImpl(InstrumentingAgents*, Frame*);
InspectorInstrumentationCookie willRunJavaScriptDialogImpl(InstrumentingAgents*, const String& message);
void didRunJavaScriptDialogImpl(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willWriteHTMLImpl(InstrumentingAgents*, Document*, unsigned startLine);
void didWriteHTMLImpl(const InspectorInstrumentationCookie&, unsigned endLine);
void didRequestAnimationFrameImpl(InstrumentingAgents*, Document*, int callbackId);
void didCancelAnimationFrameImpl(InstrumentingAgents*, Document*, int callbackId);
InspectorInstrumentationCookie willFireAnimationFrameImpl(InstrumentingAgents*, Document*, int callbackId);
void didFireAnimationFrameImpl(const InspectorInstrumentationCookie&);
void didStartWorkerContextImpl(InstrumentingAgents*, WorkerContextProxy*, const KURL&);
void workerContextTerminatedImpl(InstrumentingAgents*, WorkerContextProxy*);
void didCreateWebSocketImpl(InstrumentingAgents*, Document*, unsigned long identifier, const KURL& requestURL, const KURL& documentURL, const String& protocol);
void willSendWebSocketHandshakeRequestImpl(InstrumentingAgents*, Document*, unsigned long identifier, const WebSocketHandshakeRequest&);
void didReceiveWebSocketHandshakeResponseImpl(InstrumentingAgents*, Document*, unsigned long identifier, const WebSocketHandshakeResponse&);
void didCloseWebSocketImpl(InstrumentingAgents*, Document*, unsigned long identifier);
void didReceiveWebSocketFrameImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketFrame&);
void didSendWebSocketFrameImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketFrame&);
void didReceiveWebSocketFrameErrorImpl(InstrumentingAgents*, unsigned long identifier, const String&);
void networkStateChangedImpl(InstrumentingAgents*);
void updateApplicationCacheStatusImpl(InstrumentingAgents*, Frame*);
void layerTreeDidChangeImpl(InstrumentingAgents*);
void renderLayerDestroyedImpl(InstrumentingAgents*, const RenderLayer*);
void pseudoElementDestroyedImpl(InstrumentingAgents*, PseudoElement*);

inline void didClearWindowObjectInWorld(Frame* frame, DOMWrapperWorld* world)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didClearWindowObjectInWorldImpl(instrumentingAgents, frame, world);
}

inline void willInsertDOMNode(Document* document, Node* parent)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willInsertDOMNodeImpl(instrumentingAgents, parent);
}

inline void didInsertDOMNode(Document* document, Node* node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didInsertDOMNodeImpl(instrumentingAgents, node);
}


inline void willModifyDOMAttr(Document* document, Element* element, const AtomicString& oldValue, const AtomicString& newValue)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willModifyDOMAttrImpl(instrumentingAgents, element, oldValue, newValue);
}

inline void didModifyDOMAttr(Document* document, Element* element, const AtomicString& name, const AtomicString& value)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didModifyDOMAttrImpl(instrumentingAgents, element, name, value);
}

inline void didRemoveDOMAttr(Document* document, Element* element, const AtomicString& name)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didRemoveDOMAttrImpl(instrumentingAgents, element, name);
}
inline void characterDataModified(Document* document, CharacterData* characterData)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        characterDataModifiedImpl(instrumentingAgents, characterData);
}

inline void didInvalidateStyleAttr(Document* document, Node* node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didInvalidateStyleAttrImpl(instrumentingAgents, node);
}

inline void activeStyleSheetsUpdated(Document* document, const Vector<RefPtr<StyleSheet> >& newSheets)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        activeStyleSheetsUpdatedImpl(instrumentingAgents, document, newSheets);
}

inline void frameWindowDiscarded(Frame* frame, DOMWindow* domWindow)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameWindowDiscardedImpl(instrumentingAgents, domWindow);
}

inline void mediaQueryResultChanged(Document* document)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        mediaQueryResultChangedImpl(instrumentingAgents);
}


inline void didCreateNamedFlow(Document* document, NamedFlow* namedFlow)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCreateNamedFlowImpl(instrumentingAgents, document, namedFlow);
}
inline void willRemoveNamedFlow(Document* document, NamedFlow* namedFlow)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willRemoveNamedFlowImpl(instrumentingAgents, document, namedFlow);
}

inline void didUpdateRegionLayout(Document* document, NamedFlow* namedFlow)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didUpdateRegionLayoutImpl(instrumentingAgents, document, namedFlow);
}

inline void willSendXMLHttpRequest(ScriptExecutionContext* context, const String& url)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        willSendXMLHttpRequestImpl(instrumentingAgents, url);
}

inline void didScheduleResourceRequest(Document* document, const String& url)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didScheduleResourceRequestImpl(instrumentingAgents, document, url);
}

inline void didInstallTimer(ScriptExecutionContext* context, int timerId, int timeout, bool singleShot)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didInstallTimerImpl(instrumentingAgents, context, timerId, timeout, singleShot);
}

inline void didRemoveTimer(ScriptExecutionContext* context, int timerId)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didRemoveTimerImpl(instrumentingAgents, context, timerId);
}

inline InspectorInstrumentationCookie willCallFunction(ScriptExecutionContext* context, const String& scriptName, int scriptLine)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willCallFunctionImpl(instrumentingAgents, context, scriptName, scriptLine);
    return InspectorInstrumentationCookie();
}

inline void didCallFunction(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didCallFunctionImpl(cookie);
}

inline InspectorInstrumentationCookie willDispatchXHRReadyStateChangeEvent(ScriptExecutionContext* context, XMLHttpRequest* request)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willDispatchXHRReadyStateChangeEventImpl(instrumentingAgents, context, request);
    return InspectorInstrumentationCookie();
}

inline void didDispatchXHRReadyStateChangeEvent(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didDispatchXHRReadyStateChangeEventImpl(cookie);
}

inline InspectorInstrumentationCookie willDispatchEvent(Document* document, const Event& event, DOMWindow* window, Node* node, const EventPath& eventPath)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willDispatchEventImpl(instrumentingAgents, document, event, window, node, eventPath);
    return InspectorInstrumentationCookie();
}

inline void didDispatchEvent(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didDispatchEventImpl(cookie);
}

inline InspectorInstrumentationCookie willHandleEvent(ScriptExecutionContext* context, Event* event)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willHandleEventImpl(instrumentingAgents, event);
    return InspectorInstrumentationCookie();
}

inline void didHandleEvent(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didHandleEventImpl(cookie);
}

inline InspectorInstrumentationCookie willDispatchEventOnWindow(Frame* frame, const Event& event, DOMWindow* window)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willDispatchEventOnWindowImpl(instrumentingAgents, event, window);
    return InspectorInstrumentationCookie();
}

inline void didDispatchEventOnWindow(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didDispatchEventOnWindowImpl(cookie);
}

inline InspectorInstrumentationCookie willEvaluateScript(Frame* frame, const String& url, int lineNumber)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willEvaluateScriptImpl(instrumentingAgents, frame, url, lineNumber);
    return InspectorInstrumentationCookie();
}

inline void didEvaluateScript(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didEvaluateScriptImpl(cookie);
}

inline void scriptsEnabled(Page* page, bool isEnabled)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        scriptsEnabledImpl(instrumentingAgents, isEnabled);
}

inline void didCreateIsolatedContext(Frame* frame, ScriptState* scriptState, SecurityOrigin* origin)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didCreateIsolatedContextImpl(instrumentingAgents, frame, scriptState, origin);
}

inline InspectorInstrumentationCookie willFireTimer(ScriptExecutionContext* context, int timerId)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willFireTimerImpl(instrumentingAgents, context, timerId);
    return InspectorInstrumentationCookie();
}

inline void didFireTimer(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didFireTimerImpl(cookie);
}

inline void didInvalidateLayout(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didInvalidateLayoutImpl(instrumentingAgents, frame);
}

inline InspectorInstrumentationCookie willLayout(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willLayoutImpl(instrumentingAgents, frame);
    return InspectorInstrumentationCookie();
}

inline void didLayout(const InspectorInstrumentationCookie& cookie, RenderObject* root)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didLayoutImpl(cookie, root);
}

inline void didScroll(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didScrollImpl(instrumentingAgents);
}

inline void didResizeMainFrame(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didResizeMainFrameImpl(instrumentingAgents);
}

inline InspectorInstrumentationCookie willDispatchXHRLoadEvent(ScriptExecutionContext* context, XMLHttpRequest* request)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willDispatchXHRLoadEventImpl(instrumentingAgents, context, request);
    return InspectorInstrumentationCookie();
}

inline void didDispatchXHRLoadEvent(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didDispatchXHRLoadEventImpl(cookie);
}

inline void willScrollLayer(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        willScrollLayerImpl(instrumentingAgents, frame);
}

inline void didScrollLayer(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didScrollLayerImpl(instrumentingAgents);
}

inline void willPaint(RenderObject* renderer)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForRenderer(renderer))
        willPaintImpl(instrumentingAgents, renderer);
}

inline void didPaint(RenderObject* renderer, GraphicsContext* context, const LayoutRect& rect)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForRenderer(renderer))
        didPaintImpl(instrumentingAgents, renderer, context, rect);
}

inline InspectorInstrumentationCookie willRecalculateStyle(Document* document)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willRecalculateStyleImpl(instrumentingAgents, document);
    return InspectorInstrumentationCookie();
}

inline void didRecalculateStyle(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didRecalculateStyleImpl(cookie);
}

inline void didRecalculateStyleForElement(Element* element)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForElement(element))
        didRecalculateStyleForElementImpl(instrumentingAgents);
}

inline void didScheduleStyleRecalculation(Document* document)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didScheduleStyleRecalculationImpl(instrumentingAgents, document);
}

inline InspectorInstrumentationCookie willMatchRule(Document* document, StyleRule* rule, InspectorCSSOMWrappers& inspectorCSSOMWrappers, DocumentStyleSheetCollection* styleSheetCollection)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willMatchRuleImpl(instrumentingAgents, rule, inspectorCSSOMWrappers, styleSheetCollection);
    return InspectorInstrumentationCookie();
}

inline void didMatchRule(const InspectorInstrumentationCookie& cookie, bool matched)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didMatchRuleImpl(cookie, matched);
}

inline void didProcessRule(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didProcessRuleImpl(cookie);
}

inline void applyUserAgentOverride(Frame* frame, String* userAgent)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyUserAgentOverrideImpl(instrumentingAgents, userAgent);
}

inline void applyScreenWidthOverride(Frame* frame, long* width)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyScreenWidthOverrideImpl(instrumentingAgents, width);
}

inline void applyScreenHeightOverride(Frame* frame, long* height)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyScreenHeightOverrideImpl(instrumentingAgents, height);
}

inline void applyEmulatedMedia(Frame* frame, String* media)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyEmulatedMediaImpl(instrumentingAgents, media);
}

inline void willSendRequest(Frame* frame, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        willSendRequestImpl(instrumentingAgents, identifier, loader, request, redirectResponse);
}

inline void continueAfterPingLoader(Frame* frame, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& response)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        continueAfterPingLoaderImpl(instrumentingAgents, identifier, loader, request, response);
}

inline void markResourceAsCached(Page* page, unsigned long identifier)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        markResourceAsCachedImpl(instrumentingAgents, identifier);
}

inline void didLoadResourceFromMemoryCache(Page* page, DocumentLoader* loader, CachedResource* resource)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didLoadResourceFromMemoryCacheImpl(instrumentingAgents, loader, resource);
}

inline void didReceiveResourceData(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didReceiveResourceDataImpl(cookie);
}

inline InspectorInstrumentationCookie willReceiveResourceData(Frame* frame, unsigned long identifier, int length)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willReceiveResourceDataImpl(instrumentingAgents, frame, identifier, length);
    return InspectorInstrumentationCookie();
}

inline InspectorInstrumentationCookie willReceiveResourceResponse(Frame* frame, unsigned long identifier, const ResourceResponse& response)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willReceiveResourceResponseImpl(instrumentingAgents, frame, identifier, response);
    return InspectorInstrumentationCookie();
}

inline void didReceiveResourceResponse(const InspectorInstrumentationCookie& cookie, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    // Call this unconditionally so that we're able to log to console with no front-end attached.
    if (cookie.isValid())
        didReceiveResourceResponseImpl(cookie, identifier, loader, response, resourceLoader);
}

inline void didReceiveData(Frame* frame, unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didReceiveDataImpl(instrumentingAgents, identifier, data, dataLength, encodedDataLength);
}

inline void didFinishLoading(Frame* frame, DocumentLoader* loader, unsigned long identifier, double finishTime)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didFinishLoadingImpl(instrumentingAgents, loader, identifier, finishTime);
}

inline void didFailLoading(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceError& error)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didFailLoadingImpl(instrumentingAgents, loader, identifier, error);
}

inline void documentThreadableLoaderStartedLoadingForClient(ScriptExecutionContext* context, unsigned long identifier, ThreadableLoaderClient* client)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        documentThreadableLoaderStartedLoadingForClientImpl(instrumentingAgents, identifier, client);
}

inline void willLoadXHR(ScriptExecutionContext* context, ThreadableLoaderClient* client, const String& method, const KURL& url, bool async, PassRefPtr<FormData> formData, const HTTPHeaderMap& headers, bool includeCredentials)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        willLoadXHRImpl(instrumentingAgents, client, method, url, async, formData, headers, includeCredentials);
}

inline void didFailXHRLoading(ScriptExecutionContext* context, ThreadableLoaderClient* client)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didFailXHRLoadingImpl(instrumentingAgents, client);
}

inline void didFinishXHRLoading(ScriptExecutionContext* context, ThreadableLoaderClient* client, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didFinishXHRLoadingImpl(instrumentingAgents, client, identifier, sourceString, url, sendURL, sendLineNumber);
}

inline void didReceiveXHRResponse(ScriptExecutionContext* context, unsigned long identifier)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didReceiveXHRResponseImpl(instrumentingAgents, identifier);
}

inline void willLoadXHRSynchronously(ScriptExecutionContext* context)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        willLoadXHRSynchronouslyImpl(instrumentingAgents);
}

inline void didLoadXHRSynchronously(ScriptExecutionContext* context)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didLoadXHRSynchronouslyImpl(instrumentingAgents);
}

inline void scriptImported(ScriptExecutionContext* context, unsigned long identifier, const String& sourceString)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        scriptImportedImpl(instrumentingAgents, identifier, sourceString);
}

inline void scriptExecutionBlockedByCSP(ScriptExecutionContext* context, const String& directiveText)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        scriptExecutionBlockedByCSPImpl(instrumentingAgents, directiveText);
}

inline void didReceiveScriptResponse(ScriptExecutionContext* context, unsigned long identifier)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didReceiveScriptResponseImpl(instrumentingAgents, identifier);
}

inline void domContentLoadedEventFired(Frame* frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        domContentLoadedEventFiredImpl(instrumentingAgents, frame);
}

inline void loadEventFired(Frame* frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        loadEventFiredImpl(instrumentingAgents, frame);
}

inline void frameDetachedFromParent(Frame* frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameDetachedFromParentImpl(instrumentingAgents, frame);
}

inline void didCommitLoad(Frame* frame, DocumentLoader* loader)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didCommitLoadImpl(instrumentingAgents, frame, loader);
}

inline void frameDocumentUpdated(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameDocumentUpdatedImpl(instrumentingAgents, frame);
}

inline void loaderDetachedFromFrame(Frame* frame, DocumentLoader* loader)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        loaderDetachedFromFrameImpl(instrumentingAgents, loader);
}

inline void frameStartedLoading(Frame* frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameStartedLoadingImpl(instrumentingAgents, frame);
}

inline void frameStoppedLoading(Frame* frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameStoppedLoadingImpl(instrumentingAgents, frame);
}

inline void frameScheduledNavigation(Frame* frame, double delay)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameScheduledNavigationImpl(instrumentingAgents, frame, delay);
}

inline void frameClearedScheduledNavigation(Frame* frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameClearedScheduledNavigationImpl(instrumentingAgents, frame);
}

inline InspectorInstrumentationCookie willRunJavaScriptDialog(Page* page, const String& message)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return willRunJavaScriptDialogImpl(instrumentingAgents, message);
    return InspectorInstrumentationCookie();
}

inline void didRunJavaScriptDialog(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didRunJavaScriptDialogImpl(cookie);
}

inline InspectorInstrumentationCookie willWriteHTML(Document* document, unsigned startLine)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willWriteHTMLImpl(instrumentingAgents, document, startLine);
    return InspectorInstrumentationCookie();
}

inline void didWriteHTML(const InspectorInstrumentationCookie& cookie, unsigned endLine)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didWriteHTMLImpl(cookie, endLine);
}

inline void didRequestAnimationFrame(Document* document, int callbackId)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didRequestAnimationFrameImpl(instrumentingAgents, document, callbackId);
}

inline void didCancelAnimationFrame(Document* document, int callbackId)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCancelAnimationFrameImpl(instrumentingAgents, document, callbackId);
}

inline InspectorInstrumentationCookie willFireAnimationFrame(Document* document, int callbackId)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willFireAnimationFrameImpl(instrumentingAgents, document, callbackId);
    return InspectorInstrumentationCookie();
}

inline void didFireAnimationFrame(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didFireAnimationFrameImpl(cookie);
}

inline void didStartWorkerContext(ScriptExecutionContext* context, WorkerContextProxy* proxy, const KURL& url)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didStartWorkerContextImpl(instrumentingAgents, proxy, url);
}

inline void workerContextTerminated(ScriptExecutionContext* context, WorkerContextProxy* proxy)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        workerContextTerminatedImpl(instrumentingAgents, proxy);
}

inline void didCreateWebSocket(Document* document, unsigned long identifier, const KURL& requestURL, const KURL& documentURL, const String& protocol)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCreateWebSocketImpl(instrumentingAgents, document, identifier, requestURL, documentURL, protocol);
}

inline void willSendWebSocketHandshakeRequest(Document* document, unsigned long identifier, const WebSocketHandshakeRequest& request)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willSendWebSocketHandshakeRequestImpl(instrumentingAgents, document, identifier, request);
}

inline void didReceiveWebSocketHandshakeResponse(Document* document, unsigned long identifier, const WebSocketHandshakeResponse& response)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didReceiveWebSocketHandshakeResponseImpl(instrumentingAgents, document, identifier, response);
}

inline void didCloseWebSocket(Document* document, unsigned long identifier)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCloseWebSocketImpl(instrumentingAgents, document, identifier);
}

inline void didReceiveWebSocketFrame(Document* document, unsigned long identifier, const WebSocketFrame& frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didReceiveWebSocketFrameImpl(instrumentingAgents, identifier, frame);
}

inline void didSendWebSocketFrame(Document* document, unsigned long identifier, const WebSocketFrame& frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didSendWebSocketFrameImpl(instrumentingAgents, identifier, frame);
}

inline void didReceiveWebSocketFrameError(Document* document, unsigned long identifier, const String& errorMessage)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didReceiveWebSocketFrameErrorImpl(instrumentingAgents, identifier, errorMessage);
}

inline void networkStateChanged(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        networkStateChangedImpl(instrumentingAgents);
}

inline void updateApplicationCacheStatus(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        updateApplicationCacheStatusImpl(instrumentingAgents, frame);
}

inline void layerTreeDidChange(Page* page)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        layerTreeDidChangeImpl(instrumentingAgents);
}

inline void renderLayerDestroyed(Page* page, const RenderLayer* renderLayer)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        renderLayerDestroyedImpl(instrumentingAgents, renderLayer);
}

inline void pseudoElementDestroyed(Page* page, PseudoElement* pseudoElement)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        pseudoElementDestroyedImpl(instrumentingAgents, pseudoElement);
}

} // namespace InspectorInstrumentation

} // namespace WebCore

#endif // !defined(InspectorInstrumentation_inl_h)
