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

#include "WebSocketFrame.h"
#include "WebSocketHandshakeRequest.h"
#include "WebSocketHandshakeResponse.h"
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

void didClearWindowObjectInWorld(Frame*, DOMWrapperWorld*);
bool isDebuggerPaused(Frame*);

void willInsertDOMNode(Document*, Node* parent);
void didInsertDOMNode(Document*, Node*);
void willRemoveDOMNode(Document*, Node*);
void willModifyDOMAttr(Document*, Element*, const AtomicString& oldValue, const AtomicString& newValue);
void didModifyDOMAttr(Document*, Element*, const AtomicString& name, const AtomicString& value);
void didRemoveDOMAttr(Document*, Element*, const AtomicString& name);
void characterDataModified(Document*, CharacterData*);
void didInvalidateStyleAttr(Document*, Node*);
void activeStyleSheetsUpdated(Document*, const Vector<RefPtr<StyleSheet> >&);
void frameWindowDiscarded(Frame*, DOMWindow*);
void mediaQueryResultChanged(Document*);
void didPushShadowRoot(Element* host, ShadowRoot*);
void willPopShadowRoot(Element* host, ShadowRoot*);
void didCreateNamedFlow(Document*, NamedFlow*);
void willRemoveNamedFlow(Document*, NamedFlow*);
void didUpdateRegionLayout(Document*, NamedFlow*);

void handleMouseMove(Frame*, const PlatformMouseEvent&);
bool handleMousePress(Page*);
bool handleTouchEvent(Page*, Node*);
bool forcePseudoState(Element*, CSSSelector::PseudoType);

void willSendXMLHttpRequest(ScriptExecutionContext*, const String& url);
void didScheduleResourceRequest(Document*, const String& url);
void didInstallTimer(ScriptExecutionContext*, int timerId, int timeout, bool singleShot);
void didRemoveTimer(ScriptExecutionContext*, int timerId);

