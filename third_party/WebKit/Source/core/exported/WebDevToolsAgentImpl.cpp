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

#include "core/exported/WebDevToolsAgentImpl.h"

#include <v8-inspector.h>
#include <memory>

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreProbeSink.h"
#include "core/events/WebInputEventConversion.h"
#include "core/exported/WebSettingsImpl.h"
#include "core/exported/WebViewBase.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/inspector/DevToolsEmulator.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorAnimationAgent.h"
#include "core/inspector/InspectorApplicationCacheAgent.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "core/inspector/InspectorDOMSnapshotAgent.h"
#include "core/inspector/InspectorEmulationAgent.h"
#include "core/inspector/InspectorInputAgent.h"
#include "core/inspector/InspectorLayerTreeAgent.h"
#include "core/inspector/InspectorLogAgent.h"
#include "core/inspector/InspectorMemoryAgent.h"
#include "core/inspector/InspectorNetworkAgent.h"
#include "core/inspector/InspectorOverlayAgent.h"
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

namespace blink {

namespace {

bool IsMainFrame(WebLocalFrameBase* frame) {
  // TODO(dgozman): sometimes view->mainFrameImpl() does return null, even
  // though |frame| is meant to be main frame.  See http://crbug.com/526162.
  return frame->ViewImpl() && !frame->Parent();
}
}  // namespace

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

  static void PauseForCreateWindow(WebLocalFrameBase* frame) {
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
      RunLoop(WebLocalFrameBase::FromFrame(frame));
  }

  void RunForCreateWindow(WebLocalFrameBase* frame) {
    if (running_for_create_window_)
      return;

    running_for_create_window_ = true;
    if (!running_for_debug_break_)
      RunLoop(frame);
  }

