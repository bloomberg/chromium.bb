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

#include "CSSSelector.h"
#include "ConsoleAPITypes.h"
#include "ConsoleTypes.h"
#include "Element.h"
#include "EventContext.h"
#include "FormData.h"
#include "Frame.h"
#include "HitTestResult.h"
#include "Page.h"
#include "ScriptExecutionContext.h"
#include "ScriptState.h"
#include "StorageArea.h"
#include "WebSocketFrame.h"
#include "WebSocketHandshakeRequest.h"
#include "WebSocketHandshakeResponse.h"
#include <wtf/RefPtr.h>
#include <wtf/UnusedParam.h>

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
class InspectorInstrumentation;
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

private:
    friend class InspectorInstrumentation;
    InstrumentingAgents* instrumentingAgents() const { return m_instrumentingAgents.get(); }
    bool isValid() const { return !!m_instrumentingAgents; }
    bool hasMatchingTimelineAgentId(int id) const { return m_timelineAgentId == id; }

    RefPtr<InstrumentingAgents> m_instrumentingAgents;
    int m_timelineAgentId;
};

class InspectorInstrumentation {
public:
    static void didClearWindowObjectInWorld(Frame*, DOMWrapperWorld*);
    static bool isDebuggerPaused(Frame*);

    static void willInsertDOMNode(Document*, Node* parent);
    static void didInsertDOMNode(Document*, Node*);
    static void willRemoveDOMNode(Document*, Node*);
    static void willModifyDOMAttr(Document*, Element*, const AtomicString& oldValue, const AtomicString& newValue);
    static void didModifyDOMAttr(Document*, Element*, const AtomicString& name, const AtomicString& value);
    static void didRemoveDOMAttr(Document*, Element*, const AtomicString& name);
    static void characterDataModified(Document*, CharacterData*);
    static void didInvalidateStyleAttr(Document*, Node*);
    static void frameWindowDiscarded(Frame*, DOMWindow*);
    static void mediaQueryResultChanged(Document*);
    static void didPushShadowRoot(Element* host, ShadowRoot*);
    static void willPopShadowRoot(Element* host, ShadowRoot*);
    static void didCreateNamedFlow(Document*, NamedFlow*);
    static void willRemoveNamedFlow(Document*, NamedFlow*);
    static void didUpdateRegionLayout(Document*, NamedFlow*);

    static void mouseDidMoveOverElement(Page*, const HitTestResult&, unsigned modifierFlags);
    static bool handleMousePress(Page*);
    static bool handleTouchEvent(Page*, Node*);
    static bool forcePseudoState(Element*, CSSSelector::PseudoType);

    static void willSendXMLHttpRequest(ScriptExecutionContext*, const String& url);
    static void didScheduleResourceRequest(Document*, const String& url);
    static void didInstallTimer(ScriptExecutionContext*, int timerId, int timeout, bool singleShot);
    static void didRemoveTimer(ScriptExecutionContext*, int timerId);