InspectorInstrumentationCookie willCallFunction(ScriptExecutionContext*, const String& scriptName, int scriptLine);
void didCallFunction(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willDispatchXHRReadyStateChangeEvent(ScriptExecutionContext*, XMLHttpRequest*);
void didDispatchXHRReadyStateChangeEvent(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willDispatchEvent(Document*, const Event&, DOMWindow*, Node*, const EventPath&);
void didDispatchEvent(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willHandleEvent(ScriptExecutionContext*, Event*);
void didHandleEvent(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willDispatchEventOnWindow(Frame*, const Event& event, DOMWindow* window);
void didDispatchEventOnWindow(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willEvaluateScript(Frame*, const String& url, int lineNumber);
void didEvaluateScript(const InspectorInstrumentationCookie&);
void scriptsEnabled(Page*, bool isEnabled);
void didCreateIsolatedContext(Frame*, ScriptState*, SecurityOrigin*);
InspectorInstrumentationCookie willFireTimer(ScriptExecutionContext*, int timerId);
void didFireTimer(const InspectorInstrumentationCookie&);
void didInvalidateLayout(Frame*);
InspectorInstrumentationCookie willLayout(Frame*);
void didLayout(const InspectorInstrumentationCookie&, RenderObject*);
void didScroll(Page*);
InspectorInstrumentationCookie willDispatchXHRLoadEvent(ScriptExecutionContext*, XMLHttpRequest*);
void didDispatchXHRLoadEvent(const InspectorInstrumentationCookie&);
void willScrollLayer(Frame*);
void didScrollLayer(Frame*);
void willPaint(RenderObject*);
void didPaint(RenderObject*, GraphicsContext*, const LayoutRect&);
void willComposite(Page*);
void didComposite(Page*);
InspectorInstrumentationCookie willRecalculateStyle(Document*);
void didRecalculateStyle(const InspectorInstrumentationCookie&);
void didRecalculateStyleForElement(Element*);

void didScheduleStyleRecalculation(Document*);
InspectorInstrumentationCookie willMatchRule(Document*, StyleRule*, InspectorCSSOMWrappers&, DocumentStyleSheetCollection*);
void didMatchRule(const InspectorInstrumentationCookie&, bool matched);
InspectorInstrumentationCookie willProcessRule(Document*, StyleRule*, StyleResolver*);
void didProcessRule(const InspectorInstrumentationCookie&);

void applyUserAgentOverride(Frame*, String*);
void applyScreenWidthOverride(Frame*, long*);
void applyScreenHeightOverride(Frame*, long*);
void applyEmulatedMedia(Frame*, String*);
bool shouldApplyScreenWidthOverride(Frame*);
bool shouldApplyScreenHeightOverride(Frame*);
void willSendRequest(Frame*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
void continueAfterPingLoader(Frame*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse&);
void markResourceAsCached(Page*, unsigned long identifier);
void didLoadResourceFromMemoryCache(Page*, DocumentLoader*, CachedResource*);
InspectorInstrumentationCookie willReceiveResourceData(Frame*, unsigned long identifier, int length);
void didReceiveResourceData(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willReceiveResourceResponse(Frame*, unsigned long identifier, const ResourceResponse&);
void didReceiveResourceResponse(const InspectorInstrumentationCookie&, unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
void continueAfterXFrameOptionsDenied(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
void continueWithPolicyDownload(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
void continueWithPolicyIgnore(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
void didReceiveData(Frame*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
void didFinishLoading(Frame*, DocumentLoader*, unsigned long identifier, double finishTime);
void didFailLoading(Frame*, DocumentLoader*, unsigned long identifier, const ResourceError&);
void documentThreadableLoaderStartedLoadingForClient(ScriptExecutionContext*, unsigned long identifier, ThreadableLoaderClient*);
void willLoadXHR(ScriptExecutionContext*, ThreadableLoaderClient*, const String&, const KURL&, bool, PassRefPtr<FormData>, const HTTPHeaderMap&, bool);
void didFailXHRLoading(ScriptExecutionContext*, ThreadableLoaderClient*);
void didFinishXHRLoading(ScriptExecutionContext*, ThreadableLoaderClient*, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber);
void didReceiveXHRResponse(ScriptExecutionContext*, unsigned long identifier);
void willLoadXHRSynchronously(ScriptExecutionContext*);
void didLoadXHRSynchronously(ScriptExecutionContext*);
void scriptImported(ScriptExecutionContext*, unsigned long identifier, const String& sourceString);
void scriptExecutionBlockedByCSP(ScriptExecutionContext*, const String& directiveText);
void didReceiveScriptResponse(ScriptExecutionContext*, unsigned long identifier);
void domContentLoadedEventFired(Frame*);
void loadEventFired(Frame*);
void frameDetachedFromParent(Frame*);
void didCommitLoad(Frame*, DocumentLoader*);
void frameDocumentUpdated(Frame*);
void loaderDetachedFromFrame(Frame*, DocumentLoader*);
void frameStartedLoading(Frame*);
void frameStoppedLoading(Frame*);
void frameScheduledNavigation(Frame*, double delay);
void frameClearedScheduledNavigation(Frame*);
InspectorInstrumentationCookie willRunJavaScriptDialog(Page*, const String& message);
void didRunJavaScriptDialog(const InspectorInstrumentationCookie&);
void willDestroyCachedResource(CachedResource*);

InspectorInstrumentationCookie willWriteHTML(Document*, unsigned startLine);
void didWriteHTML(const InspectorInstrumentationCookie&, unsigned endLine);

// FIXME: Remove once we no longer generate stacks outside of Inspector.
void addMessageToConsole(Page*, MessageSource, MessageType, MessageLevel, const String& message, PassRefPtr<ScriptCallStack>, unsigned long requestIdentifier = 0);
void addMessageToConsole(Page*, MessageSource, MessageType, MessageLevel, const String& message, ScriptState*, PassRefPtr<ScriptArguments>, unsigned long requestIdentifier = 0);
void addMessageToConsole(Page*, MessageSource, MessageType, MessageLevel, const String& message, const String&, unsigned lineNumber, ScriptState* = 0, unsigned long requestIdentifier = 0);
// FIXME: Convert to ScriptArguments to match non-worker context.
void addMessageToConsole(WorkerContext*, MessageSource, MessageType, MessageLevel, const String& message, PassRefPtr<ScriptCallStack>, unsigned long requestIdentifier = 0);
void addMessageToConsole(WorkerContext*, MessageSource, MessageType, MessageLevel, const String& message, const String&, unsigned lineNumber, ScriptState* = 0, unsigned long requestIdentifier = 0);
void consoleCount(Page*, ScriptState*, PassRefPtr<ScriptArguments>);
void startConsoleTiming(Frame*, const String& title);
void stopConsoleTiming(Frame*, const String& title, PassRefPtr<ScriptCallStack>);
void consoleTimeStamp(Frame*, PassRefPtr<ScriptArguments>);

void didRequestAnimationFrame(Document*, int callbackId);
void didCancelAnimationFrame(Document*, int callbackId);
InspectorInstrumentationCookie willFireAnimationFrame(Document*, int callbackId);
void didFireAnimationFrame(const InspectorInstrumentationCookie&);

void addStartProfilingMessageToConsole(Page*, const String& title, unsigned lineNumber, const String& sourceURL);
void addProfile(Page*, RefPtr<ScriptProfile>, PassRefPtr<ScriptCallStack>);
String getCurrentUserInitiatedProfileName(Page*, bool incrementProfileNumber);
bool profilerEnabled(Page*);

void didOpenDatabase(ScriptExecutionContext*, PassRefPtr<Database>, const String& domain, const String& name, const String& version);

void didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType, SecurityOrigin*, Page*);

bool shouldPauseDedicatedWorkerOnStart(ScriptExecutionContext*);
void didStartWorkerContext(ScriptExecutionContext*, WorkerContextProxy*, const KURL&);
void workerContextTerminated(ScriptExecutionContext*, WorkerContextProxy*);
void willEvaluateWorkerScript(WorkerContext*, int workerThreadStartMode);

void didCreateWebSocket(Document*, unsigned long identifier, const KURL& requestURL, const KURL& documentURL, const String& protocol);
void willSendWebSocketHandshakeRequest(Document*, unsigned long identifier, const WebSocketHandshakeRequest&);
void didReceiveWebSocketHandshakeResponse(Document*, unsigned long identifier, const WebSocketHandshakeResponse&);
void didCloseWebSocket(Document*, unsigned long identifier);
void didReceiveWebSocketFrame(Document*, unsigned long identifier, const WebSocketFrame&);
void didSendWebSocketFrame(Document*, unsigned long identifier, const WebSocketFrame&);
void didReceiveWebSocketFrameError(Document*, unsigned long identifier, const String& errorMessage);

ScriptObject wrapCanvas2DRenderingContextForInstrumentation(Document*, const ScriptObject&);
ScriptObject wrapWebGLRenderingContextForInstrumentation(Document*, const ScriptObject&);

void networkStateChanged(Page*);
void updateApplicationCacheStatus(Frame*);

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
bool canvasAgentEnabled(ScriptExecutionContext*);
bool consoleAgentEnabled(ScriptExecutionContext*);
bool timelineAgentEnabled(ScriptExecutionContext*);
bool collectingHTMLParseErrors(Page*);

GeolocationPosition* overrideGeolocationPosition(Page*, GeolocationPosition*);

void registerInstrumentingAgents(InstrumentingAgents*);
void unregisterInstrumentingAgents(InstrumentingAgents*);

DeviceOrientationData* overrideDeviceOrientation(Page*, DeviceOrientationData*);

void layerTreeDidChange(Page*);
void renderLayerDestroyed(Page*, const RenderLayer*);
void pseudoElementDestroyed(Page*, PseudoElement*);

void didClearWindowObjectInWorldImpl(InstrumentingAgents*, Frame*, DOMWrapperWorld*);
bool isDebuggerPausedImpl(InstrumentingAgents*);

void willInsertDOMNodeImpl(InstrumentingAgents*, Node* parent);
void didInsertDOMNodeImpl(InstrumentingAgents*, Node*);
void willRemoveDOMNodeImpl(InstrumentingAgents*, Node*);
void didRemoveDOMNodeImpl(InstrumentingAgents*, Node*);
void willModifyDOMAttrImpl(InstrumentingAgents*, Element*, const AtomicString& oldValue, const AtomicString& newValue);
void didModifyDOMAttrImpl(InstrumentingAgents*, Element*, const AtomicString& name, const AtomicString& value);
void didRemoveDOMAttrImpl(InstrumentingAgents*, Element*, const AtomicString& name);
void characterDataModifiedImpl(InstrumentingAgents*, CharacterData*);
void didInvalidateStyleAttrImpl(InstrumentingAgents*, Node*);
void activeStyleSheetsUpdatedImpl(InstrumentingAgents*, const Vector<RefPtr<StyleSheet> >&);
void frameWindowDiscardedImpl(InstrumentingAgents*, DOMWindow*);
void mediaQueryResultChangedImpl(InstrumentingAgents*);
void didPushShadowRootImpl(InstrumentingAgents*, Element* host, ShadowRoot*);
void willPopShadowRootImpl(InstrumentingAgents*, Element* host, ShadowRoot*);
void didCreateNamedFlowImpl(InstrumentingAgents*, Document*, NamedFlow*);
void willRemoveNamedFlowImpl(InstrumentingAgents*, Document*, NamedFlow*);
void didUpdateRegionLayoutImpl(InstrumentingAgents*, Document*, NamedFlow*);

void handleMouseMoveImpl(InstrumentingAgents*, Frame*, const PlatformMouseEvent&);
bool handleTouchEventImpl(InstrumentingAgents*, Node*);
bool handleMousePressImpl(InstrumentingAgents*);
bool forcePseudoStateImpl(InstrumentingAgents*, Element*, CSSSelector::PseudoType);

void willSendXMLHttpRequestImpl(InstrumentingAgents*, const String& url);
void didScheduleResourceRequestImpl(InstrumentingAgents*, const String& url, Frame*);
void didInstallTimerImpl(InstrumentingAgents*, int timerId, int timeout, bool singleShot, ScriptExecutionContext*);
void didRemoveTimerImpl(InstrumentingAgents*, int timerId, ScriptExecutionContext*);

InspectorInstrumentationCookie willCallFunctionImpl(InstrumentingAgents*, const String& scriptName, int scriptLine, ScriptExecutionContext*);
void didCallFunctionImpl(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willDispatchXHRReadyStateChangeEventImpl(InstrumentingAgents*, XMLHttpRequest*, ScriptExecutionContext*);
void didDispatchXHRReadyStateChangeEventImpl(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willDispatchEventImpl(InstrumentingAgents*, const Event&, DOMWindow*, Node*, const EventPath&, Document*);
InspectorInstrumentationCookie willHandleEventImpl(InstrumentingAgents*, Event*);
void didHandleEventImpl(const InspectorInstrumentationCookie&);
void didDispatchEventImpl(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willDispatchEventOnWindowImpl(InstrumentingAgents*, const Event&, DOMWindow*);
void didDispatchEventOnWindowImpl(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willEvaluateScriptImpl(InstrumentingAgents*, const String& url, int lineNumber, Frame*);
void didEvaluateScriptImpl(const InspectorInstrumentationCookie&);
void scriptsEnabledImpl(InstrumentingAgents*, bool isEnabled);
void didCreateIsolatedContextImpl(InstrumentingAgents*, Frame*, ScriptState*, SecurityOrigin*);
InspectorInstrumentationCookie willFireTimerImpl(InstrumentingAgents*, int timerId, ScriptExecutionContext*);
void didFireTimerImpl(const InspectorInstrumentationCookie&);
void didInvalidateLayoutImpl(InstrumentingAgents*, Frame*);
InspectorInstrumentationCookie willLayoutImpl(InstrumentingAgents*, Frame*);
void didLayoutImpl(const InspectorInstrumentationCookie&, RenderObject*);
void didScrollImpl(InstrumentingAgents*);
InspectorInstrumentationCookie willDispatchXHRLoadEventImpl(InstrumentingAgents*, XMLHttpRequest*, ScriptExecutionContext*);
void didDispatchXHRLoadEventImpl(const InspectorInstrumentationCookie&);
void willScrollLayerImpl(InstrumentingAgents*, Frame*);
void didScrollLayerImpl(InstrumentingAgents*);
void willPaintImpl(InstrumentingAgents*, RenderObject*);
void didPaintImpl(InstrumentingAgents*, RenderObject*, GraphicsContext*, const LayoutRect&);
InspectorInstrumentationCookie willRecalculateStyleImpl(InstrumentingAgents*, Frame*);
void didRecalculateStyleImpl(const InspectorInstrumentationCookie&);
void didRecalculateStyleForElementImpl(InstrumentingAgents*);
void didScheduleStyleRecalculationImpl(InstrumentingAgents*, Document*);
InspectorInstrumentationCookie willMatchRuleImpl(InstrumentingAgents*, StyleRule*, InspectorCSSOMWrappers&, DocumentStyleSheetCollection*);
void didMatchRuleImpl(const InspectorInstrumentationCookie&, bool matched);
InspectorInstrumentationCookie willProcessRuleImpl(InstrumentingAgents*, StyleRule*, StyleResolver*);
void didProcessRuleImpl(const InspectorInstrumentationCookie&);

void applyUserAgentOverrideImpl(InstrumentingAgents*, String*);
void applyScreenWidthOverrideImpl(InstrumentingAgents*, long*);
void applyScreenHeightOverrideImpl(InstrumentingAgents*, long*);
void applyEmulatedMediaImpl(InstrumentingAgents*, String*);
bool shouldApplyScreenWidthOverrideImpl(InstrumentingAgents*);
bool shouldApplyScreenHeightOverrideImpl(InstrumentingAgents*);
void willSendRequestImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
void continueAfterPingLoaderImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse&);
void markResourceAsCachedImpl(InstrumentingAgents*, unsigned long identifier);
void didLoadResourceFromMemoryCacheImpl(InstrumentingAgents*, DocumentLoader*, CachedResource*);
InspectorInstrumentationCookie willReceiveResourceDataImpl(InstrumentingAgents*, unsigned long identifier, Frame*, int length);
void didReceiveResourceDataImpl(const InspectorInstrumentationCookie&);
InspectorInstrumentationCookie willReceiveResourceResponseImpl(InstrumentingAgents*, unsigned long identifier, const ResourceResponse&, Frame*);
void didReceiveResourceResponseImpl(const InspectorInstrumentationCookie&, unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
void didReceiveResourceResponseButCanceledImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
void continueAfterXFrameOptionsDeniedImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
void continueWithPolicyDownloadImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
void continueWithPolicyIgnoreImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
void didReceiveDataImpl(InstrumentingAgents*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
void didFinishLoadingImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, double finishTime);
void didFailLoadingImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, const ResourceError&);
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
void didCommitLoadImpl(InstrumentingAgents*, Page*, DocumentLoader*);
void frameDocumentUpdatedImpl(InstrumentingAgents*, Frame*);
void loaderDetachedFromFrameImpl(InstrumentingAgents*, DocumentLoader*);
void frameStartedLoadingImpl(InstrumentingAgents*, Frame*);
void frameStoppedLoadingImpl(InstrumentingAgents*, Frame*);
void frameScheduledNavigationImpl(InstrumentingAgents*, Frame*, double delay);
void frameClearedScheduledNavigationImpl(InstrumentingAgents*, Frame*);
InspectorInstrumentationCookie willRunJavaScriptDialogImpl(InstrumentingAgents*, const String& message);
void didRunJavaScriptDialogImpl(const InspectorInstrumentationCookie&);
void willDestroyCachedResourceImpl(CachedResource*);

InspectorInstrumentationCookie willWriteHTMLImpl(InstrumentingAgents*, unsigned startLine, Frame*);
void didWriteHTMLImpl(const InspectorInstrumentationCookie&, unsigned endLine);

void addMessageToConsoleImpl(InstrumentingAgents*, MessageSource, MessageType, MessageLevel, const String& message, ScriptState*, PassRefPtr<ScriptArguments>, unsigned long requestIdentifier);
void addMessageToConsoleImpl(InstrumentingAgents*, MessageSource, MessageType, MessageLevel, const String& message, const String& scriptId, unsigned lineNumber, ScriptState*, unsigned long requestIdentifier);

// FIXME: Remove once we no longer generate stacks outside of Inspector.
void addMessageToConsoleImpl(InstrumentingAgents*, MessageSource, MessageType, MessageLevel, const String& message, PassRefPtr<ScriptCallStack>, unsigned long requestIdentifier);

void consoleCountImpl(InstrumentingAgents*, ScriptState*, PassRefPtr<ScriptArguments>);
void startConsoleTimingImpl(InstrumentingAgents*, Frame*, const String& title);
void stopConsoleTimingImpl(InstrumentingAgents*, Frame*, const String& title, PassRefPtr<ScriptCallStack>);
void consoleTimeStampImpl(InstrumentingAgents*, Frame*, PassRefPtr<ScriptArguments>);

void didRequestAnimationFrameImpl(InstrumentingAgents*, int callbackId, Frame*);
void didCancelAnimationFrameImpl(InstrumentingAgents*, int callbackId, Frame*);
InspectorInstrumentationCookie willFireAnimationFrameImpl(InstrumentingAgents*, int callbackId, Frame*);
void didFireAnimationFrameImpl(const InspectorInstrumentationCookie&);

void addStartProfilingMessageToConsoleImpl(InstrumentingAgents*, const String& title, unsigned lineNumber, const String& sourceURL);
void addProfileImpl(InstrumentingAgents*, RefPtr<ScriptProfile>, PassRefPtr<ScriptCallStack>);
String getCurrentUserInitiatedProfileNameImpl(InstrumentingAgents*, bool incrementProfileNumber);
bool profilerEnabledImpl(InstrumentingAgents*);

void didOpenDatabaseImpl(InstrumentingAgents*, PassRefPtr<Database>, const String& domain, const String& name, const String& version);

void didDispatchDOMStorageEventImpl(InstrumentingAgents*, const String& key, const String& oldValue, const String& newValue, StorageType, SecurityOrigin*, Page*);

bool shouldPauseDedicatedWorkerOnStartImpl(InstrumentingAgents*);
void didStartWorkerContextImpl(InstrumentingAgents*, WorkerContextProxy*, const KURL&);
void workerContextTerminatedImpl(InstrumentingAgents*, WorkerContextProxy*);

void didCreateWebSocketImpl(InstrumentingAgents*, unsigned long identifier, const KURL& requestURL, const KURL& documentURL, const String& protocol, Document*);
void willSendWebSocketHandshakeRequestImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketHandshakeRequest&, Document*);
void didReceiveWebSocketHandshakeResponseImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketHandshakeResponse&, Document*);
void didCloseWebSocketImpl(InstrumentingAgents*, unsigned long identifier, Document*);
void didReceiveWebSocketFrameImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketFrame&);
void didSendWebSocketFrameImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketFrame&);
void didReceiveWebSocketFrameErrorImpl(InstrumentingAgents*, unsigned long identifier, const String&);

void networkStateChangedImpl(InstrumentingAgents*);
void updateApplicationCacheStatusImpl(InstrumentingAgents*, Frame*);

InstrumentingAgents* instrumentingAgentsForPage(Page*);
InstrumentingAgents* instrumentingAgentsForFrame(Frame*);
InstrumentingAgents* instrumentingAgentsForContext(ScriptExecutionContext*);
InstrumentingAgents* instrumentingAgentsForDocument(Document*);
InstrumentingAgents* instrumentingAgentsForRenderer(RenderObject*);

InstrumentingAgents* instrumentingAgentsForWorkerContext(WorkerContext*);
InstrumentingAgents* instrumentingAgentsForNonDocumentContext(ScriptExecutionContext*);

bool collectingHTMLParseErrors(InstrumentingAgents*);
void pauseOnNativeEventIfNeeded(InstrumentingAgents*, bool isDOMEvent, const String& eventName, bool synchronous);
void cancelPauseOnNativeEvent(InstrumentingAgents*);
InspectorTimelineAgent* retrieveTimelineAgent(const InspectorInstrumentationCookie&);

GeolocationPosition* overrideGeolocationPositionImpl(InstrumentingAgents*, GeolocationPosition*);

DeviceOrientationData* overrideDeviceOrientationImpl(InstrumentingAgents*, DeviceOrientationData*);

void layerTreeDidChangeImpl(InstrumentingAgents*);
void renderLayerDestroyedImpl(InstrumentingAgents*, const RenderLayer*);
void pseudoElementDestroyedImpl(InstrumentingAgents*, PseudoElement*);

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

inline void didClearWindowObjectInWorld(Frame* frame, DOMWrapperWorld* world)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didClearWindowObjectInWorldImpl(instrumentingAgents, frame, world);
}

inline bool isDebuggerPaused(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return isDebuggerPausedImpl(instrumentingAgents);
    return false;
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

inline void willRemoveDOMNode(Document* document, Node* node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document)) {
        willRemoveDOMNodeImpl(instrumentingAgents, node);
        didRemoveDOMNodeImpl(instrumentingAgents, node);
    }
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
        activeStyleSheetsUpdatedImpl(instrumentingAgents, newSheets);
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

inline void didPushShadowRoot(Element* host, ShadowRoot* root)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(host->ownerDocument()))
        didPushShadowRootImpl(instrumentingAgents, host, root);
}

inline void willPopShadowRoot(Element* host, ShadowRoot* root)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(host->ownerDocument()))
        willPopShadowRootImpl(instrumentingAgents, host, root);
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

inline void handleMouseMove(Frame* frame, const PlatformMouseEvent& event)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(frame->page()))
        handleMouseMoveImpl(instrumentingAgents, frame, event);
}

inline bool handleTouchEvent(Page* page, Node* node)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return handleTouchEventImpl(instrumentingAgents, node);
    return false;
}

inline bool handleMousePress(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return handleMousePressImpl(instrumentingAgents);
    return false;
}

inline bool forcePseudoState(Element* element, CSSSelector::PseudoType pseudoState)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(element->document()))
        return forcePseudoStateImpl(instrumentingAgents, element, pseudoState);
    return false;
}

inline void characterDataModified(Document* document, CharacterData* characterData)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        characterDataModifiedImpl(instrumentingAgents, characterData);
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
        didScheduleResourceRequestImpl(instrumentingAgents, url, document->frame());
}

