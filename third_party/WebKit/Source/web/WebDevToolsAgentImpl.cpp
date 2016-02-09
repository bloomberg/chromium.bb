/*
 * Copyright (C) 2010-2011 Google Inc. All rights reserved.
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

#include "web/WebDevToolsAgentImpl.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/InspectorBackendDispatcher.h"
#include "core/InspectorFrontend.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorAnimationAgent.h"
#include "core/inspector/InspectorApplicationCacheAgent.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "core/inspector/InspectorDebuggerAgent.h"
#include "core/inspector/InspectorHeapProfilerAgent.h"
#include "core/inspector/InspectorInputAgent.h"
#include "core/inspector/InspectorInspectorAgent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorLayerTreeAgent.h"
#include "core/inspector/InspectorMemoryAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorProfilerAgent.h"
#include "core/inspector/InspectorResourceAgent.h"
#include "core/inspector/InspectorResourceContentLoader.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/inspector/InspectorTracingAgent.h"
#include "core/inspector/InspectorWorkerAgent.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/LayoutEditor.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/inspector/PageConsoleAgent.h"
#include "core/inspector/PageDebuggerAgent.h"
#include "core/inspector/PageRuntimeAgent.h"
#include "core/inspector/v8/InjectedScriptHost.h"
#include "core/inspector/v8/InjectedScriptManager.h"
#include "core/layout/LayoutView.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "modules/accessibility/InspectorAccessibilityAgent.h"
#include "modules/cachestorage/InspectorCacheStorageAgent.h"
#include "modules/device_orientation/DeviceOrientationInspectorAgent.h"
#include "modules/filesystem/InspectorFileSystemAgent.h"
#include "modules/indexeddb/InspectorIndexedDBAgent.h"
#include "modules/screen_orientation/ScreenOrientationInspectorAgent.h"
#include "modules/storage/InspectorDOMStorageAgent.h"
#include "modules/webdatabase/InspectorDatabaseAgent.h"
#include "platform/JSONValues.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "public/platform/Platform.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebString.h"
#include "public/web/WebDevToolsAgentClient.h"
#include "public/web/WebSettings.h"
#include "web/DevToolsEmulator.h"
#include "web/InspectorEmulationAgent.h"
#include "web/InspectorOverlay.h"
#include "web/InspectorRenderingAgent.h"
#include "web/WebFrameWidgetImpl.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebSettingsImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/MathExtras.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ClientMessageLoopAdapter : public MainThreadDebugger::ClientMessageLoop {
public:
    ~ClientMessageLoopAdapter() override
    {
        s_instance = nullptr;
    }

    static void ensureMainThreadDebuggerCreated(WebDevToolsAgentClient* client)
    {
        if (s_instance)
            return;
        OwnPtr<ClientMessageLoopAdapter> instance = adoptPtr(new ClientMessageLoopAdapter(adoptPtr(client->createClientMessageLoop())));
        s_instance = instance.get();
        v8::Isolate* isolate = V8PerIsolateData::mainThreadIsolate();
        V8PerIsolateData* data = V8PerIsolateData::from(isolate);
        data->setThreadDebugger(MainThreadDebugger::create(instance.release(), isolate));
    }

    static void webViewImplClosed(WebViewImpl* view)
    {
        if (s_instance)
            s_instance->m_frozenViews.remove(view);
    }

    static void webFrameWidgetImplClosed(WebFrameWidgetImpl* widget)
    {
        if (s_instance)
            s_instance->m_frozenWidgets.remove(widget);
    }

    static void continueProgram()
    {
        // Release render thread if necessary.
        if (s_instance)
            s_instance->quitNow();
    }

    static void pauseForCreateWindow(WebLocalFrameImpl* frame)
    {
        if (s_instance)
            s_instance->runForCreateWindow(frame);
    }

    static bool resumeForCreateWindow()
    {
        return s_instance ? s_instance->quitForCreateWindow() : false;
    }

private:
    ClientMessageLoopAdapter(PassOwnPtr<WebDevToolsAgentClient::WebKitClientMessageLoop> messageLoop)
        : m_runningForDebugBreak(false)
        , m_runningForCreateWindow(false)
        , m_messageLoop(messageLoop) { }

    void run(LocalFrame* frame) override
    {
        if (m_runningForDebugBreak)
            return;

        m_runningForDebugBreak = true;
        if (!m_runningForCreateWindow)
            runLoop(WebLocalFrameImpl::fromFrame(frame));
    }

    void runForCreateWindow(WebLocalFrameImpl* frame)
    {
        if (m_runningForCreateWindow)
            return;

        m_runningForCreateWindow = true;
        if (!m_runningForDebugBreak)
            runLoop(frame);
    }

    void runLoop(WebLocalFrameImpl* frame)
    {
        // 0. Flush pending frontend messages.
        WebDevToolsAgentImpl* agent = frame->devToolsAgentImpl();
        agent->flushPendingProtocolNotifications();

        Vector<WebViewImpl*> views;
        WillBeHeapVector<RawPtrWillBeMember<WebFrameWidgetImpl>> widgets;

        // 1. Disable input events.
        const HashSet<WebViewImpl*>& viewImpls = WebViewImpl::allInstances();
        HashSet<WebViewImpl*>::const_iterator viewImplsEnd = viewImpls.end();
        for (HashSet<WebViewImpl*>::const_iterator it =  viewImpls.begin(); it != viewImplsEnd; ++it) {
            WebViewImpl* view = *it;
            m_frozenViews.add(view);
            views.append(view);
            view->setIgnoreInputEvents(true);
        }

        const WebFrameWidgetsSet& widgetImpls = WebFrameWidgetImpl::allInstances();
        WebFrameWidgetsSet::const_iterator widgetImplsEnd = widgetImpls.end();
        for (WebFrameWidgetsSet::const_iterator it =  widgetImpls.begin(); it != widgetImplsEnd; ++it) {
            WebFrameWidgetImpl* widget = *it;
            m_frozenWidgets.add(widget);
            widgets.append(widget);
            widget->setIgnoreInputEvents(true);
        }

        // 2. Notify embedder about pausing.
        agent->client()->willEnterDebugLoop();

        // 3. Disable active objects
        WebView::willEnterModalLoop();

        // 4. Process messages until quitNow is called.
        m_messageLoop->run();

        // 5. Resume active objects
        WebView::didExitModalLoop();

        // 6. Resume input events.
        for (Vector<WebViewImpl*>::iterator it = views.begin(); it != views.end(); ++it) {
            if (m_frozenViews.contains(*it)) {
                // The view was not closed during the dispatch.
                (*it)->setIgnoreInputEvents(false);
            }
        }
        for (WillBeHeapVector<RawPtrWillBeMember<WebFrameWidgetImpl>>::iterator it = widgets.begin(); it != widgets.end(); ++it) {
            if (m_frozenWidgets.contains(*it)) {
                // The widget was not closed during the dispatch.
                (*it)->setIgnoreInputEvents(false);
            }
        }

        // 7. Notify embedder about resuming.
        agent->client()->didExitDebugLoop();

        // 8. All views have been resumed, clear the set.
        m_frozenViews.clear();
        m_frozenWidgets.clear();
    }

    void quitNow() override
    {
        if (m_runningForDebugBreak) {
            m_runningForDebugBreak = false;
            if (!m_runningForCreateWindow)
                m_messageLoop->quitNow();
        }
    }

    bool quitForCreateWindow()
    {
        if (m_runningForCreateWindow) {
            m_runningForCreateWindow = false;
            if (!m_runningForDebugBreak)
                m_messageLoop->quitNow();
            return true;
        }
        return false;
    }

    bool m_runningForDebugBreak;
    bool m_runningForCreateWindow;
    OwnPtr<WebDevToolsAgentClient::WebKitClientMessageLoop> m_messageLoop;
    typedef HashSet<WebViewImpl*> FrozenViewsSet;
    FrozenViewsSet m_frozenViews;
    WebFrameWidgetsSet m_frozenWidgets;
    static ClientMessageLoopAdapter* s_instance;
};

ClientMessageLoopAdapter* ClientMessageLoopAdapter::s_instance = nullptr;

class PageInjectedScriptHostClient: public InjectedScriptHostClient {
public:
    PageInjectedScriptHostClient() { }

    ~PageInjectedScriptHostClient() override { }

    void muteWarningsAndDeprecations()
    {
        FrameConsole::mute();
        UseCounter::muteForInspector();
    }

    void unmuteWarningsAndDeprecations()
    {
        FrameConsole::unmute();
        UseCounter::unmuteForInspector();
    }
};

class DebuggerTask : public InspectorTaskRunner::Task {
public:
    DebuggerTask(int sessionId, PassOwnPtr<WebDevToolsAgent::MessageDescriptor> descriptor)
        : m_sessionId(sessionId)
        , m_descriptor(descriptor)
    {
    }

    ~DebuggerTask() override {}
    virtual void run()
    {
        WebDevToolsAgent* webagent = m_descriptor->agent();
        if (!webagent)
            return;

        WebDevToolsAgentImpl* agentImpl = static_cast<WebDevToolsAgentImpl*>(webagent);
        if (agentImpl->m_attached)
            agentImpl->dispatchMessageFromFrontend(m_sessionId, m_descriptor->message());
    }

private:
    int m_sessionId;
    OwnPtr<WebDevToolsAgent::MessageDescriptor> m_descriptor;
};

// static
PassOwnPtrWillBeRawPtr<WebDevToolsAgentImpl> WebDevToolsAgentImpl::create(WebLocalFrameImpl* frame, WebDevToolsAgentClient* client)
{
    WebViewImpl* view = frame->viewImpl();
    // TODO(dgozman): sometimes view->mainFrameImpl() does return null, even though |frame| is meant to be main frame.
    // See http://crbug.com/526162.
    bool isMainFrame = view && !frame->parent();
    if (!isMainFrame) {
        WebDevToolsAgentImpl* agent = new WebDevToolsAgentImpl(frame, client, nullptr);
        if (frame->frameWidget())
            agent->layerTreeViewChanged(toWebFrameWidgetImpl(frame->frameWidget())->layerTreeView());
        return adoptPtrWillBeNoop(agent);
    }

    WebDevToolsAgentImpl* agent = new WebDevToolsAgentImpl(frame, client, InspectorOverlay::create(view));
    // TODO(dgozman): we should actually pass the view instead of frame, but during
    // remote->local transition we cannot access mainFrameImpl() yet, so we have to store the
    // frame which will become the main frame later.
    agent->registerAgent(InspectorRenderingAgent::create(frame));
    agent->registerAgent(InspectorEmulationAgent::create(frame, agent));
    // TODO(dgozman): migrate each of the following agents to frame once module is ready.
    agent->registerAgent(InspectorDatabaseAgent::create(view->page()));
    agent->registerAgent(DeviceOrientationInspectorAgent::create(view->page()));
    agent->registerAgent(InspectorAccessibilityAgent::create(view->page()));
    agent->registerAgent(InspectorDOMStorageAgent::create(view->page()));
    agent->registerAgent(InspectorCacheStorageAgent::create());
    agent->layerTreeViewChanged(view->layerTreeView());
    return adoptPtrWillBeNoop(agent);
}

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebLocalFrameImpl* webLocalFrameImpl,
    WebDevToolsAgentClient* client,
    PassOwnPtrWillBeRawPtr<InspectorOverlay> overlay)
    : m_client(client)
    , m_webLocalFrameImpl(webLocalFrameImpl)
    , m_attached(false)
#if ENABLE(ASSERT)
    , m_hasBeenDisposed(false)
#endif
    , m_instrumentingAgents(m_webLocalFrameImpl->frame()->instrumentingAgents())
    , m_resourceContentLoader(InspectorResourceContentLoader::create(m_webLocalFrameImpl->frame()))
    , m_overlay(overlay)
    , m_inspectedFrames(InspectedFrames::create(m_webLocalFrameImpl->frame()))
    , m_inspectorAgent(nullptr)
    , m_domAgent(nullptr)
    , m_pageAgent(nullptr)
    , m_resourceAgent(nullptr)
    , m_layerTreeAgent(nullptr)
    , m_tracingAgent(nullptr)
    , m_pageRuntimeAgent(nullptr)
    , m_pageConsoleAgent(nullptr)
    , m_agents(m_instrumentingAgents.get())
    , m_deferredAgentsInitialized(false)
    , m_sessionId(0)
    , m_stateMuted(false)
{
    ASSERT(isMainThread());
    ASSERT(m_webLocalFrameImpl->frame());

    long processId = Platform::current()->getUniqueIdForProcess();
    ASSERT(processId > 0);
    IdentifiersFactory::setProcessId(processId);

    ClientMessageLoopAdapter::ensureMainThreadDebuggerCreated(m_client);
    MainThreadDebugger* mainThreadDebugger = MainThreadDebugger::instance();
    m_injectedScriptManager = InjectedScriptManager::create(mainThreadDebugger);
    InjectedScriptManager* injectedScriptManager = m_injectedScriptManager.get();

    OwnPtrWillBeRawPtr<InspectorInspectorAgent> inspectorAgentPtr(InspectorInspectorAgent::create(injectedScriptManager));
    m_inspectorAgent = inspectorAgentPtr.get();
    m_agents.append(inspectorAgentPtr.release());

    OwnPtrWillBeRawPtr<InspectorDOMAgent> domAgentPtr(InspectorDOMAgent::create(m_inspectedFrames.get(), injectedScriptManager, m_overlay.get()));
    m_domAgent = domAgentPtr.get();
    m_agents.append(domAgentPtr.release());

    OwnPtrWillBeRawPtr<InspectorLayerTreeAgent> layerTreeAgentPtr(InspectorLayerTreeAgent::create(m_inspectedFrames.get()));
    m_layerTreeAgent = layerTreeAgentPtr.get();
    m_agents.append(layerTreeAgentPtr.release());

    OwnPtrWillBeRawPtr<PageRuntimeAgent> pageRuntimeAgentPtr(PageRuntimeAgent::create(injectedScriptManager, this, mainThreadDebugger->debugger(), m_inspectedFrames.get()));
    m_pageRuntimeAgent = pageRuntimeAgentPtr.get();
    m_agents.append(pageRuntimeAgentPtr.release());

    OwnPtrWillBeRawPtr<PageConsoleAgent> pageConsoleAgentPtr = PageConsoleAgent::create(injectedScriptManager, m_domAgent, m_inspectedFrames.get());
    m_pageConsoleAgent = pageConsoleAgentPtr.get();

    OwnPtrWillBeRawPtr<InspectorWorkerAgent> workerAgentPtr = InspectorWorkerAgent::create(pageConsoleAgentPtr.get());

    OwnPtrWillBeRawPtr<InspectorTracingAgent> tracingAgentPtr = InspectorTracingAgent::create(this, workerAgentPtr.get(), m_inspectedFrames.get());
    m_tracingAgent = tracingAgentPtr.get();
    m_agents.append(tracingAgentPtr.release());

    m_agents.append(workerAgentPtr.release());
    m_agents.append(pageConsoleAgentPtr.release());

    m_agents.append(ScreenOrientationInspectorAgent::create(*m_webLocalFrameImpl->frame()));
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl()
{
    ASSERT(m_hasBeenDisposed);
}

void WebDevToolsAgentImpl::dispose()
{
    // Explicitly dispose of the agent before destructing to ensure
    // same behavior (and correctness) with and without Oilpan.
    if (m_attached)
        Platform::current()->currentThread()->removeTaskObserver(this);
#if ENABLE(ASSERT)
    ASSERT(!m_hasBeenDisposed);
    m_hasBeenDisposed = true;
#endif
}

// static
void WebDevToolsAgentImpl::webViewImplClosed(WebViewImpl* webViewImpl)
{
    ClientMessageLoopAdapter::webViewImplClosed(webViewImpl);
}

// static
void WebDevToolsAgentImpl::webFrameWidgetImplClosed(WebFrameWidgetImpl* webFrameWidgetImpl)
{
    ClientMessageLoopAdapter::webFrameWidgetImplClosed(webFrameWidgetImpl);
}

DEFINE_TRACE(WebDevToolsAgentImpl)
{
    visitor->trace(m_webLocalFrameImpl);
    visitor->trace(m_instrumentingAgents);
    visitor->trace(m_resourceContentLoader);
    visitor->trace(m_overlay);
    visitor->trace(m_inspectedFrames);
    visitor->trace(m_inspectorAgent);
    visitor->trace(m_domAgent);
    visitor->trace(m_pageAgent);
    visitor->trace(m_resourceAgent);
    visitor->trace(m_layerTreeAgent);
    visitor->trace(m_tracingAgent);
    visitor->trace(m_pageRuntimeAgent);
    visitor->trace(m_pageConsoleAgent);
    visitor->trace(m_agents);
}

void WebDevToolsAgentImpl::willBeDestroyed()
{
    ASSERT(m_webLocalFrameImpl->frame());
    ASSERT(m_inspectedFrames->root()->view());

    detach();
    m_injectedScriptManager->disconnect();
    m_resourceContentLoader->dispose();
    m_agents.discardAgents();
    m_instrumentingAgents->reset();
}

void WebDevToolsAgentImpl::initializeDeferredAgents()
{
    if (m_deferredAgentsInitialized)
        return;
    m_deferredAgentsInitialized = true;

    InjectedScriptManager* injectedScriptManager = m_injectedScriptManager.get();

    OwnPtrWillBeRawPtr<InspectorResourceAgent> resourceAgentPtr(InspectorResourceAgent::create(m_inspectedFrames.get()));
    m_resourceAgent = resourceAgentPtr.get();
    m_agents.append(resourceAgentPtr.release());

    OwnPtrWillBeRawPtr<InspectorCSSAgent> cssAgentPtr(InspectorCSSAgent::create(m_domAgent, m_inspectedFrames.get(), m_resourceAgent, m_resourceContentLoader.get()));
    InspectorCSSAgent* cssAgent = cssAgentPtr.get();
    m_agents.append(cssAgentPtr.release());

    m_agents.append(InspectorAnimationAgent::create(m_inspectedFrames.get(), m_domAgent, cssAgent, injectedScriptManager));

    m_agents.append(InspectorMemoryAgent::create());

    m_agents.append(InspectorApplicationCacheAgent::create(m_inspectedFrames.get()));
    m_agents.append(InspectorFileSystemAgent::create(m_inspectedFrames.get()));
    m_agents.append(InspectorIndexedDBAgent::create(m_inspectedFrames.get()));

    OwnPtrWillBeRawPtr<InspectorDebuggerAgent> debuggerAgentPtr(PageDebuggerAgent::create(MainThreadDebugger::instance(), m_inspectedFrames.get(), injectedScriptManager));
    InspectorDebuggerAgent* debuggerAgent = debuggerAgentPtr.get();
    m_agents.append(debuggerAgentPtr.release());

    m_agents.append(InspectorDOMDebuggerAgent::create(injectedScriptManager, m_domAgent, debuggerAgent->v8DebuggerAgent()));

    m_agents.append(InspectorInputAgent::create(m_inspectedFrames.get()));

    v8::Isolate* isolate = V8PerIsolateData::mainThreadIsolate();
    m_agents.append(InspectorProfilerAgent::create(MainThreadDebugger::instance()->debugger(), m_overlay.get()));

    m_agents.append(InspectorHeapProfilerAgent::create(isolate, injectedScriptManager));

    OwnPtrWillBeRawPtr<InspectorPageAgent> pageAgentPtr(InspectorPageAgent::create(m_inspectedFrames.get(), this, m_resourceContentLoader.get(), debuggerAgent));
    m_pageAgent = pageAgentPtr.get();
    m_agents.append(pageAgentPtr.release());

    m_pageConsoleAgent->setDebuggerAgent(debuggerAgent->v8DebuggerAgent());

    m_injectedScriptManager->injectedScriptHost()->init(
        bind<PassRefPtr<TypeBuilder::Runtime::RemoteObject>, PassRefPtr<JSONObject>>(&InspectorInspectorAgent::inspect, m_inspectorAgent.get()),
        bind<>(&InspectorConsoleAgent::clearAllMessages, m_pageConsoleAgent.get()),
        adoptPtr(new PageInjectedScriptHostClient()));

    if (m_overlay)
        m_overlay->init(cssAgent, debuggerAgent, m_domAgent.get());
}

void WebDevToolsAgentImpl::registerAgent(PassOwnPtrWillBeRawPtr<InspectorAgent> agent)
{
    m_agents.append(agent);
}

void WebDevToolsAgentImpl::attach(const WebString& hostId, int sessionId)
{
    if (m_attached)
        return;

    // Set the attached bit first so that sync notifications were delivered.
    m_attached = true;
    m_sessionId = sessionId;

    initializeDeferredAgents();
    m_resourceAgent->setHostId(hostId);

    m_inspectorFrontend = adoptPtr(new InspectorFrontend(this));
    // We can reconnect to existing front-end -> unmute state.
    m_stateMuted = false;
    m_agents.setFrontend(m_inspectorFrontend.get());

    InspectorInstrumentation::registerInstrumentingAgents(m_instrumentingAgents.get());
    InspectorInstrumentation::frontendCreated();

    m_inspectorBackendDispatcher = InspectorBackendDispatcher::create(this);
    m_agents.registerInDispatcher(m_inspectorBackendDispatcher.get());

    Platform::current()->currentThread()->addTaskObserver(this);
}

void WebDevToolsAgentImpl::reattach(const WebString& hostId, int sessionId, const WebString& savedState)
{
    if (m_attached)
        return;

    attach(hostId, sessionId);
    m_agents.restore(savedState);
}

void WebDevToolsAgentImpl::detach()
{
    if (!m_attached)
        return;

    Platform::current()->currentThread()->removeTaskObserver(this);

    m_inspectorBackendDispatcher->clearFrontend();
    m_inspectorBackendDispatcher.clear();

    // Destroying agents would change the state, but we don't want that.
    // Pre-disconnect state will be used to restore inspector agents.
    m_stateMuted = true;
    m_agents.clearFrontend();
    m_inspectorFrontend.clear();

    // Release overlay resources.
    if (m_overlay)
        m_overlay->clear();
    InspectorInstrumentation::frontendDeleted();
    InspectorInstrumentation::unregisterInstrumentingAgents(m_instrumentingAgents.get());

    m_sessionId = 0;
    m_attached = false;
}

void WebDevToolsAgentImpl::continueProgram()
{
    ClientMessageLoopAdapter::continueProgram();
}

void WebDevToolsAgentImpl::didCommitLoadForLocalFrame(LocalFrame* frame)
{
    m_resourceContentLoader->didCommitLoadForLocalFrame(frame);
    m_agents.didCommitLoadForLocalFrame(frame);
}

bool WebDevToolsAgentImpl::screencastEnabled()
{
    return m_pageAgent && m_pageAgent->screencastEnabled();
}

void WebDevToolsAgentImpl::willAddPageOverlay(const GraphicsLayer* layer)
{
    m_layerTreeAgent->willAddPageOverlay(layer);
}

void WebDevToolsAgentImpl::didRemovePageOverlay(const GraphicsLayer* layer)
{
    m_layerTreeAgent->didRemovePageOverlay(layer);
}

void WebDevToolsAgentImpl::layerTreeViewChanged(WebLayerTreeView* layerTreeView)
{
    m_tracingAgent->setLayerTreeId(layerTreeView ? layerTreeView->layerTreeId() : 0);
}

void WebDevToolsAgentImpl::enableTracing(const String& categoryFilter)
{
    m_client->enableTracing(categoryFilter);
}

void WebDevToolsAgentImpl::disableTracing()
{
    m_client->disableTracing();
}

void WebDevToolsAgentImpl::setCPUThrottlingRate(double rate)
{
    m_client->setCPUThrottlingRate(rate);
}

void WebDevToolsAgentImpl::dispatchOnInspectorBackend(int sessionId, const WebString& message)
{
    if (!m_attached)
        return;
    if (WebDevToolsAgent::shouldInterruptForMessage(message))
        MainThreadDebugger::instance()->taskRunner()->runPendingTasks();
    else
        dispatchMessageFromFrontend(sessionId, message);
}

void WebDevToolsAgentImpl::dispatchMessageFromFrontend(int sessionId, const String& message)
{
    InspectorTaskRunner::IgnoreInterruptsScope scope(MainThreadDebugger::instance()->taskRunner());
    if (m_inspectorBackendDispatcher)
        m_inspectorBackendDispatcher->dispatch(sessionId, message);
}

void WebDevToolsAgentImpl::inspectElementAt(const WebPoint& pointInRootFrame)
{
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::Move | HitTestRequest::ReadOnly | HitTestRequest::AllowChildFrameContent;
    HitTestRequest request(hitType);
    WebMouseEvent dummyEvent;
    dummyEvent.type = WebInputEvent::MouseDown;
    dummyEvent.x = pointInRootFrame.x;
    dummyEvent.y = pointInRootFrame.y;
    IntPoint transformedPoint = PlatformMouseEventBuilder(m_webLocalFrameImpl->frameView(), dummyEvent).position();
    HitTestResult result(request, m_webLocalFrameImpl->frameView()->rootFrameToContents(transformedPoint));
    m_webLocalFrameImpl->frame()->contentLayoutObject()->hitTest(result);
    Node* node = result.innerNode();
    if (!node && m_webLocalFrameImpl->frame()->document())
        node = m_webLocalFrameImpl->frame()->document()->documentElement();
    m_domAgent->inspect(node);
}

void WebDevToolsAgentImpl::failedToRequestDevTools()
{
    ClientMessageLoopAdapter::resumeForCreateWindow();
}

void WebDevToolsAgentImpl::sendProtocolResponse(int sessionId, int callId, PassRefPtr<JSONObject> message)
{
    if (!m_attached)
        return;
    flushPendingProtocolNotifications();
    String stateToSend;
    if (!m_stateMuted) {
        stateToSend = m_agents.state();
        if (stateToSend == m_stateCookie)
            stateToSend = String();
        else
            m_stateCookie = stateToSend;
    }
    m_client->sendProtocolMessage(sessionId, callId, message->toJSONString(), stateToSend);
}

void WebDevToolsAgentImpl::sendProtocolNotification(PassRefPtr<JSONObject> message)
{
    if (!m_attached)
        return;
    m_notificationQueue.append(std::make_pair(m_sessionId, message));
}

void WebDevToolsAgentImpl::flush()
{
    flushPendingProtocolNotifications();
}

void WebDevToolsAgentImpl::resumeStartup()
{
    // If we've paused for createWindow, handle it ourselves.
    if (ClientMessageLoopAdapter::resumeForCreateWindow())
        return;
    // Otherwise, pass to the client (embedded workers do it differently).
    m_client->resumeStartup();
}

void WebDevToolsAgentImpl::pageLayoutInvalidated()
{
    if (m_overlay)
        m_overlay->pageLayoutInvalidated();
}

void WebDevToolsAgentImpl::setPausedInDebuggerMessage(const String* message)
{
    if (m_overlay)
        m_overlay->setPausedInDebuggerMessage(message);
}

void WebDevToolsAgentImpl::waitForCreateWindow(LocalFrame* frame)
{
    if (!m_attached)
        return;
    if (m_client->requestDevToolsForFrame(WebLocalFrameImpl::fromFrame(frame)))
        ClientMessageLoopAdapter::pauseForCreateWindow(m_webLocalFrameImpl);
}

void WebDevToolsAgentImpl::evaluateInWebInspector(long callId, const WebString& script)
{
    m_inspectorAgent->evaluateForTestInFrontend(callId, script);
}

WebString WebDevToolsAgentImpl::evaluateInWebInspectorOverlay(const WebString& script)
{
    if (!m_overlay)
        return WebString();

    return m_overlay->evaluateInOverlayForTest(script);
}

void WebDevToolsAgentImpl::flushPendingProtocolNotifications()
{
    if (m_attached) {
        m_agents.flushPendingProtocolNotifications();
        for (size_t i = 0; i < m_notificationQueue.size(); ++i)
            m_client->sendProtocolMessage(m_notificationQueue[i].first, 0, m_notificationQueue[i].second->toJSONString(), WebString());
    }
    m_notificationQueue.clear();
}

void WebDevToolsAgentImpl::willProcessTask()
{
    if (!m_attached)
        return;
    if (InspectorProfilerAgent* profilerAgent = m_instrumentingAgents->inspectorProfilerAgent())
        profilerAgent->willProcessTask();
}

void WebDevToolsAgentImpl::didProcessTask()
{
    if (!m_attached)
        return;
    if (InspectorProfilerAgent* profilerAgent = m_instrumentingAgents->inspectorProfilerAgent())
        profilerAgent->didProcessTask();
    flushPendingProtocolNotifications();
}

void WebDevToolsAgent::interruptAndDispatch(int sessionId, MessageDescriptor* rawDescriptor)
{
    // rawDescriptor can't be a PassOwnPtr because interruptAndDispatch is a WebKit API function.
    OwnPtr<MessageDescriptor> descriptor = adoptPtr(rawDescriptor);
    OwnPtr<DebuggerTask> task = adoptPtr(new DebuggerTask(sessionId, descriptor.release()));
    MainThreadDebugger::interruptMainThreadAndRun(task.release());
}

bool WebDevToolsAgent::shouldInterruptForMessage(const WebString& message)
{
    String commandName;
    if (!InspectorBackendDispatcher::getCommandName(message, &commandName))
        return false;
    return commandName == InspectorBackendDispatcher::commandName(InspectorBackendDispatcher::kDebugger_pauseCmd)
        || commandName == InspectorBackendDispatcher::commandName(InspectorBackendDispatcher::kDebugger_setBreakpointCmd)
        || commandName == InspectorBackendDispatcher::commandName(InspectorBackendDispatcher::kDebugger_setBreakpointByUrlCmd)
        || commandName == InspectorBackendDispatcher::commandName(InspectorBackendDispatcher::kDebugger_removeBreakpointCmd)
        || commandName == InspectorBackendDispatcher::commandName(InspectorBackendDispatcher::kDebugger_setBreakpointsActiveCmd);
}

} // namespace blink