    static InspectorInstrumentationCookie willCallFunction(ScriptExecutionContext*, const String& scriptName, int scriptLine);
    static void didCallFunction(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchXHRReadyStateChangeEvent(ScriptExecutionContext*, XMLHttpRequest*);
    static void didDispatchXHRReadyStateChangeEvent(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEvent(Document*, const Event&, DOMWindow*, Node*, const EventPath&);
    static void didDispatchEvent(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willHandleEvent(ScriptExecutionContext*, Event*);
    static void didHandleEvent(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEventOnWindow(Frame*, const Event& event, DOMWindow* window);
    static void didDispatchEventOnWindow(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willEvaluateScript(Frame*, const String& url, int lineNumber);
    static void didEvaluateScript(const InspectorInstrumentationCookie&);
    static void scriptsEnabled(Page*, bool isEnabled);
    static void didCreateIsolatedContext(Frame*, ScriptState*, SecurityOrigin*);
    static InspectorInstrumentationCookie willFireTimer(ScriptExecutionContext*, int timerId);
    static void didFireTimer(const InspectorInstrumentationCookie&);
    static void didInvalidateLayout(Frame*);
    static InspectorInstrumentationCookie willLayout(Frame*);
    static void didLayout(const InspectorInstrumentationCookie&, RenderObject*);
    static void didScroll(Page*);
    static InspectorInstrumentationCookie willDispatchXHRLoadEvent(ScriptExecutionContext*, XMLHttpRequest*);
    static void didDispatchXHRLoadEvent(const InspectorInstrumentationCookie&);
    static void willScrollLayer(Frame*);
    static void didScrollLayer(Frame*);
    static void willPaint(RenderObject*);
    static void didPaint(RenderObject*, GraphicsContext*, const LayoutRect&);
    static void willComposite(Page*);
    static void didComposite(Page*);
    static InspectorInstrumentationCookie willRecalculateStyle(Document*);
    static void didRecalculateStyle(const InspectorInstrumentationCookie&);
    static void didRecalculateStyleForElement(Element*);

    static void didScheduleStyleRecalculation(Document*);
    static InspectorInstrumentationCookie willMatchRule(Document*, StyleRule*, InspectorCSSOMWrappers&, DocumentStyleSheetCollection*);
    static void didMatchRule(const InspectorInstrumentationCookie&, bool matched);
    static InspectorInstrumentationCookie willProcessRule(Document*, StyleRule*, StyleResolver*);
    static void didProcessRule(const InspectorInstrumentationCookie&);

    static void applyUserAgentOverride(Frame*, String*);
    static void applyScreenWidthOverride(Frame*, long*);
    static void applyScreenHeightOverride(Frame*, long*);
    static void applyEmulatedMedia(Frame*, String*);
    static bool shouldApplyScreenWidthOverride(Frame*);
    static bool shouldApplyScreenHeightOverride(Frame*);
    static void willSendRequest(Frame*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
    static void continueAfterPingLoader(Frame*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse&);
    static void markResourceAsCached(Page*, unsigned long identifier);
    static void didLoadResourceFromMemoryCache(Page*, DocumentLoader*, CachedResource*);
    static InspectorInstrumentationCookie willReceiveResourceData(Frame*, unsigned long identifier, int length);
    static void didReceiveResourceData(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willReceiveResourceResponse(Frame*, unsigned long identifier, const ResourceResponse&);
    static void didReceiveResourceResponse(const InspectorInstrumentationCookie&, unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    static void continueAfterXFrameOptionsDenied(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyDownload(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyIgnore(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void didReceiveData(Frame*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
    static void didFinishLoading(Frame*, DocumentLoader*, unsigned long identifier, double finishTime);
    static void didFailLoading(Frame*, DocumentLoader*, unsigned long identifier, const ResourceError&);
    static void documentThreadableLoaderStartedLoadingForClient(ScriptExecutionContext*, unsigned long identifier, ThreadableLoaderClient*);
    static void willLoadXHR(ScriptExecutionContext*, ThreadableLoaderClient*, const String&, const KURL&, bool, PassRefPtr<FormData>, const HTTPHeaderMap&, bool);
    static void didFailXHRLoading(ScriptExecutionContext*, ThreadableLoaderClient*);
    static void didFinishXHRLoading(ScriptExecutionContext*, ThreadableLoaderClient*, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber);
    static void didReceiveXHRResponse(ScriptExecutionContext*, unsigned long identifier);
    static void willLoadXHRSynchronously(ScriptExecutionContext*);
    static void didLoadXHRSynchronously(ScriptExecutionContext*);
    static void scriptImported(ScriptExecutionContext*, unsigned long identifier, const String& sourceString);
    static void scriptExecutionBlockedByCSP(ScriptExecutionContext*, const String& directiveText);
    static void didReceiveScriptResponse(ScriptExecutionContext*, unsigned long identifier);
    static void domContentLoadedEventFired(Frame*);
    static void loadEventFired(Frame*);
    static void frameDetachedFromParent(Frame*);
    static void didCommitLoad(Frame*, DocumentLoader*);
    static void frameDocumentUpdated(Frame*);
    static void loaderDetachedFromFrame(Frame*, DocumentLoader*);
    static void frameStartedLoading(Frame*);
    static void frameStoppedLoading(Frame*);
    static void frameScheduledNavigation(Frame*, double delay);
    static void frameClearedScheduledNavigation(Frame*);
    static InspectorInstrumentationCookie willRunJavaScriptDialog(Page*, const String& message);
    static void didRunJavaScriptDialog(const InspectorInstrumentationCookie&);
    static void willDestroyCachedResource(CachedResource*);

    static InspectorInstrumentationCookie willWriteHTML(Document*, unsigned startLine);
    static void didWriteHTML(const InspectorInstrumentationCookie&, unsigned endLine);

    // FIXME: Remove once we no longer generate stacks outside of Inspector.
    static void addMessageToConsole(Page*, MessageSource, MessageType, MessageLevel, const String& message, PassRefPtr<ScriptCallStack>, unsigned long requestIdentifier = 0);
    static void addMessageToConsole(Page*, MessageSource, MessageType, MessageLevel, const String& message, ScriptState*, PassRefPtr<ScriptArguments>, unsigned long requestIdentifier = 0);
    static void addMessageToConsole(Page*, MessageSource, MessageType, MessageLevel, const String& message, const String&, unsigned lineNumber, ScriptState* = 0, unsigned long requestIdentifier = 0);
    // FIXME: Convert to ScriptArguments to match non-worker context.
    static void addMessageToConsole(WorkerContext*, MessageSource, MessageType, MessageLevel, const String& message, PassRefPtr<ScriptCallStack>, unsigned long requestIdentifier = 0);
    static void addMessageToConsole(WorkerContext*, MessageSource, MessageType, MessageLevel, const String& message, const String&, unsigned lineNumber, ScriptState* = 0, unsigned long requestIdentifier = 0);
    static void consoleCount(Page*, ScriptState*, PassRefPtr<ScriptArguments>);
    static void startConsoleTiming(Frame*, const String& title);
    static void stopConsoleTiming(Frame*, const String& title, PassRefPtr<ScriptCallStack>);
    static void consoleTimeStamp(Frame*, PassRefPtr<ScriptArguments>);

    static void didRequestAnimationFrame(Document*, int callbackId);
    static void didCancelAnimationFrame(Document*, int callbackId);
    static InspectorInstrumentationCookie willFireAnimationFrame(Document*, int callbackId);
    static void didFireAnimationFrame(const InspectorInstrumentationCookie&);

    static void addStartProfilingMessageToConsole(Page*, const String& title, unsigned lineNumber, const String& sourceURL);
    static void addProfile(Page*, RefPtr<ScriptProfile>, PassRefPtr<ScriptCallStack>);
    static String getCurrentUserInitiatedProfileName(Page*, bool incrementProfileNumber);
    static bool profilerEnabled(Page*);

    static void didOpenDatabase(ScriptExecutionContext*, PassRefPtr<Database>, const String& domain, const String& name, const String& version);

    static void didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType, SecurityOrigin*, Page*);

    static bool shouldPauseDedicatedWorkerOnStart(ScriptExecutionContext*);
    static void didStartWorkerContext(ScriptExecutionContext*, WorkerContextProxy*, const KURL&);
    static void workerContextTerminated(ScriptExecutionContext*, WorkerContextProxy*);
    static void willEvaluateWorkerScript(WorkerContext*, int workerThreadStartMode);

    static void didCreateWebSocket(Document*, unsigned long identifier, const KURL& requestURL, const KURL& documentURL, const String& protocol);
    static void willSendWebSocketHandshakeRequest(Document*, unsigned long identifier, const WebSocketHandshakeRequest&);
    static void didReceiveWebSocketHandshakeResponse(Document*, unsigned long identifier, const WebSocketHandshakeResponse&);
    static void didCloseWebSocket(Document*, unsigned long identifier);
    static void didReceiveWebSocketFrame(Document*, unsigned long identifier, const WebSocketFrame&);
    static void didSendWebSocketFrame(Document*, unsigned long identifier, const WebSocketFrame&);
    static void didReceiveWebSocketFrameError(Document*, unsigned long identifier, const String& errorMessage);

    static ScriptObject wrapCanvas2DRenderingContextForInstrumentation(Document*, const ScriptObject&);
    static ScriptObject wrapWebGLRenderingContextForInstrumentation(Document*, const ScriptObject&);

    static void networkStateChanged(Page*);
    static void updateApplicationCacheStatus(Frame*);

    static void frontendCreated() { s_frontendCounter += 1; }
    static void frontendDeleted() { s_frontendCounter -= 1; }
    static bool hasFrontends() { return s_frontendCounter; }
    static bool canvasAgentEnabled(ScriptExecutionContext*);
    static bool consoleAgentEnabled(ScriptExecutionContext*);
    static bool timelineAgentEnabled(ScriptExecutionContext*);
    static bool collectingHTMLParseErrors(Page*);

    static GeolocationPosition* overrideGeolocationPosition(Page*, GeolocationPosition*);

    static void registerInstrumentingAgents(InstrumentingAgents*);
    static void unregisterInstrumentingAgents(InstrumentingAgents*);

    static DeviceOrientationData* overrideDeviceOrientation(Page*, DeviceOrientationData*);

    static void layerTreeDidChange(Page*);
    static void renderLayerDestroyed(Page*, const RenderLayer*);
    static void pseudoElementDestroyed(Page*, PseudoElement*);

private:
    static void didClearWindowObjectInWorldImpl(InstrumentingAgents*, Frame*, DOMWrapperWorld*);
    static bool isDebuggerPausedImpl(InstrumentingAgents*);

    static void willInsertDOMNodeImpl(InstrumentingAgents*, Node* parent);
    static void didInsertDOMNodeImpl(InstrumentingAgents*, Node*);
    static void willRemoveDOMNodeImpl(InstrumentingAgents*, Node*);
    static void didRemoveDOMNodeImpl(InstrumentingAgents*, Node*);
    static void willModifyDOMAttrImpl(InstrumentingAgents*, Element*, const AtomicString& oldValue, const AtomicString& newValue);
    static void didModifyDOMAttrImpl(InstrumentingAgents*, Element*, const AtomicString& name, const AtomicString& value);
    static void didRemoveDOMAttrImpl(InstrumentingAgents*, Element*, const AtomicString& name);
    static void characterDataModifiedImpl(InstrumentingAgents*, CharacterData*);
    static void didInvalidateStyleAttrImpl(InstrumentingAgents*, Node*);
    static void frameWindowDiscardedImpl(InstrumentingAgents*, DOMWindow*);
    static void mediaQueryResultChangedImpl(InstrumentingAgents*);
    static void didPushShadowRootImpl(InstrumentingAgents*, Element* host, ShadowRoot*);
    static void willPopShadowRootImpl(InstrumentingAgents*, Element* host, ShadowRoot*);
    static void didCreateNamedFlowImpl(InstrumentingAgents*, Document*, NamedFlow*);
    static void willRemoveNamedFlowImpl(InstrumentingAgents*, Document*, NamedFlow*);
    static void didUpdateRegionLayoutImpl(InstrumentingAgents*, Document*, NamedFlow*);

    static void mouseDidMoveOverElementImpl(InstrumentingAgents*, const HitTestResult&, unsigned modifierFlags);
    static bool handleTouchEventImpl(InstrumentingAgents*, Node*);
    static bool handleMousePressImpl(InstrumentingAgents*);
    static bool forcePseudoStateImpl(InstrumentingAgents*, Element*, CSSSelector::PseudoType);

    static void willSendXMLHttpRequestImpl(InstrumentingAgents*, const String& url);
    static void didScheduleResourceRequestImpl(InstrumentingAgents*, const String& url, Frame*);
    static void didInstallTimerImpl(InstrumentingAgents*, int timerId, int timeout, bool singleShot, ScriptExecutionContext*);
    static void didRemoveTimerImpl(InstrumentingAgents*, int timerId, ScriptExecutionContext*);

    static InspectorInstrumentationCookie willCallFunctionImpl(InstrumentingAgents*, const String& scriptName, int scriptLine, ScriptExecutionContext*);
    static void didCallFunctionImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchXHRReadyStateChangeEventImpl(InstrumentingAgents*, XMLHttpRequest*, ScriptExecutionContext*);
    static void didDispatchXHRReadyStateChangeEventImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEventImpl(InstrumentingAgents*, const Event&, DOMWindow*, Node*, const EventPath&, Document*);
    static InspectorInstrumentationCookie willHandleEventImpl(InstrumentingAgents*, Event*);
    static void didHandleEventImpl(const InspectorInstrumentationCookie&);
    static void didDispatchEventImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEventOnWindowImpl(InstrumentingAgents*, const Event&, DOMWindow*);
    static void didDispatchEventOnWindowImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willEvaluateScriptImpl(InstrumentingAgents*, const String& url, int lineNumber, Frame*);
    static void didEvaluateScriptImpl(const InspectorInstrumentationCookie&);
    static void scriptsEnabledImpl(InstrumentingAgents*, bool isEnabled);
    static void didCreateIsolatedContextImpl(InstrumentingAgents*, Frame*, ScriptState*, SecurityOrigin*);
    static InspectorInstrumentationCookie willFireTimerImpl(InstrumentingAgents*, int timerId, ScriptExecutionContext*);
    static void didFireTimerImpl(const InspectorInstrumentationCookie&);
    static void didInvalidateLayoutImpl(InstrumentingAgents*, Frame*);
    static InspectorInstrumentationCookie willLayoutImpl(InstrumentingAgents*, Frame*);
    static void didLayoutImpl(const InspectorInstrumentationCookie&, RenderObject*);
    static void didScrollImpl(InstrumentingAgents*);
    static InspectorInstrumentationCookie willDispatchXHRLoadEventImpl(InstrumentingAgents*, XMLHttpRequest*, ScriptExecutionContext*);
    static void didDispatchXHRLoadEventImpl(const InspectorInstrumentationCookie&);
    static void willScrollLayerImpl(InstrumentingAgents*, Frame*);
    static void didScrollLayerImpl(InstrumentingAgents*);
    static void willPaintImpl(InstrumentingAgents*, RenderObject*);
    static void didPaintImpl(InstrumentingAgents*, RenderObject*, GraphicsContext*, const LayoutRect&);
    static InspectorInstrumentationCookie willRecalculateStyleImpl(InstrumentingAgents*, Frame*);
    static void didRecalculateStyleImpl(const InspectorInstrumentationCookie&);
    static void didRecalculateStyleForElementImpl(InstrumentingAgents*);
    static void didScheduleStyleRecalculationImpl(InstrumentingAgents*, Document*);
    static InspectorInstrumentationCookie willMatchRuleImpl(InstrumentingAgents*, StyleRule*, InspectorCSSOMWrappers&, DocumentStyleSheetCollection*);
    static void didMatchRuleImpl(const InspectorInstrumentationCookie&, bool matched);
    static InspectorInstrumentationCookie willProcessRuleImpl(InstrumentingAgents*, StyleRule*, StyleResolver*);
    static void didProcessRuleImpl(const InspectorInstrumentationCookie&);

    static void applyUserAgentOverrideImpl(InstrumentingAgents*, String*);
    static void applyScreenWidthOverrideImpl(InstrumentingAgents*, long*);
    static void applyScreenHeightOverrideImpl(InstrumentingAgents*, long*);
    static void applyEmulatedMediaImpl(InstrumentingAgents*, String*);
    static bool shouldApplyScreenWidthOverrideImpl(InstrumentingAgents*);
    static bool shouldApplyScreenHeightOverrideImpl(InstrumentingAgents*);
    static void willSendRequestImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
    static void continueAfterPingLoaderImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse&);
    static void markResourceAsCachedImpl(InstrumentingAgents*, unsigned long identifier);
    static void didLoadResourceFromMemoryCacheImpl(InstrumentingAgents*, DocumentLoader*, CachedResource*);
    static InspectorInstrumentationCookie willReceiveResourceDataImpl(InstrumentingAgents*, unsigned long identifier, Frame*, int length);
    static void didReceiveResourceDataImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willReceiveResourceResponseImpl(InstrumentingAgents*, unsigned long identifier, const ResourceResponse&, Frame*);
    static void didReceiveResourceResponseImpl(const InspectorInstrumentationCookie&, unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    static void didReceiveResourceResponseButCanceledImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void continueAfterXFrameOptionsDeniedImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyDownloadImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyIgnoreImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void didReceiveDataImpl(InstrumentingAgents*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
    static void didFinishLoadingImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, double finishTime);
    static void didFailLoadingImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, const ResourceError&);
    static void documentThreadableLoaderStartedLoadingForClientImpl(InstrumentingAgents*, unsigned long identifier, ThreadableLoaderClient*);
    static void willLoadXHRImpl(InstrumentingAgents*, ThreadableLoaderClient*, const String&, const KURL&, bool, PassRefPtr<FormData>, const HTTPHeaderMap&, bool);
    static void didFailXHRLoadingImpl(InstrumentingAgents*, ThreadableLoaderClient*);
    static void didFinishXHRLoadingImpl(InstrumentingAgents*, ThreadableLoaderClient*, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber);
    static void didReceiveXHRResponseImpl(InstrumentingAgents*, unsigned long identifier);
    static void willLoadXHRSynchronouslyImpl(InstrumentingAgents*);
    static void didLoadXHRSynchronouslyImpl(InstrumentingAgents*);
    static void scriptImportedImpl(InstrumentingAgents*, unsigned long identifier, const String& sourceString);
    static void scriptExecutionBlockedByCSPImpl(InstrumentingAgents*, const String& directiveText);
    static void didReceiveScriptResponseImpl(InstrumentingAgents*, unsigned long identifier);
    static void domContentLoadedEventFiredImpl(InstrumentingAgents*, Frame*);
    static void loadEventFiredImpl(InstrumentingAgents*, Frame*);
    static void frameDetachedFromParentImpl(InstrumentingAgents*, Frame*);
    static void didCommitLoadImpl(InstrumentingAgents*, Page*, DocumentLoader*);
    static void frameDocumentUpdatedImpl(InstrumentingAgents*, Frame*);
    static void loaderDetachedFromFrameImpl(InstrumentingAgents*, DocumentLoader*);
    static void frameStartedLoadingImpl(InstrumentingAgents*, Frame*);
    static void frameStoppedLoadingImpl(InstrumentingAgents*, Frame*);
    static void frameScheduledNavigationImpl(InstrumentingAgents*, Frame*, double delay);
    static void frameClearedScheduledNavigationImpl(InstrumentingAgents*, Frame*);
    static InspectorInstrumentationCookie willRunJavaScriptDialogImpl(InstrumentingAgents*, const String& message);
    static void didRunJavaScriptDialogImpl(const InspectorInstrumentationCookie&);
    static void willDestroyCachedResourceImpl(CachedResource*);

    static InspectorInstrumentationCookie willWriteHTMLImpl(InstrumentingAgents*, unsigned startLine, Frame*);
    static void didWriteHTMLImpl(const InspectorInstrumentationCookie&, unsigned endLine);

    static void addMessageToConsoleImpl(InstrumentingAgents*, MessageSource, MessageType, MessageLevel, const String& message, ScriptState*, PassRefPtr<ScriptArguments>, unsigned long requestIdentifier);
    static void addMessageToConsoleImpl(InstrumentingAgents*, MessageSource, MessageType, MessageLevel, const String& message, const String& scriptId, unsigned lineNumber, ScriptState*, unsigned long requestIdentifier);

    // FIXME: Remove once we no longer generate stacks outside of Inspector.
    static void addMessageToConsoleImpl(InstrumentingAgents*, MessageSource, MessageType, MessageLevel, const String& message, PassRefPtr<ScriptCallStack>, unsigned long requestIdentifier);

    static void consoleCountImpl(InstrumentingAgents*, ScriptState*, PassRefPtr<ScriptArguments>);
    static void startConsoleTimingImpl(InstrumentingAgents*, Frame*, const String& title);
    static void stopConsoleTimingImpl(InstrumentingAgents*, Frame*, const String& title, PassRefPtr<ScriptCallStack>);
    static void consoleTimeStampImpl(InstrumentingAgents*, Frame*, PassRefPtr<ScriptArguments>);

    static void didRequestAnimationFrameImpl(InstrumentingAgents*, int callbackId, Frame*);
    static void didCancelAnimationFrameImpl(InstrumentingAgents*, int callbackId, Frame*);
    static InspectorInstrumentationCookie willFireAnimationFrameImpl(InstrumentingAgents*, int callbackId, Frame*);
    static void didFireAnimationFrameImpl(const InspectorInstrumentationCookie&);

    static void addStartProfilingMessageToConsoleImpl(InstrumentingAgents*, const String& title, unsigned lineNumber, const String& sourceURL);
    static void addProfileImpl(InstrumentingAgents*, RefPtr<ScriptProfile>, PassRefPtr<ScriptCallStack>);
    static String getCurrentUserInitiatedProfileNameImpl(InstrumentingAgents*, bool incrementProfileNumber);
    static bool profilerEnabledImpl(InstrumentingAgents*);

    static void didOpenDatabaseImpl(InstrumentingAgents*, PassRefPtr<Database>, const String& domain, const String& name, const String& version);

    static void didDispatchDOMStorageEventImpl(InstrumentingAgents*, const String& key, const String& oldValue, const String& newValue, StorageType, SecurityOrigin*, Page*);

    static bool shouldPauseDedicatedWorkerOnStartImpl(InstrumentingAgents*);
    static void didStartWorkerContextImpl(InstrumentingAgents*, WorkerContextProxy*, const KURL&);
    static void workerContextTerminatedImpl(InstrumentingAgents*, WorkerContextProxy*);

    static void didCreateWebSocketImpl(InstrumentingAgents*, unsigned long identifier, const KURL& requestURL, const KURL& documentURL, const String& protocol, Document*);
    static void willSendWebSocketHandshakeRequestImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketHandshakeRequest&, Document*);
    static void didReceiveWebSocketHandshakeResponseImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketHandshakeResponse&, Document*);
    static void didCloseWebSocketImpl(InstrumentingAgents*, unsigned long identifier, Document*);
    static void didReceiveWebSocketFrameImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketFrame&);
    static void didSendWebSocketFrameImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketFrame&);
    static void didReceiveWebSocketFrameErrorImpl(InstrumentingAgents*, unsigned long identifier, const String&);

    static void networkStateChangedImpl(InstrumentingAgents*);
    static void updateApplicationCacheStatusImpl(InstrumentingAgents*, Frame*);

    static InstrumentingAgents* instrumentingAgentsForPage(Page*);
    static InstrumentingAgents* instrumentingAgentsForFrame(Frame*);
    static InstrumentingAgents* instrumentingAgentsForContext(ScriptExecutionContext*);
    static InstrumentingAgents* instrumentingAgentsForDocument(Document*);
    static InstrumentingAgents* instrumentingAgentsForRenderer(RenderObject*);

    static InstrumentingAgents* instrumentingAgentsForWorkerContext(WorkerContext*);
    static InstrumentingAgents* instrumentingAgentsForNonDocumentContext(ScriptExecutionContext*);

    static bool collectingHTMLParseErrors(InstrumentingAgents*);
    static void pauseOnNativeEventIfNeeded(InstrumentingAgents*, bool isDOMEvent, const String& eventName, bool synchronous);
    static void cancelPauseOnNativeEvent(InstrumentingAgents*);
    static InspectorTimelineAgent* retrieveTimelineAgent(const InspectorInstrumentationCookie&);

    static GeolocationPosition* overrideGeolocationPositionImpl(InstrumentingAgents*, GeolocationPosition*);

    static DeviceOrientationData* overrideDeviceOrientationImpl(InstrumentingAgents*, DeviceOrientationData*);

    static void layerTreeDidChangeImpl(InstrumentingAgents*);
    static void renderLayerDestroyedImpl(InstrumentingAgents*, const RenderLayer*);
    static void pseudoElementDestroyedImpl(InstrumentingAgents*, PseudoElement*);

    static int s_frontendCounter;
};

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

inline void InspectorInstrumentation::didClearWindowObjectInWorld(Frame* frame, DOMWrapperWorld* world)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didClearWindowObjectInWorldImpl(instrumentingAgents, frame, world);
}

inline bool InspectorInstrumentation::isDebuggerPaused(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return isDebuggerPausedImpl(instrumentingAgents);
    return false;
}

inline void InspectorInstrumentation::willInsertDOMNode(Document* document, Node* parent)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willInsertDOMNodeImpl(instrumentingAgents, parent);
}

inline void InspectorInstrumentation::didInsertDOMNode(Document* document, Node* node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didInsertDOMNodeImpl(instrumentingAgents, node);
}

inline void InspectorInstrumentation::willRemoveDOMNode(Document* document, Node* node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document)) {
        willRemoveDOMNodeImpl(instrumentingAgents, node);
        didRemoveDOMNodeImpl(instrumentingAgents, node);
    }
}

inline void InspectorInstrumentation::willModifyDOMAttr(Document* document, Element* element, const AtomicString& oldValue, const AtomicString& newValue)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willModifyDOMAttrImpl(instrumentingAgents, element, oldValue, newValue);
}

inline void InspectorInstrumentation::didModifyDOMAttr(Document* document, Element* element, const AtomicString& name, const AtomicString& value)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didModifyDOMAttrImpl(instrumentingAgents, element, name, value);
}