inline void didInstallTimer(ScriptExecutionContext* context, int timerId, int timeout, bool singleShot)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didInstallTimerImpl(instrumentingAgents, timerId, timeout, singleShot, context);
}

inline void didRemoveTimer(ScriptExecutionContext* context, int timerId)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didRemoveTimerImpl(instrumentingAgents, timerId, context);
}

inline InspectorInstrumentationCookie willCallFunction(ScriptExecutionContext* context, const String& scriptName, int scriptLine)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willCallFunctionImpl(instrumentingAgents, scriptName, scriptLine, context);
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
        return willDispatchXHRReadyStateChangeEventImpl(instrumentingAgents, request, context);
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
        return willDispatchEventImpl(instrumentingAgents, event, window, node, eventPath, document);
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
        return willEvaluateScriptImpl(instrumentingAgents, url, lineNumber, frame);
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
        return scriptsEnabledImpl(instrumentingAgents, isEnabled);
}

inline void didCreateIsolatedContext(Frame* frame, ScriptState* scriptState, SecurityOrigin* origin)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return didCreateIsolatedContextImpl(instrumentingAgents, frame, scriptState, origin);
}

inline InspectorInstrumentationCookie willFireTimer(ScriptExecutionContext* context, int timerId)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willFireTimerImpl(instrumentingAgents, timerId, context);
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