  void RunLoop(WebLocalFrameBase* frame) {
    // 0. Flush pending frontend messages.
    WebDevToolsAgentImpl* agent = frame->DevToolsAgentImpl();
    agent->FlushProtocolNotifications();

    // 1. Disable input events.
    WebFrameWidgetBase::SetIgnoreInputEvents(true);
    for (const auto view : WebViewBase::AllInstances())
      view->GetChromeClient().NotifyPopupOpeningObservers();

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
        WebLocalFrameBase::FromFrame(frame)->DevToolsAgentImpl();
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
    WebLocalFrameBase* frame,
    WebDevToolsAgentClient* client) {
  if (!IsMainFrame(frame)) {
    WebDevToolsAgentImpl* agent =
        new WebDevToolsAgentImpl(frame, client, false);
    if (frame->FrameWidget())
      agent->LayerTreeViewChanged(frame->FrameWidget()->GetLayerTreeView());
    return agent;
  }

  WebViewBase* view = frame->ViewImpl();
  WebDevToolsAgentImpl* agent = new WebDevToolsAgentImpl(frame, client, true);
  agent->LayerTreeViewChanged(view->LayerTreeView());
  return agent;
}

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebLocalFrameBase* web_local_frame_impl,
    WebDevToolsAgentClient* client,
    bool include_view_agents)
    : client_(client),
      web_local_frame_impl_(web_local_frame_impl),
      probe_sink_(web_local_frame_impl_->GetFrame()->GetProbeSink()),
      resource_content_loader_(InspectorResourceContentLoader::Create(
          web_local_frame_impl_->GetFrame())),
      inspected_frames_(
          InspectedFrames::Create(web_local_frame_impl_->GetFrame())),
      resource_container_(new InspectorResourceContainer(inspected_frames_)),
      trace_events_(new InspectorTraceEvents()),
      include_view_agents_(include_view_agents),
      layer_tree_id_(0) {
  DCHECK(IsMainThread());
  DCHECK(web_local_frame_impl_->GetFrame());
  probe_sink_->addInspectorTraceEvents(trace_events_);
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl() {
  DCHECK(!client_);
}

DEFINE_TRACE(WebDevToolsAgentImpl) {
  visitor->Trace(web_local_frame_impl_);
  visitor->Trace(probe_sink_);
  visitor->Trace(resource_content_loader_);
  visitor->Trace(inspected_frames_);
  visitor->Trace(resource_container_);
  visitor->Trace(trace_events_);
  visitor->Trace(page_agents_);
  visitor->Trace(network_agents_);
  visitor->Trace(tracing_agents_);
  visitor->Trace(overlay_agents_);
  visitor->Trace(sessions_);
}

void WebDevToolsAgentImpl::WillBeDestroyed() {
  DCHECK(web_local_frame_impl_->GetFrame());
  DCHECK(inspected_frames_->Root()->View());
  probe_sink_->removeInspectorTraceEvents(trace_events_);
  trace_events_ = nullptr;

  Vector<int> session_ids;
  for (int session_id : sessions_.Keys())
    session_ids.push_back(session_id);
  for (int session_id : session_ids)
    Detach(session_id);

  resource_content_loader_->Dispose();
  client_ = nullptr;
}

InspectorSession* WebDevToolsAgentImpl::InitializeSession(int session_id,
                                                          const String& host_id,
                                                          String* state) {
  DCHECK(client_);
  ClientMessageLoopAdapter::EnsureMainThreadDebuggerCreated(client_);
  MainThreadDebugger* main_thread_debugger = MainThreadDebugger::Instance();
  v8::Isolate* isolate = V8PerIsolateData::MainThreadIsolate();

  InspectorSession* session = new InspectorSession(
      this, probe_sink_.Get(), session_id,
      main_thread_debugger->GetV8Inspector(),
      main_thread_debugger->ContextGroupId(inspected_frames_->Root()), state);

  InspectorDOMAgent* dom_agent = new InspectorDOMAgent(
      isolate, inspected_frames_.Get(), session->V8Session());
  session->Append(dom_agent);

  InspectorLayerTreeAgent* layer_tree_agent =
      InspectorLayerTreeAgent::Create(inspected_frames_.Get(), this);
  session->Append(layer_tree_agent);

  InspectorNetworkAgent* network_agent =
      InspectorNetworkAgent::Create(inspected_frames_.Get());
  network_agents_.Set(session_id, network_agent);
  session->Append(network_agent);

  InspectorCSSAgent* css_agent = InspectorCSSAgent::Create(
      dom_agent, inspected_frames_.Get(), network_agent,
      resource_content_loader_.Get(), resource_container_.Get());
  session->Append(css_agent);

  session->Append(InspectorDOMSnapshotAgent::Create(inspected_frames_.Get()));

  session->Append(new InspectorAnimationAgent(inspected_frames_.Get(),
                                              css_agent, session->V8Session()));

  session->Append(InspectorMemoryAgent::Create());

  session->Append(
      InspectorApplicationCacheAgent::Create(inspected_frames_.Get()));

  InspectorWorkerAgent* worker_agent =
      new InspectorWorkerAgent(inspected_frames_.Get());
  session->Append(worker_agent);

  InspectorTracingAgent* tracing_agent = InspectorTracingAgent::Create(
      this, worker_agent, inspected_frames_.Get());
  tracing_agents_.Set(session_id, tracing_agent);
  session->Append(tracing_agent);

  session->Append(
      new InspectorDOMDebuggerAgent(isolate, dom_agent, session->V8Session()));

  session->Append(InspectorInputAgent::Create(inspected_frames_.Get()));

  InspectorPageAgent* page_agent = InspectorPageAgent::Create(
      inspected_frames_.Get(), this, resource_content_loader_.Get(),
      session->V8Session());
  page_agents_.Set(session_id, page_agent);
  session->Append(page_agent);

  session->Append(new InspectorLogAgent(
      &inspected_frames_->Root()->GetPage()->GetConsoleMessageStorage(),
      inspected_frames_->Root()->GetPerformanceMonitor()));

  InspectorOverlayAgent* overlay_agent =
      new InspectorOverlayAgent(web_local_frame_impl_, inspected_frames_.Get(),
                                session->V8Session(), dom_agent);
  overlay_agents_.Set(session_id, overlay_agent);
  session->Append(overlay_agent);

  tracing_agent->SetLayerTreeId(layer_tree_id_);
  network_agent->SetHostId(host_id);

  if (include_view_agents_) {
    // TODO(dgozman): we should actually pass the view instead of frame, but
    // during remote->local transition we cannot access mainFrameImpl() yet, so
    // we have to store the frame which will become the main frame later.
    session->Append(
        InspectorEmulationAgent::Create(web_local_frame_impl_, this));
  }

  // Call session init callbacks registered from higher layers
  InspectorAgent::CallSessionInitCallbacks(
      session, include_view_agents_, dom_agent, inspected_frames_.Get(),
      web_local_frame_impl_->ViewImpl()->GetPage());

  if (!sessions_.size())
    Platform::Current()->CurrentThread()->AddTaskObserver(this);

  sessions_.Set(session_id, session);
  return session;
}

void WebDevToolsAgentImpl::DestroySession(int session_id) {
  overlay_agents_.erase(session_id);
  tracing_agents_.erase(session_id);
  network_agents_.erase(session_id);
  page_agents_.erase(session_id);

  auto session_it = sessions_.find(session_id);
  DCHECK(session_it != sessions_.end());
  session_it->value->Dispose();
  sessions_.erase(session_it);

  if (!sessions_.size())
    Platform::Current()->CurrentThread()->RemoveTaskObserver(this);
}

void WebDevToolsAgentImpl::Attach(const WebString& host_id, int session_id) {
  if (!session_id || sessions_.find(session_id) != sessions_.end())
    return;
  InitializeSession(session_id, host_id, nullptr);
}

void WebDevToolsAgentImpl::Reattach(const WebString& host_id,
                                    int session_id,
                                    const WebString& saved_state) {
  if (!session_id || sessions_.find(session_id) != sessions_.end())
    return;
  String state = saved_state;
  InspectorSession* session = InitializeSession(session_id, host_id, &state);
  session->Restore();
}

void WebDevToolsAgentImpl::Detach(int session_id) {
  if (!session_id || sessions_.find(session_id) == sessions_.end())
    return;
  DestroySession(session_id);
}

void WebDevToolsAgentImpl::ContinueProgram() {
  ClientMessageLoopAdapter::ContinueProgram();
}

void WebDevToolsAgentImpl::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  resource_container_->DidCommitLoadForLocalFrame(frame);
  resource_content_loader_->DidCommitLoadForLocalFrame(frame);
  for (auto& it : sessions_)
    it.value->DidCommitLoadForLocalFrame(frame);
}