inline void InspectorInstrumentation::didRemoveDOMAttr(Document* document, Element* element, const AtomicString& name)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didRemoveDOMAttrImpl(instrumentingAgents, element, name);
}

inline void InspectorInstrumentation::didInvalidateStyleAttr(Document* document, Node* node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didInvalidateStyleAttrImpl(instrumentingAgents, node);
}

inline void InspectorInstrumentation::frameWindowDiscarded(Frame* frame, DOMWindow* domWindow)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameWindowDiscardedImpl(instrumentingAgents, domWindow);
}

inline void InspectorInstrumentation::mediaQueryResultChanged(Document* document)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        mediaQueryResultChangedImpl(instrumentingAgents);
}

inline void InspectorInstrumentation::didPushShadowRoot(Element* host, ShadowRoot* root)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(host->ownerDocument()))
        didPushShadowRootImpl(instrumentingAgents, host, root);
}

inline void InspectorInstrumentation::willPopShadowRoot(Element* host, ShadowRoot* root)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(host->ownerDocument()))
        willPopShadowRootImpl(instrumentingAgents, host, root);
}

inline void InspectorInstrumentation::didCreateNamedFlow(Document* document, NamedFlow* namedFlow)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCreateNamedFlowImpl(instrumentingAgents, document, namedFlow);
}