inline InspectorInstrumentationCookie willDispatchXHRLoadEvent(ScriptExecutionContext* context, XMLHttpRequest* request)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willDispatchXHRLoadEventImpl(instrumentingAgents, request, context);
    return InspectorInstrumentationCookie();
}

inline void didDispatchXHRLoadEvent(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didDispatchXHRLoadEventImpl(cookie);
}

inline void willPaint(RenderObject* renderer)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForRenderer(renderer))
        return willPaintImpl(instrumentingAgents, renderer);
}

inline void didPaint(RenderObject* renderer, GraphicsContext* context, const LayoutRect& rect)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForRenderer(renderer))
        didPaintImpl(instrumentingAgents, renderer, context, rect);
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

inline InspectorInstrumentationCookie willRecalculateStyle(Document* document)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willRecalculateStyleImpl(instrumentingAgents, document->frame());
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
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(element->document()))
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

inline InspectorInstrumentationCookie willProcessRule(Document* document, StyleRule* rule, StyleResolver* styleResolver)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (!rule)
        return InspectorInstrumentationCookie();
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willProcessRuleImpl(instrumentingAgents, rule, styleResolver);
    return InspectorInstrumentationCookie();
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

inline bool shouldApplyScreenWidthOverride(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return shouldApplyScreenWidthOverrideImpl(instrumentingAgents);
    return false;
}

