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

#include <v8-inspector.h>
#include <memory>

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/CoreProbeSink.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorAnimationAgent.h"
#include "core/inspector/InspectorApplicationCacheAgent.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "core/inspector/InspectorInputAgent.h"
#include "core/inspector/InspectorLayerTreeAgent.h"
#include "core/inspector/InspectorLogAgent.h"
#include "core/inspector/InspectorMemoryAgent.h"
#include "core/inspector/InspectorNetworkAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorResourceContainer.h"
#include "core/inspector/InspectorResourceContentLoader.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/inspector/InspectorTracingAgent.h"
#include "core/inspector/InspectorWorkerAgent.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/probe/CoreProbes.h"
#include "modules/accessibility/InspectorAccessibilityAgent.h"
#include "modules/cachestorage/InspectorCacheStorageAgent.h"
#include "modules/device_orientation/DeviceOrientationInspectorAgent.h"
#include "modules/indexeddb/InspectorIndexedDBAgent.h"
#include "modules/storage/InspectorDOMStorageAgent.h"
#include "modules/webdatabase/InspectorDatabaseAgent.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/WTFString.h"
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

namespace blink {

namespace {

bool IsMainFrame(WebLocalFrameImpl* frame) {
  // TODO(dgozman): sometimes view->mainFrameImpl() does return null, even
  // though |frame| is meant to be main frame.  See http://crbug.com/526162.
  return frame->ViewImpl() && !frame->Parent();
}
}

class ClientMessageLoopAdapter : public MainThreadDebugger::ClientMessageLoop {
 public:
  ~ClientMessageLoopAdapter() override { instance_ = nullptr; }

  static void EnsureMainThreadDebuggerCreated(WebDevToolsAgentClient* client) {
    if (instance_)
      return;
    std::unique_ptr<ClientMessageLoopAdapter> instance =
        WTF::WrapUnique(new ClientMessageLoopAdapter(
            WTF::WrapUnique(client->CreateClientMessageLoop())));
    instance_ = instance.get();
    MainThreadDebugger::Instance()->SetClientMessageLoop(std::move(instance));
  }

  static void ContinueProgram() {
    // Release render thread if necessary.
    if (instance_)
      instance_->QuitNow();
  }

  static void PauseForCreateWindow(WebLocalFrameImpl* frame) {
    if (instance_)
      instance_->RunForCreateWindow(frame);
  }

  static bool ResumeForCreateWindow() {
    return instance_ ? instance_->QuitForCreateWindow() : false;
  }

 private:
  ClientMessageLoopAdapter(
      std::unique_ptr<WebDevToolsAgentClient::WebKitClientMessageLoop>
          message_loop)
      : running_for_debug_break_(false),
        running_for_create_window_(false),
        message_loop_(std::move(message_loop)) {
    DCHECK(message_loop_.get());
  }

  void Run(LocalFrame* frame) override {
    if (running_for_debug_break_)
      return;

    running_for_debug_break_ = true;
    if (!running_for_create_window_)
      RunLoop(WebLocalFrameImpl::FromFrame(frame));
  }

  void RunForCreateWindow(WebLocalFrameImpl* frame) {
    if (running_for_create_window_)
      return;

    running_for_create_window_ = true;
    if (!running_for_debug_break_)
      RunLoop(frame);
  }

  void RunLoop(WebLocalFrameImpl* frame) {
    // 0. Flush pending frontend messages.
    WebDevToolsAgentImpl* agent = frame->DevToolsAgentImpl();
    agent->FlushProtocolNotifications();

    // 1. Disable input events.
    WebFrameWidgetBase::SetIgnoreInputEvents(true);
    for (const auto view : WebViewImpl::AllInstances())
      view->ChromeClient().NotifyPopupOpeningObservers();

    // 2. Notify embedder about pausing.
    if (agent->Client())
      agent->Client()->WillEnterDebugLoop();

    // 3. Disable active objects
    WebView::WillEnterModalLoop();

    // 4. Process messages until quitNow is called.
    message_loop_->Run();

    // 5. Resume active objects
    WebView::DidExitModalLoop();

    WebFrameWidgetBase::SetIgnoreInputEvents(false);

    // 7. Notify embedder about resuming.
    if (agent->Client())
      agent->Client()->DidExitDebugLoop();
  }