void WebDevToolsAgentImpl::DidStartProvisionalLoad(LocalFrame* frame) {
  if (inspected_frames_->Root() == frame) {
    for (auto& it : sessions_)
      it.value->V8Session()->resume();
  }
}

bool WebDevToolsAgentImpl::ScreencastEnabled() {
  for (auto& it : page_agents_) {
    if (it.value->ScreencastEnabled())
      return true;
  }
  return false;
}

void WebDevToolsAgentImpl::RootLayerCleared() {
  for (auto& it : tracing_agents_)
    it.value->RootLayerCleared();
}

void WebDevToolsAgentImpl::LayerTreeViewChanged(
    WebLayerTreeView* layer_tree_view) {
  layer_tree_id_ = layer_tree_view ? layer_tree_view->LayerTreeId() : 0;
  for (auto& it : tracing_agents_)
    it.value->SetLayerTreeId(layer_tree_id_);
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
  for (auto& it : overlay_agents_)
    it.value->ShowReloadingBlanket();
}

void WebDevToolsAgentImpl::HideReloadingBlanket() {
  for (auto& it : overlay_agents_)
    it.value->HideReloadingBlanket();
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
  if (!session_id)
    return;
  auto session_it = sessions_.find(session_id);
  if (session_it == sessions_.end())
    return;
  InspectorTaskRunner::IgnoreInterruptsScope scope(
      MainThreadDebugger::Instance()->TaskRunner());
  session_it->value->DispatchProtocolMessage(method, message);
}

void WebDevToolsAgentImpl::InspectElementAt(
    int session_id,
    const WebPoint& point_in_root_frame) {
  if (!session_id)
    return;
  auto agent_it = overlay_agents_.find(session_id);
  if (agent_it == overlay_agents_.end())
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
  agent_it->value->Inspect(node);
}

void WebDevToolsAgentImpl::FailedToRequestDevTools() {
  ClientMessageLoopAdapter::ResumeForCreateWindow();
}

void WebDevToolsAgentImpl::SendProtocolMessage(int session_id,
                                               int call_id,
                                               const String& response,
                                               const String& state) {
  DCHECK(Attached());
  if (client_)
    client_->SendProtocolMessage(session_id, call_id, response, state);
}

void WebDevToolsAgentImpl::PageLayoutInvalidated(bool resized) {
  for (auto& it : overlay_agents_)
    it.value->PageLayoutInvalidated(resized);
}

void WebDevToolsAgentImpl::WaitForCreateWindow(LocalFrame* frame) {
  if (!Attached())
    return;
  if (client_ &&
      client_->RequestDevToolsForFrame(WebLocalFrameBase::FromFrame(frame)))
    ClientMessageLoopAdapter::PauseForCreateWindow(web_local_frame_impl_);
}

bool WebDevToolsAgentImpl::IsInspectorLayer(GraphicsLayer* layer) {
  for (auto& it : overlay_agents_) {
    if (it.value->IsInspectorLayer(layer))
      return true;
  }
  return false;
}

WebString WebDevToolsAgentImpl::EvaluateInWebInspectorOverlay(
    const WebString& script) {
  WebString result;
  for (auto& it : overlay_agents_)
    result = it.value->EvaluateInOverlayForTest(script);
  return result;
}

void WebDevToolsAgentImpl::PaintOverlay() {
  for (auto& it : overlay_agents_)
    it.value->PaintOverlay();
}

void WebDevToolsAgentImpl::LayoutOverlay() {
  for (auto& it : overlay_agents_)
    it.value->LayoutOverlay();
}

bool WebDevToolsAgentImpl::HandleInputEvent(const WebInputEvent& event) {
  for (auto& it : overlay_agents_) {
    if (it.value->HandleInputEvent(event))
      return true;
  }
  return false;
}

bool WebDevToolsAgentImpl::CacheDisabled() {
  for (auto& it : network_agents_) {
    if (it.value->CacheDisabled())
      return true;
  }
  return false;
}

void WebDevToolsAgentImpl::FlushProtocolNotifications() {
  for (auto& it : sessions_)
    it.value->flushProtocolNotifications();
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
  if (agent_impl->Attached()) {
    agent_impl->DispatchMessageFromFrontend(session_id, descriptor->Method(),
                                            descriptor->Message());
  }
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