inline void applyEmulatedMedia(Frame* frame, String* media)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyEmulatedMediaImpl(instrumentingAgents, media);
}

inline bool shouldApplyScreenHeightOverride(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return shouldApplyScreenHeightOverrideImpl(instrumentingAgents);
    return false;
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

inline InspectorInstrumentationCookie willReceiveResourceData(Frame* frame, unsigned long identifier, int length)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willReceiveResourceDataImpl(instrumentingAgents, identifier, frame, length);
    return InspectorInstrumentationCookie();
}

inline void didReceiveResourceData(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didReceiveResourceDataImpl(cookie);
}

inline InspectorInstrumentationCookie willReceiveResourceResponse(Frame* frame, unsigned long identifier, const ResourceResponse& response)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willReceiveResourceResponseImpl(instrumentingAgents, identifier, response, frame);
    return InspectorInstrumentationCookie();
}

inline void didReceiveResourceResponse(const InspectorInstrumentationCookie& cookie, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    // Call this unconditionally so that we're able to log to console with no front-end attached.
    if (cookie.isValid())
        didReceiveResourceResponseImpl(cookie, identifier, loader, response, resourceLoader);
}

inline void continueAfterXFrameOptionsDenied(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    continueAfterXFrameOptionsDeniedImpl(frame, loader, identifier, r);
}