  void QuitNow() override {
    if (running_for_debug_break_) {
      running_for_debug_break_ = false;
      if (!running_for_create_window_)
        message_loop_->QuitNow();
    }
  }

  bool QuitForCreateWindow() {
    if (running_for_create_window_) {
      running_for_create_window_ = false;
      if (!running_for_debug_break_)
        message_loop_->QuitNow();
      return true;
    }
    return false;
  }

  void RunIfWaitingForDebugger(LocalFrame* frame) override {
    // If we've paused for createWindow, handle it ourselves.
    if (QuitForCreateWindow())
      return;
    // Otherwise, pass to the client (embedded workers do it differently).
    WebDevToolsAgentImpl* agent =
        WebLocalFrameImpl::FromFrame(frame)->DevToolsAgentImpl();
    if (agent && agent->Client())
      agent->Client()->ResumeStartup();
  }

  bool running_for_debug_break_;
  bool running_for_create_window_;
  std::unique_ptr<WebDevToolsAgentClient::WebKitClientMessageLoop>
      message_loop_;

  static ClientMessageLoopAdapter* instance_;
};

ClientMessageLoopAdapter* ClientMessageLoopAdapter::instance_ = nullptr;

// static
WebDevToolsAgentImpl* WebDevToolsAgentImpl::Create(
    WebLocalFrameImpl* frame,
    WebDevToolsAgentClient* client) {
  InspectorOverlay* overlay = new InspectorOverlay(frame);

  if (!IsMainFrame(frame)) {
    WebDevToolsAgentImpl* agent =
        new WebDevToolsAgentImpl(frame, client, overlay, false);
    if (frame->FrameWidget())
      agent->LayerTreeViewChanged(
          ToWebFrameWidgetImpl(frame->FrameWidget())->LayerTreeView());
    return agent;
  }

  WebViewImpl* view = frame->ViewImpl();
  WebDevToolsAgentImpl* agent =
      new WebDevToolsAgentImpl(frame, client, overlay, true);
  agent->LayerTreeViewChanged(view->LayerTreeView());
  return agent;
}

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebLocalFrameImpl* web_local_frame_impl,
    WebDevToolsAgentClient* client,
    InspectorOverlay* overlay,
    bool include_view_agents)
    : client_(client),
      web_local_frame_impl_(web_local_frame_impl),
      instrumenting_agents_(
          web_local_frame_impl_->GetFrame()->InstrumentingAgents()),
      resource_content_loader_(InspectorResourceContentLoader::Create(
          web_local_frame_impl_->GetFrame())),
      overlay_(overlay),
      inspected_frames_(
          InspectedFrames::Create(web_local_frame_impl_->GetFrame())),
      resource_container_(new InspectorResourceContainer(inspected_frames_)),
      dom_agent_(nullptr),
      page_agent_(nullptr),
      network_agent_(nullptr),
      layer_tree_agent_(nullptr),
      tracing_agent_(nullptr),
      trace_events_agent_(new InspectorTraceEvents()),
      include_view_agents_(include_view_agents),
      layer_tree_id_(0) {
  DCHECK(IsMainThread());
  DCHECK(web_local_frame_impl_->GetFrame());
  instrumenting_agents_->addInspectorTraceEvents(trace_events_agent_);
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl() {
  DCHECK(!client_);
}

DEFINE_TRACE(WebDevToolsAgentImpl) {
  visitor->Trace(web_local_frame_impl_);
  visitor->Trace(instrumenting_agents_);
  visitor->Trace(resource_content_loader_);
  visitor->Trace(overlay_);
  visitor->Trace(inspected_frames_);
  visitor->Trace(resource_container_);
  visitor->Trace(dom_agent_);
  visitor->Trace(page_agent_);
  visitor->Trace(network_agent_);
  visitor->Trace(layer_tree_agent_);
  visitor->Trace(tracing_agent_);
  visitor->Trace(trace_events_agent_);
  visitor->Trace(session_);
}

void WebDevToolsAgentImpl::WillBeDestroyed() {
  DCHECK(web_local_frame_impl_->GetFrame());
  DCHECK(inspected_frames_->Root()->View());
  instrumenting_agents_->removeInspectorTraceEvents(trace_events_agent_);
  trace_events_agent_ = nullptr;
  Detach();
  resource_content_loader_->Dispose();
  client_ = nullptr;
}

void WebDevToolsAgentImpl::InitializeSession(int session_id,
                                             const String& host_id,
                                             String* state) {
  DCHECK(client_);
  ClientMessageLoopAdapter::EnsureMainThreadDebuggerCreated(client_);
  MainThreadDebugger* main_thread_debugger = MainThreadDebugger::Instance();
  v8::Isolate* isolate = V8PerIsolateData::MainThreadIsolate();

  session_ = new InspectorSession(
      this, instrumenting_agents_.Get(), session_id,
      main_thread_debugger->GetV8Inspector(),
      main_thread_debugger->ContextGroupId(inspected_frames_->Root()), state);

  InspectorDOMAgent* dom_agent = new InspectorDOMAgent(
      isolate, inspected_frames_.Get(), session_->V8Session(), overlay_.Get());
  dom_agent_ = dom_agent;
  session_->Append(dom_agent);

  InspectorLayerTreeAgent* layer_tree_agent =
      InspectorLayerTreeAgent::Create(inspected_frames_.Get());
  layer_tree_agent_ = layer_tree_agent;
  session_->Append(layer_tree_agent);

  InspectorNetworkAgent* network_agent =
      InspectorNetworkAgent::Create(inspected_frames_.Get());
  network_agent_ = network_agent;
  session_->Append(network_agent);

  InspectorCSSAgent* css_agent = InspectorCSSAgent::Create(
      dom_agent_, inspected_frames_.Get(), network_agent_,
      resource_content_loader_.Get(), resource_container_.Get());
  session_->Append(css_agent);

  session_->Append(new InspectorAnimationAgent(
      inspected_frames_.Get(), css_agent, session_->V8Session()));

  session_->Append(InspectorMemoryAgent::Create());

  session_->Append(
      InspectorApplicationCacheAgent::Create(inspected_frames_.Get()));

  session_->Append(new InspectorIndexedDBAgent(inspected_frames_.Get(),
                                               session_->V8Session()));

  InspectorWorkerAgent* worker_agent =
      new InspectorWorkerAgent(inspected_frames_.Get());
  session_->Append(worker_agent);

  InspectorTracingAgent* tracing_agent = InspectorTracingAgent::Create(
      this, worker_agent, inspected_frames_.Get());
  tracing_agent_ = tracing_agent;
  session_->Append(tracing_agent);

  session_->Append(new InspectorDOMDebuggerAgent(isolate, dom_agent_,
                                                 session_->V8Session()));

  session_->Append(InspectorInputAgent::Create(inspected_frames_.Get()));

  InspectorPageAgent* page_agent = InspectorPageAgent::Create(
      inspected_frames_.Get(), this, resource_content_loader_.Get(),
      session_->V8Session());
  page_agent_ = page_agent;
  session_->Append(page_agent);

  session_->Append(new InspectorLogAgent(
      &inspected_frames_->Root()->GetPage()->GetConsoleMessageStorage(),
      inspected_frames_->Root()->GetPerformanceMonitor()));

  session_->Append(
      new DeviceOrientationInspectorAgent(inspected_frames_.Get()));

  tracing_agent_->SetLayerTreeId(layer_tree_id_);
  network_agent_->SetHostId(host_id);

  if (include_view_agents_) {
    // TODO(dgozman): we should actually pass the view instead of frame, but
    // during remote->local transition we cannot access mainFrameImpl() yet, so
    // we have to store the frame which will become the main frame later.
    session_->Append(
        InspectorRenderingAgent::Create(web_local_frame_impl_, overlay_.Get()));
    session_->Append(
        InspectorEmulationAgent::Create(web_local_frame_impl_, this));
    // TODO(dgozman): migrate each of the following agents to frame once module
    // is ready.
    Page* page = web_local_frame_impl_->ViewImpl()->GetPage();
    session_->Append(InspectorDatabaseAgent::Create(page));
    session_->Append(new InspectorAccessibilityAgent(page, dom_agent_));
    session_->Append(InspectorDOMStorageAgent::Create(page));
    session_->Append(InspectorCacheStorageAgent::Create());
  }

  if (overlay_)
    overlay_->Init(session_->V8Session(), dom_agent_);

  Platform::Current()->CurrentThread()->AddTaskObserver(this);
}

void WebDevToolsAgentImpl::DestroySession() {
  if (overlay_)
    overlay_->Clear();

  tracing_agent_.Clear();
  layer_tree_agent_.Clear();
  network_agent_.Clear();
  page_agent_.Clear();
  dom_agent_.Clear();

  session_->Dispose();
  session_.Clear();

  Platform::Current()->CurrentThread()->RemoveTaskObserver(this);
}

void WebDevToolsAgentImpl::Attach(const WebString& host_id, int session_id) {
  if (Attached())
    return;
  InitializeSession(session_id, host_id, nullptr);
}

void WebDevToolsAgentImpl::Reattach(const WebString& host_id,
                                    int session_id,
                                    const WebString& saved_state) {
  if (Attached())
    return;
  String state = saved_state;
  InitializeSession(session_id, host_id, &state);
  session_->Restore();
}

void WebDevToolsAgentImpl::Detach() {
  if (!Attached())
    return;
  DestroySession();
}

void WebDevToolsAgentImpl::ContinueProgram() {
  ClientMessageLoopAdapter::ContinueProgram();
}

void WebDevToolsAgentImpl::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  resource_container_->DidCommitLoadForLocalFrame(frame);
  resource_content_loader_->DidCommitLoadForLocalFrame(frame);
  if (session_)
    session_->DidCommitLoadForLocalFrame(frame);
}