inline void InspectorInstrumentation::willRemoveNamedFlow(Document* document, NamedFlow* namedFlow)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willRemoveNamedFlowImpl(instrumentingAgents, document, namedFlow);
}

inline void InspectorInstrumentation::didUpdateRegionLayout(Document* document, NamedFlow* namedFlow)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didUpdateRegionLayoutImpl(instrumentingAgents, document, namedFlow);
}

inline void InspectorInstrumentation::mouseDidMoveOverElement(Page* page, const HitTestResult& result, unsigned modifierFlags)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        mouseDidMoveOverElementImpl(instrumentingAgents, result, modifierFlags);
}

inline bool InspectorInstrumentation::handleTouchEvent(Page* page, Node* node)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return handleTouchEventImpl(instrumentingAgents, node);
    return false;
}

inline bool InspectorInstrumentation::handleMousePress(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return handleMousePressImpl(instrumentingAgents);
    return false;
}

inline bool InspectorInstrumentation::forcePseudoState(Element* element, CSSSelector::PseudoType pseudoState)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(element->document()))
        return forcePseudoStateImpl(instrumentingAgents, element, pseudoState);
    return false;
}

inline void InspectorInstrumentation::characterDataModified(Document* document, CharacterData* characterData)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        characterDataModifiedImpl(instrumentingAgents, characterData);
}