inline void continueWithPolicyDownload(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    continueWithPolicyDownloadImpl(frame, loader, identifier, r);
}

inline void continueWithPolicyIgnore(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    continueWithPolicyIgnoreImpl(frame, loader, identifier, r);
}

inline void didReceiveData(Frame* frame, unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didReceiveDataImpl(instrumentingAgents, identifier, data, dataLength, encodedDataLength);
}

inline void didFinishLoading(Frame* frame, DocumentLoader* loader, unsigned long identifier, double finishTime)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didFinishLoadingImpl(instrumentingAgents, identifier, loader, finishTime);
}

inline void didFailLoading(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceError& error)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didFailLoadingImpl(instrumentingAgents, identifier, loader, error);
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
        didCommitLoadImpl(instrumentingAgents, frame->page(), loader);
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

inline void willDestroyCachedResource(CachedResource* cachedResource)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    willDestroyCachedResourceImpl(cachedResource);
}

inline InspectorInstrumentationCookie willWriteHTML(Document* document, unsigned startLine)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willWriteHTMLImpl(instrumentingAgents, startLine, document->frame());
    return InspectorInstrumentationCookie();
}

inline void didWriteHTML(const InspectorInstrumentationCookie& cookie, unsigned endLine)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didWriteHTMLImpl(cookie, endLine);
}