void WebDevToolsAgentImpl::DidStartProvisionalLoad(LocalFrame* frame) {
  if (session_ && inspected_frames_->Root() == frame)
    session_->V8Session()->resume();
}

bool WebDevToolsAgentImpl::ScreencastEnabled() {
  return page_agent_ && page_agent_->ScreencastEnabled();
}

void WebDevToolsAgentImpl::WillAddPageOverlay(const GraphicsLayer* layer) {
  if (layer_tree_agent_)
    layer_tree_agent_->WillAddPageOverlay(layer);
}

void WebDevToolsAgentImpl::DidRemovePageOverlay(const GraphicsLayer* layer) {
  if (layer_tree_agent_)
    layer_tree_agent_->DidRemovePageOverlay(layer);
}

void WebDevToolsAgentImpl::RootLayerCleared() {
  if (tracing_agent_)
    tracing_agent_->RootLayerCleared();
}

void WebDevToolsAgentImpl::LayerTreeViewChanged(
    WebLayerTreeView* layer_tree_view) {
  layer_tree_id_ = layer_tree_view ? layer_tree_view->LayerTreeId() : 0;
  if (tracing_agent_)
    tracing_agent_->SetLayerTreeId(layer_tree_id_);
}

void WebDevToolsAgentImpl::EnableTracing(const String& category_filter) {
  if (client_)
    client_->EnableTracing(category_filter);
}