inline void InspectorInstrumentation::willSendXMLHttpRequest(ScriptExecutionContext* context, const String& url)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        willSendXMLHttpRequestImpl(instrumentingAgents, url);
}

inline void InspectorInstrumentation::didScheduleResourceRequest(Document* document, const String& url)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didScheduleResourceRequestImpl(instrumentingAgents, url, document->frame());
}

inline void InspectorInstrumentation::didInstallTimer(ScriptExecutionContext* context, int timerId, int timeout, bool singleShot)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didInstallTimerImpl(instrumentingAgents, timerId, timeout, singleShot, context);
}

inline void InspectorInstrumentation::didRemoveTimer(ScriptExecutionContext* context, int timerId)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didRemoveTimerImpl(instrumentingAgents, timerId, context);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willCallFunction(ScriptExecutionContext* context, const String& scriptName, int scriptLine)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willCallFunctionImpl(instrumentingAgents, scriptName, scriptLine, context);
    return InspectorInstrumentationCookie();
}


inline void InspectorInstrumentation::didCallFunction(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didCallFunctionImpl(cookie);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willDispatchXHRReadyStateChangeEvent(ScriptExecutionContext* context, XMLHttpRequest* request)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willDispatchXHRReadyStateChangeEventImpl(instrumentingAgents, request, context);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didDispatchXHRReadyStateChangeEvent(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didDispatchXHRReadyStateChangeEventImpl(cookie);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEvent(Document* document, const Event& event, DOMWindow* window, Node* node, const EventPath& eventPath)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willDispatchEventImpl(instrumentingAgents, event, window, node, eventPath, document);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didDispatchEvent(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didDispatchEventImpl(cookie);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willHandleEvent(ScriptExecutionContext* context, Event* event)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willHandleEventImpl(instrumentingAgents, event);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didHandleEvent(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didHandleEventImpl(cookie);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEventOnWindow(Frame* frame, const Event& event, DOMWindow* window)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willDispatchEventOnWindowImpl(instrumentingAgents, event, window);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didDispatchEventOnWindow(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didDispatchEventOnWindowImpl(cookie);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willEvaluateScript(Frame* frame, const String& url, int lineNumber)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willEvaluateScriptImpl(instrumentingAgents, url, lineNumber, frame);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didEvaluateScript(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didEvaluateScriptImpl(cookie);
}

inline void InspectorInstrumentation::scriptsEnabled(Page* page, bool isEnabled)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return scriptsEnabledImpl(instrumentingAgents, isEnabled);
}

inline void InspectorInstrumentation::didCreateIsolatedContext(Frame* frame, ScriptState* scriptState, SecurityOrigin* origin)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return didCreateIsolatedContextImpl(instrumentingAgents, frame, scriptState, origin);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willFireTimer(ScriptExecutionContext* context, int timerId)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willFireTimerImpl(instrumentingAgents, timerId, context);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didFireTimer(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didFireTimerImpl(cookie);
}

inline void InspectorInstrumentation::didInvalidateLayout(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didInvalidateLayoutImpl(instrumentingAgents, frame);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willLayout(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willLayoutImpl(instrumentingAgents, frame);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didLayout(const InspectorInstrumentationCookie& cookie, RenderObject* root)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didLayoutImpl(cookie, root);
}

inline void InspectorInstrumentation::didScroll(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didScrollImpl(instrumentingAgents);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willDispatchXHRLoadEvent(ScriptExecutionContext* context, XMLHttpRequest* request)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willDispatchXHRLoadEventImpl(instrumentingAgents, request, context);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didDispatchXHRLoadEvent(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didDispatchXHRLoadEventImpl(cookie);
}

inline void InspectorInstrumentation::willPaint(RenderObject* renderer)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForRenderer(renderer))
        return willPaintImpl(instrumentingAgents, renderer);
}

inline void InspectorInstrumentation::didPaint(RenderObject* renderer, GraphicsContext* context, const LayoutRect& rect)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForRenderer(renderer))
        didPaintImpl(instrumentingAgents, renderer, context, rect);
}

inline void InspectorInstrumentation::willScrollLayer(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        willScrollLayerImpl(instrumentingAgents, frame);
}

inline void InspectorInstrumentation::didScrollLayer(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didScrollLayerImpl(instrumentingAgents);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willRecalculateStyle(Document* document)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willRecalculateStyleImpl(instrumentingAgents, document->frame());
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didRecalculateStyle(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didRecalculateStyleImpl(cookie);
}

inline void InspectorInstrumentation::didRecalculateStyleForElement(Element* element)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(element->document()))
        didRecalculateStyleForElementImpl(instrumentingAgents);
}

inline void InspectorInstrumentation::didScheduleStyleRecalculation(Document* document)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didScheduleStyleRecalculationImpl(instrumentingAgents, document);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willMatchRule(Document* document, StyleRule* rule, InspectorCSSOMWrappers& inspectorCSSOMWrappers, DocumentStyleSheetCollection* styleSheetCollection)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willMatchRuleImpl(instrumentingAgents, rule, inspectorCSSOMWrappers, styleSheetCollection);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didMatchRule(const InspectorInstrumentationCookie& cookie, bool matched)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didMatchRuleImpl(cookie, matched);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willProcessRule(Document* document, StyleRule* rule, StyleResolver* styleResolver)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (!rule)
        return InspectorInstrumentationCookie();
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willProcessRuleImpl(instrumentingAgents, rule, styleResolver);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didProcessRule(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didProcessRuleImpl(cookie);
}

inline void InspectorInstrumentation::applyUserAgentOverride(Frame* frame, String* userAgent)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyUserAgentOverrideImpl(instrumentingAgents, userAgent);
}