inline void didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType storageType, SecurityOrigin* securityOrigin, Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didDispatchDOMStorageEventImpl(instrumentingAgents, key, oldValue, newValue, storageType, securityOrigin, page);
}

inline bool shouldPauseDedicatedWorkerOnStart(ScriptExecutionContext* context)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return shouldPauseDedicatedWorkerOnStartImpl(instrumentingAgents);
    return false;
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
        didCreateWebSocketImpl(instrumentingAgents, identifier, requestURL, documentURL, protocol, document);
}

inline void willSendWebSocketHandshakeRequest(Document* document, unsigned long identifier, const WebSocketHandshakeRequest& request)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willSendWebSocketHandshakeRequestImpl(instrumentingAgents, identifier, request, document);
}

inline void didReceiveWebSocketHandshakeResponse(Document* document, unsigned long identifier, const WebSocketHandshakeResponse& response)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didReceiveWebSocketHandshakeResponseImpl(instrumentingAgents, identifier, response, document);
}

inline void didCloseWebSocket(Document* document, unsigned long identifier)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCloseWebSocketImpl(instrumentingAgents, identifier, document);
}
inline void didReceiveWebSocketFrame(Document* document, unsigned long identifier, const WebSocketFrame& frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didReceiveWebSocketFrameImpl(instrumentingAgents, identifier, frame);
}
inline void didReceiveWebSocketFrameError(Document* document, unsigned long identifier, const String& errorMessage)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didReceiveWebSocketFrameErrorImpl(instrumentingAgents, identifier, errorMessage);
}
inline void didSendWebSocketFrame(Document* document, unsigned long identifier, const WebSocketFrame& frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didSendWebSocketFrameImpl(instrumentingAgents, identifier, frame);
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

inline void didRequestAnimationFrame(Document* document, int callbackId)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didRequestAnimationFrameImpl(instrumentingAgents, callbackId, document->frame());
}