void WebDevToolsAgentImpl::DisableTracing() {
  if (client_)
    client_->DisableTracing();
}

void WebDevToolsAgentImpl::ShowReloadingBlanket() {
  if (overlay_)
    overlay_->ShowReloadingBlanket();
}

void WebDevToolsAgentImpl::HideReloadingBlanket() {
  if (overlay_)
    overlay_->HideReloadingBlanket();
}

void WebDevToolsAgentImpl::SetCPUThrottlingRate(double rate) {
  if (client_)
    client_->SetCPUThrottlingRate(rate);
}

void WebDevToolsAgentImpl::DispatchOnInspectorBackend(
    int session_id,
    int call_id,
    const WebString& method,
    const WebString& message) {
  if (!Attached())
    return;
  if (WebDevToolsAgent::ShouldInterruptForMethod(method))
    MainThreadDebugger::Instance()->TaskRunner()->RunAllTasksDontWait();
  else
    DispatchMessageFromFrontend(session_id, method, message);
}

void WebDevToolsAgentImpl::DispatchMessageFromFrontend(int session_id,
                                                       const String& method,
                                                       const String& message) {
  if (!Attached() || session_id != session_->SessionId())
    return;
  InspectorTaskRunner::IgnoreInterruptsScope scope(
      MainThreadDebugger::Instance()->TaskRunner());
  session_->DispatchProtocolMessage(method, message);
}