inline void InspectorInstrumentation::applyScreenWidthOverride(Frame* frame, long* width)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyScreenWidthOverrideImpl(instrumentingAgents, width);
}

inline void InspectorInstrumentation::applyScreenHeightOverride(Frame* frame, long* height)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyScreenHeightOverrideImpl(instrumentingAgents, height);
}

inline bool InspectorInstrumentation::shouldApplyScreenWidthOverride(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return shouldApplyScreenWidthOverrideImpl(instrumentingAgents);
    return false;
}

inline void InspectorInstrumentation::applyEmulatedMedia(Frame* frame, String* media)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyEmulatedMediaImpl(instrumentingAgents, media);
}

inline bool InspectorInstrumentation::shouldApplyScreenHeightOverride(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return shouldApplyScreenHeightOverrideImpl(instrumentingAgents);
    return false;
}

inline void InspectorInstrumentation::willSendRequest(Frame* frame, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        willSendRequestImpl(instrumentingAgents, identifier, loader, request, redirectResponse);
}

inline void InspectorInstrumentation::continueAfterPingLoader(Frame* frame, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& response)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        InspectorInstrumentation::continueAfterPingLoaderImpl(instrumentingAgents, identifier, loader, request, response);
}

inline void InspectorInstrumentation::markResourceAsCached(Page* page, unsigned long identifier)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        markResourceAsCachedImpl(instrumentingAgents, identifier);
}