inline void didCancelAnimationFrame(Document* document, int callbackId)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCancelAnimationFrameImpl(instrumentingAgents, callbackId, document->frame());
}

inline InspectorInstrumentationCookie willFireAnimationFrame(Document* document, int callbackId)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willFireAnimationFrameImpl(instrumentingAgents, callbackId, document->frame());
    return InspectorInstrumentationCookie();
}

inline void didFireAnimationFrame(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didFireAnimationFrameImpl(cookie);
}

inline GeolocationPosition* overrideGeolocationPosition(Page* page, GeolocationPosition* position)
{
    FAST_RETURN_IF_NO_FRONTENDS(position);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return overrideGeolocationPositionImpl(instrumentingAgents, position);
    return position;
}

inline DeviceOrientationData* overrideDeviceOrientation(Page* page, DeviceOrientationData* deviceOrientation)
{
    FAST_RETURN_IF_NO_FRONTENDS(deviceOrientation);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return overrideDeviceOrientationImpl(instrumentingAgents, deviceOrientation);
    return deviceOrientation;
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

inline bool collectingHTMLParseErrors(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return collectingHTMLParseErrors(instrumentingAgents);
    return false;
}

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

} // namespace InspectorInstrumentation

} // namespace WebCore

#endif // !defined(InspectorInstrumentation_h)