void WebDevToolsAgentImpl::InspectElementAt(
    int session_id,
    const WebPoint& point_in_root_frame) {
  if (!dom_agent_ || !session_ || session_->SessionId() != session_id)
    return;
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kMove | HitTestRequest::kReadOnly |
      HitTestRequest::kAllowChildFrameContent;
  HitTestRequest request(hit_type);
  WebMouseEvent dummy_event(WebInputEvent::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WTF::MonotonicallyIncreasingTimeMS());
  dummy_event.SetPositionInWidget(point_in_root_frame.x, point_in_root_frame.y);
  IntPoint transformed_point = FlooredIntPoint(
      TransformWebMouseEvent(web_local_frame_impl_->GetFrameView(), dummy_event)
          .PositionInRootFrame());
  HitTestResult result(
      request, web_local_frame_impl_->GetFrameView()->RootFrameToContents(
                   transformed_point));
  web_local_frame_impl_->GetFrame()->ContentLayoutItem().HitTest(result);
  Node* node = result.InnerNode();
  if (!node && web_local_frame_impl_->GetFrame()->GetDocument())
    node = web_local_frame_impl_->GetFrame()->GetDocument()->documentElement();
  dom_agent_->Inspect(node);
}

void WebDevToolsAgentImpl::FailedToRequestDevTools() {
  ClientMessageLoopAdapter::ResumeForCreateWindow();
}

void WebDevToolsAgentImpl::SendProtocolMessage(int session_id,
                                               int call_id,
                                               const String& response,
                                               const String& state) {
  ASSERT(Attached());
  if (client_)
    client_->SendProtocolMessage(session_id, call_id, response, state);
}

void WebDevToolsAgentImpl::PageLayoutInvalidated(bool resized) {
  if (overlay_)
    overlay_->PageLayoutInvalidated(resized);
}

void WebDevToolsAgentImpl::ConfigureOverlay(bool suspended,
                                            const String& message) {
  if (!overlay_)
    return;
  overlay_->SetPausedInDebuggerMessage(message);
  if (suspended)
    overlay_->Suspend();
  else
    overlay_->Resume();
}

void WebDevToolsAgentImpl::WaitForCreateWindow(LocalFrame* frame) {
  if (!Attached())
    return;
  if (client_ &&
      client_->RequestDevToolsForFrame(WebLocalFrameImpl::FromFrame(frame)))
    ClientMessageLoopAdapter::PauseForCreateWindow(web_local_frame_impl_);
}

WebString WebDevToolsAgentImpl::EvaluateInWebInspectorOverlay(
    const WebString& script) {
  if (!overlay_)
    return WebString();

  return overlay_->EvaluateInOverlayForTest(script);
}

bool WebDevToolsAgentImpl::CacheDisabled() {
  if (!network_agent_)
    return false;
  return network_agent_->CacheDisabled();
}

void WebDevToolsAgentImpl::FlushProtocolNotifications() {
  if (session_)
    session_->flushProtocolNotifications();
}

void WebDevToolsAgentImpl::WillProcessTask() {
  if (!Attached())
    return;
  ThreadDebugger::IdleFinished(V8PerIsolateData::MainThreadIsolate());
}

void WebDevToolsAgentImpl::DidProcessTask() {
  if (!Attached())
    return;
  ThreadDebugger::IdleStarted(V8PerIsolateData::MainThreadIsolate());
  FlushProtocolNotifications();
}

void WebDevToolsAgentImpl::RunDebuggerTask(
    int session_id,
    std::unique_ptr<WebDevToolsAgent::MessageDescriptor> descriptor) {
  WebDevToolsAgent* webagent = descriptor->Agent();
  if (!webagent)
    return;

  WebDevToolsAgentImpl* agent_impl =
      static_cast<WebDevToolsAgentImpl*>(webagent);
  if (agent_impl->Attached())
    agent_impl->DispatchMessageFromFrontend(session_id, descriptor->Method(),
                                            descriptor->Message());
}

void WebDevToolsAgent::InterruptAndDispatch(int session_id,
                                            MessageDescriptor* raw_descriptor) {
  // rawDescriptor can't be a std::unique_ptr because interruptAndDispatch is a
  // WebKit API function.
  MainThreadDebugger::InterruptMainThreadAndRun(
      CrossThreadBind(WebDevToolsAgentImpl::RunDebuggerTask, session_id,
                      WTF::Passed(WTF::WrapUnique(raw_descriptor))));
}

bool WebDevToolsAgent::ShouldInterruptForMethod(const WebString& method) {
  return method == "Debugger.pause" || method == "Debugger.setBreakpoint" ||
         method == "Debugger.setBreakpointByUrl" ||
         method == "Debugger.removeBreakpoint" ||
         method == "Debugger.setBreakpointsActive";
}

}  // namespace blink