inline void InspectorInstrumentation::didLoadResourceFromMemoryCache(Page* page, DocumentLoader* loader, CachedResource* resource)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didLoadResourceFromMemoryCacheImpl(instrumentingAgents, loader, resource);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceData(Frame* frame, unsigned long identifier, int length)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willReceiveResourceDataImpl(instrumentingAgents, identifier, frame, length);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didReceiveResourceData(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didReceiveResourceDataImpl(cookie);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceResponse(Frame* frame, unsigned long identifier, const ResourceResponse& response)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willReceiveResourceResponseImpl(instrumentingAgents, identifier, response, frame);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didReceiveResourceResponse(const InspectorInstrumentationCookie& cookie, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    // Call this unconditionally so that we're able to log to console with no front-end attached.
    if (cookie.isValid())
        didReceiveResourceResponseImpl(cookie, identifier, loader, response, resourceLoader);
}

inline void InspectorInstrumentation::continueAfterXFrameOptionsDenied(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    InspectorInstrumentation::continueAfterXFrameOptionsDeniedImpl(frame, loader, identifier, r);
}

inline void InspectorInstrumentation::continueWithPolicyDownload(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    InspectorInstrumentation::continueWithPolicyDownloadImpl(frame, loader, identifier, r);
}

inline void InspectorInstrumentation::continueWithPolicyIgnore(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    InspectorInstrumentation::continueWithPolicyIgnoreImpl(frame, loader, identifier, r);
}

inline void InspectorInstrumentation::didReceiveData(Frame* frame, unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didReceiveDataImpl(instrumentingAgents, identifier, data, dataLength, encodedDataLength);
}

inline void InspectorInstrumentation::didFinishLoading(Frame* frame, DocumentLoader* loader, unsigned long identifier, double finishTime)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didFinishLoadingImpl(instrumentingAgents, identifier, loader, finishTime);
}

inline void InspectorInstrumentation::didFailLoading(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceError& error)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didFailLoadingImpl(instrumentingAgents, identifier, loader, error);
}

inline void InspectorInstrumentation::documentThreadableLoaderStartedLoadingForClient(ScriptExecutionContext* context, unsigned long identifier, ThreadableLoaderClient* client)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        documentThreadableLoaderStartedLoadingForClientImpl(instrumentingAgents, identifier, client);
}

inline void InspectorInstrumentation::willLoadXHR(ScriptExecutionContext* context, ThreadableLoaderClient* client, const String& method, const KURL& url, bool async, PassRefPtr<FormData> formData, const HTTPHeaderMap& headers, bool includeCredentials)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        willLoadXHRImpl(instrumentingAgents, client, method, url, async, formData, headers, includeCredentials);
}

inline void InspectorInstrumentation::didFailXHRLoading(ScriptExecutionContext* context, ThreadableLoaderClient* client)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didFailXHRLoadingImpl(instrumentingAgents, client);
}


inline void InspectorInstrumentation::didFinishXHRLoading(ScriptExecutionContext* context, ThreadableLoaderClient* client, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didFinishXHRLoadingImpl(instrumentingAgents, client, identifier, sourceString, url, sendURL, sendLineNumber);
}

inline void InspectorInstrumentation::didReceiveXHRResponse(ScriptExecutionContext* context, unsigned long identifier)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didReceiveXHRResponseImpl(instrumentingAgents, identifier);
}

inline void InspectorInstrumentation::willLoadXHRSynchronously(ScriptExecutionContext* context)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        willLoadXHRSynchronouslyImpl(instrumentingAgents);
}

inline void InspectorInstrumentation::didLoadXHRSynchronously(ScriptExecutionContext* context)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didLoadXHRSynchronouslyImpl(instrumentingAgents);
}

inline void InspectorInstrumentation::scriptImported(ScriptExecutionContext* context, unsigned long identifier, const String& sourceString)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        scriptImportedImpl(instrumentingAgents, identifier, sourceString);
}

inline void InspectorInstrumentation::scriptExecutionBlockedByCSP(ScriptExecutionContext* context, const String& directiveText)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        scriptExecutionBlockedByCSPImpl(instrumentingAgents, directiveText);
}

inline void InspectorInstrumentation::didReceiveScriptResponse(ScriptExecutionContext* context, unsigned long identifier)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didReceiveScriptResponseImpl(instrumentingAgents, identifier);
}

inline void InspectorInstrumentation::domContentLoadedEventFired(Frame* frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        domContentLoadedEventFiredImpl(instrumentingAgents, frame);
}

inline void InspectorInstrumentation::loadEventFired(Frame* frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        loadEventFiredImpl(instrumentingAgents, frame);
}

inline void InspectorInstrumentation::frameDetachedFromParent(Frame* frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameDetachedFromParentImpl(instrumentingAgents, frame);
}

inline void InspectorInstrumentation::didCommitLoad(Frame* frame, DocumentLoader* loader)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didCommitLoadImpl(instrumentingAgents, frame->page(), loader);
}

inline void InspectorInstrumentation::frameDocumentUpdated(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameDocumentUpdatedImpl(instrumentingAgents, frame);
}

inline void InspectorInstrumentation::loaderDetachedFromFrame(Frame* frame, DocumentLoader* loader)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        loaderDetachedFromFrameImpl(instrumentingAgents, loader);
}

inline void InspectorInstrumentation::frameStartedLoading(Frame* frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameStartedLoadingImpl(instrumentingAgents, frame);
}

inline void InspectorInstrumentation::frameStoppedLoading(Frame* frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameStoppedLoadingImpl(instrumentingAgents, frame);
}

inline void InspectorInstrumentation::frameScheduledNavigation(Frame* frame, double delay)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameScheduledNavigationImpl(instrumentingAgents, frame, delay);
}

inline void InspectorInstrumentation::frameClearedScheduledNavigation(Frame* frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameClearedScheduledNavigationImpl(instrumentingAgents, frame);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willRunJavaScriptDialog(Page* page, const String& message)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return willRunJavaScriptDialogImpl(instrumentingAgents, message);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didRunJavaScriptDialog(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didRunJavaScriptDialogImpl(cookie);
}

inline void InspectorInstrumentation::willDestroyCachedResource(CachedResource* cachedResource)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    willDestroyCachedResourceImpl(cachedResource);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willWriteHTML(Document* document, unsigned startLine)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willWriteHTMLImpl(instrumentingAgents, startLine, document->frame());
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didWriteHTML(const InspectorInstrumentationCookie& cookie, unsigned endLine)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didWriteHTMLImpl(cookie, endLine);
}

inline void InspectorInstrumentation::didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType storageType, SecurityOrigin* securityOrigin, Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didDispatchDOMStorageEventImpl(instrumentingAgents, key, oldValue, newValue, storageType, securityOrigin, page);
}

inline bool InspectorInstrumentation::shouldPauseDedicatedWorkerOnStart(ScriptExecutionContext* context)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return shouldPauseDedicatedWorkerOnStartImpl(instrumentingAgents);
    return false;
}

inline void InspectorInstrumentation::didStartWorkerContext(ScriptExecutionContext* context, WorkerContextProxy* proxy, const KURL& url)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didStartWorkerContextImpl(instrumentingAgents, proxy, url);
}

inline void InspectorInstrumentation::workerContextTerminated(ScriptExecutionContext* context, WorkerContextProxy* proxy)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        workerContextTerminatedImpl(instrumentingAgents, proxy);
}

inline void InspectorInstrumentation::didCreateWebSocket(Document* document, unsigned long identifier, const KURL& requestURL, const KURL& documentURL, const String& protocol)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCreateWebSocketImpl(instrumentingAgents, identifier, requestURL, documentURL, protocol, document);
}

inline void InspectorInstrumentation::willSendWebSocketHandshakeRequest(Document* document, unsigned long identifier, const WebSocketHandshakeRequest& request)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willSendWebSocketHandshakeRequestImpl(instrumentingAgents, identifier, request, document);
}

inline void InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(Document* document, unsigned long identifier, const WebSocketHandshakeResponse& response)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didReceiveWebSocketHandshakeResponseImpl(instrumentingAgents, identifier, response, document);
}

inline void InspectorInstrumentation::didCloseWebSocket(Document* document, unsigned long identifier)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCloseWebSocketImpl(instrumentingAgents, identifier, document);
}
inline void InspectorInstrumentation::didReceiveWebSocketFrame(Document* document, unsigned long identifier, const WebSocketFrame& frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didReceiveWebSocketFrameImpl(instrumentingAgents, identifier, frame);
}
inline void InspectorInstrumentation::didReceiveWebSocketFrameError(Document* document, unsigned long identifier, const String& errorMessage)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didReceiveWebSocketFrameErrorImpl(instrumentingAgents, identifier, errorMessage);
}
inline void InspectorInstrumentation::didSendWebSocketFrame(Document* document, unsigned long identifier, const WebSocketFrame& frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didSendWebSocketFrameImpl(instrumentingAgents, identifier, frame);
}

inline void InspectorInstrumentation::networkStateChanged(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        networkStateChangedImpl(instrumentingAgents);
}

inline void InspectorInstrumentation::updateApplicationCacheStatus(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        updateApplicationCacheStatusImpl(instrumentingAgents, frame);
}

inline void InspectorInstrumentation::didRequestAnimationFrame(Document* document, int callbackId)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didRequestAnimationFrameImpl(instrumentingAgents, callbackId, document->frame());
}

inline void InspectorInstrumentation::didCancelAnimationFrame(Document* document, int callbackId)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCancelAnimationFrameImpl(instrumentingAgents, callbackId, document->frame());
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willFireAnimationFrame(Document* document, int callbackId)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willFireAnimationFrameImpl(instrumentingAgents, callbackId, document->frame());
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didFireAnimationFrame(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didFireAnimationFrameImpl(cookie);
}

inline GeolocationPosition* InspectorInstrumentation::overrideGeolocationPosition(Page* page, GeolocationPosition* position)
{
    FAST_RETURN_IF_NO_FRONTENDS(position);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return overrideGeolocationPositionImpl(instrumentingAgents, position);
    return position;
}

inline DeviceOrientationData* InspectorInstrumentation::overrideDeviceOrientation(Page* page, DeviceOrientationData* deviceOrientation)
{
    FAST_RETURN_IF_NO_FRONTENDS(deviceOrientation);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return overrideDeviceOrientationImpl(instrumentingAgents, deviceOrientation);
    return deviceOrientation;
}

inline void InspectorInstrumentation::layerTreeDidChange(Page* page)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        layerTreeDidChangeImpl(instrumentingAgents);
}

inline void InspectorInstrumentation::renderLayerDestroyed(Page* page, const RenderLayer* renderLayer)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        renderLayerDestroyedImpl(instrumentingAgents, renderLayer);
}

inline void InspectorInstrumentation::pseudoElementDestroyed(Page* page, PseudoElement* pseudoElement)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        pseudoElementDestroyedImpl(instrumentingAgents, pseudoElement);
}

inline bool InspectorInstrumentation::collectingHTMLParseErrors(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return collectingHTMLParseErrors(instrumentingAgents);
    return false;
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForContext(ScriptExecutionContext* context)
{
    if (!context)
        return 0;
    if (context->isDocument())
        return instrumentingAgentsForPage(toDocument(context)->page());
    return instrumentingAgentsForNonDocumentContext(context);
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForFrame(Frame* frame)
{
    if (frame)
        return instrumentingAgentsForPage(frame->page());
    return 0;
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForDocument(Document* document)
{
    if (document) {
        Page* page = document->page();
        if (!page && document->templateDocumentHost())
            page = document->templateDocumentHost()->page();
        return instrumentingAgentsForPage(page);
    }
    return 0;
}

} // namespace WebCore

#endif // !defined(InspectorInstrumentation_h)
