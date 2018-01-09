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
#include <utility>

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreInitializer.h"
#include "core/CoreProbeSink.h"
#include "core/events/WebInputEventConversion.h"
#include "core/exported/WebSettingsImpl.h"
#include "core/exported/WebViewImpl.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/inspector/DevToolsEmulator.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorAnimationAgent.h"
#include "core/inspector/InspectorApplicationCacheAgent.h"
#include "core/inspector/InspectorAuditsAgent.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "core/inspector/InspectorDOMSnapshotAgent.h"
#include "core/inspector/InspectorEmulationAgent.h"
#include "core/inspector/InspectorIOAgent.h"
#include "core/inspector/InspectorLayerTreeAgent.h"
#include "core/inspector/InspectorLogAgent.h"
#include "core/inspector/InspectorMemoryAgent.h"
#include "core/inspector/InspectorNetworkAgent.h"
#include "core/inspector/InspectorOverlayAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorPerformanceAgent.h"
#include "core/inspector/InspectorResourceContainer.h"
#include "core/inspector/InspectorResourceContentLoader.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/inspector/InspectorTracingAgent.h"
#include "core/inspector/InspectorWorkerAgent.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/layout/LayoutView.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/probe/CoreProbes.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/LayoutTestSupport.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/InterfaceRegistry.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebString.h"
#include "public/web/WebSettings.h"
#include "public/web/WebViewClient.h"

namespace blink {

namespace {

bool IsMainFrame(WebLocalFrameImpl* frame) {
  // TODO(dgozman): sometimes view->mainFrameImpl() does return null, even
  // though |frame| is meant to be main frame.  See http://crbug.com/526162.
  return frame->ViewImpl() && !frame->Parent();
}

// TODO(dgozman): somehow get this from a mojo config.
// See kMaximumMojoMessageSize in services/service_manager/embedder/main.cc.
const size_t kMaxDevToolsMessageChunkSize = 128 * 1024 * 1024 / 8;

bool ShouldInterruptForMethod(const String& method) {
  // Keep in sync with DevToolsSession::ShouldSendOnIO.
  // TODO(dgozman): find a way to share this.
  return method == "Debugger.pause" || method == "Debugger.setBreakpoint" ||
         method == "Debugger.setBreakpointByUrl" ||
         method == "Debugger.removeBreakpoint" ||
         method == "Debugger.setBreakpointsActive" ||
         method == "Performance.getMetrics";
}

}  // namespace

class ClientMessageLoopAdapter : public MainThreadDebugger::ClientMessageLoop {
 public:
  ~ClientMessageLoopAdapter() override { instance_ = nullptr; }

  static void EnsureMainThreadDebuggerCreated() {
    if (instance_)
      return;
    std::unique_ptr<ClientMessageLoopAdapter> instance(
        new ClientMessageLoopAdapter(
            Platform::Current()->CreateNestedMessageLoopRunner()));
    instance_ = instance.get();
    MainThreadDebugger::Instance()->SetClientMessageLoop(std::move(instance));
  }

  static void ContinueProgram() {
    // Release render thread if necessary.
    if (instance_)
      instance_->QuitNow();
  }

 private:
  ClientMessageLoopAdapter(
      std::unique_ptr<Platform::NestedMessageLoopRunner> message_loop)
      : running_for_debug_break_(false),
        message_loop_(std::move(message_loop)) {
    DCHECK(message_loop_.get());
  }

  void Run(LocalFrame* frame) override {
    if (running_for_debug_break_)
      return;

    running_for_debug_break_ = true;
    RunLoop(WebLocalFrameImpl::FromFrame(frame));
  }

  void RunLoop(WebLocalFrameImpl* frame) {
    // 0. Flush pending frontend messages.
    WebDevToolsAgentImpl* agent = frame->DevToolsAgentImpl();
    agent->FlushProtocolNotifications();

    // 1. Disable input events.
    WebFrameWidgetBase::SetIgnoreInputEvents(true);
    for (const auto view : WebViewImpl::AllInstances())
      view->GetChromeClient().NotifyPopupOpeningObservers();

    // 2. Disable active objects
    WebView::WillEnterModalLoop();

    // 3. Process messages until quitNow is called.
    message_loop_->Run();

    // 4. Resume active objects
    WebView::DidExitModalLoop();

    // 5. Enable input events.
    WebFrameWidgetBase::SetIgnoreInputEvents(false);
  }

  void QuitNow() override {
    if (running_for_debug_break_) {
      running_for_debug_break_ = false;
      message_loop_->QuitNow();
    }
  }

  void RunIfWaitingForDebugger(LocalFrame* frame) override {
    WebDevToolsAgentImpl* agent =
        WebLocalFrameImpl::FromFrame(frame)->DevToolsAgentImpl();
    if (agent && agent->GetClient())
      agent->GetClient()->ResumeStartup();
  }

  bool running_for_debug_break_;
  std::unique_ptr<Platform::NestedMessageLoopRunner> message_loop_;

  static ClientMessageLoopAdapter* instance_;
};

ClientMessageLoopAdapter* ClientMessageLoopAdapter::instance_ = nullptr;

// Created and stored in unique_ptr on UI.
// Binds request, receives messages and destroys on IO.
class WebDevToolsAgentImpl::IOSession : public mojom::blink::DevToolsSession {
 public:
  IOSession(int session_id,
            scoped_refptr<base::SingleThreadTaskRunner> session_task_runner,
            scoped_refptr<WebTaskRunner> agent_task_runner,
            CrossThreadWeakPersistent<WebDevToolsAgentImpl> agent,
            mojom::blink::DevToolsSessionRequest request)
      : session_id_(session_id),
        session_task_runner_(session_task_runner),
        agent_task_runner_(agent_task_runner),
        agent_(std::move(agent)),
        binding_(this) {
    session_task_runner->PostTask(
        FROM_HERE, ConvertToBaseCallback(CrossThreadBind(
                       &IOSession::BindInterface, CrossThreadUnretained(this),
                       WTF::Passed(std::move(request)))));
  }

  ~IOSession() override {}

  void BindInterface(mojom::blink::DevToolsSessionRequest request) {
    binding_.Bind(std::move(request));
  }

  void DeleteSoon() { session_task_runner_->DeleteSoon(FROM_HERE, this); }

  // mojom::blink::DevToolsSession implementation.
  void DispatchProtocolMessage(int call_id,
                               const String& method,
                               const String& message) override {
    DCHECK(ShouldInterruptForMethod(method));
    MainThreadDebugger::InterruptMainThreadAndRun(
        CrossThreadBind(&WebDevToolsAgentImpl::DispatchMessageFromFrontend,
                        agent_, session_id_, method, message));
    PostCrossThreadTask(
        *agent_task_runner_, FROM_HERE,
        CrossThreadBind(&WebDevToolsAgentImpl::DispatchOnInspectorBackend,
                        agent_, session_id_, call_id, method, message));
  }

  void InspectElement(const WebPoint&) override { NOTREACHED(); }

 private:
  int session_id_;
  scoped_refptr<base::SingleThreadTaskRunner> session_task_runner_;
  scoped_refptr<WebTaskRunner> agent_task_runner_;
  CrossThreadWeakPersistent<WebDevToolsAgentImpl> agent_;
  mojo::Binding<mojom::blink::DevToolsSession> binding_;

  DISALLOW_COPY_AND_ASSIGN(IOSession);
};

class WebDevToolsAgentImpl::Session : public GarbageCollectedFinalized<Session>,
                                      public mojom::blink::DevToolsSession {
 public:
  Session(int session_id,
          WebDevToolsAgentImpl* agent,
          mojom::blink::DevToolsSessionAssociatedRequest request)
      : session_id_(session_id),
        agent_(agent),
        binding_(this, std::move(request)) {}

  ~Session() override {}

  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(agent_); }

  // mojom::blink::DevToolsSession implementation.
  void DispatchProtocolMessage(int call_id,
                               const String& method,
                               const String& message) override {
    DCHECK(!ShouldInterruptForMethod(method));
    agent_->DispatchOnInspectorBackend(session_id_, call_id, method, message);
  }

  void InspectElement(const WebPoint& point) override {
    agent_->InspectElementAt(session_id_, point);
  }

 private:
  int session_id_;
  Member<WebDevToolsAgentImpl> agent_;
  mojo::AssociatedBinding<mojom::blink::DevToolsSession> binding_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

// static
WebDevToolsAgentImpl* WebDevToolsAgentImpl::Create(WebLocalFrameImpl* frame) {
  if (!IsMainFrame(frame)) {
    WebDevToolsAgentImpl* agent = new WebDevToolsAgentImpl(frame, false);
    if (frame->FrameWidget())
      agent->LayerTreeViewChanged(frame->FrameWidget()->GetLayerTreeView());
    return agent;
  }

  WebViewImpl* view = frame->ViewImpl();
  WebDevToolsAgentImpl* agent = new WebDevToolsAgentImpl(frame, true);
  agent->LayerTreeViewChanged(view->LayerTreeView());
  return agent;
}

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebLocalFrameImpl* web_local_frame_impl,
    bool include_view_agents)
    : binding_(this),
      client_(nullptr),
      web_local_frame_impl_(web_local_frame_impl),
      probe_sink_(web_local_frame_impl_->GetFrame()->GetProbeSink()),
      resource_content_loader_(InspectorResourceContentLoader::Create(
          web_local_frame_impl_->GetFrame())),
      inspected_frames_(new InspectedFrames(
          web_local_frame_impl_->GetFrame(),
          web_local_frame_impl_->GetFrame()->GetInstrumentationToken())),
      resource_container_(new InspectorResourceContainer(inspected_frames_)),
      include_view_agents_(include_view_agents),
      layer_tree_id_(0) {
  DCHECK(IsMainThread());
  DCHECK(web_local_frame_impl_->GetFrame());
  web_local_frame_impl_->GetFrame()
      ->GetInterfaceRegistry()
      ->AddAssociatedInterface(WTF::BindRepeating(
          &WebDevToolsAgentImpl::BindRequest, WrapWeakPersistent(this)));
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl() {
  DCHECK(!client_);
}

void WebDevToolsAgentImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(main_sessions_);
  visitor->Trace(web_local_frame_impl_);
  visitor->Trace(probe_sink_);
  visitor->Trace(resource_content_loader_);
  visitor->Trace(inspected_frames_);
  visitor->Trace(resource_container_);
  visitor->Trace(page_agents_);
  visitor->Trace(network_agents_);
  visitor->Trace(tracing_agents_);
  visitor->Trace(overlay_agents_);
  visitor->Trace(sessions_);
}

void WebDevToolsAgentImpl::WillBeDestroyed() {
  DCHECK(web_local_frame_impl_->GetFrame());
  DCHECK(inspected_frames_->Root()->View());

  Vector<int> session_ids;
  for (int session_id : sessions_.Keys())
    session_ids.push_back(session_id);
  for (int session_id : session_ids)
    Detach(session_id);

  resource_content_loader_->Dispose();
  client_ = nullptr;
  binding_.Close();
}

void WebDevToolsAgentImpl::BindRequest(
    mojom::blink::DevToolsAgentAssociatedRequest request) {
  binding_.Bind(std::move(request));
}

void WebDevToolsAgentImpl::AttachDevToolsSession(
    mojom::blink::DevToolsSessionHostAssociatedPtrInfo host,
    mojom::blink::DevToolsSessionAssociatedRequest session,
    mojom::blink::DevToolsSessionRequest io_session,
    const String& reattach_state) {
  int session_id = ++last_session_id_;

  if (!reattach_state.IsNull()) {
    Reattach(session_id, reattach_state);
  } else {
    Attach(session_id);
  }

  main_sessions_.insert(session_id,
                        new Session(session_id, this, std::move(session)));
  io_sessions_.insert(
      session_id,
      new IOSession(session_id, Platform::Current()->GetIOTaskRunner(),
                    web_local_frame_impl_->GetFrame()->GetTaskRunner(
                        TaskType::kUnthrottled),
                    WrapCrossThreadWeakPersistent(this),
                    std::move(io_session)));

  mojom::blink::DevToolsSessionHostAssociatedPtr host_ptr;
  host_ptr.Bind(std::move(host));
  host_ptr.set_connection_error_handler(
      WTF::Bind(&WebDevToolsAgentImpl::DetachSession, WrapWeakPersistent(this),
                session_id));
  hosts_.insert(session_id, std::move(host_ptr));
}

InspectorSession* WebDevToolsAgentImpl::InitializeSession(int session_id,
                                                          String* state) {
  ClientMessageLoopAdapter::EnsureMainThreadDebuggerCreated();
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

  InspectorNetworkAgent* network_agent = new InspectorNetworkAgent(
      inspected_frames_.Get(), nullptr, session->V8Session());
  network_agents_.Set(session_id, network_agent);
  session->Append(network_agent);

  InspectorCSSAgent* css_agent = InspectorCSSAgent::Create(
      dom_agent, inspected_frames_.Get(), network_agent,
      resource_content_loader_.Get(), resource_container_.Get());
  session->Append(css_agent);

  InspectorDOMDebuggerAgent* dom_debugger_agent =
      new InspectorDOMDebuggerAgent(isolate, dom_agent, session->V8Session());
  session->Append(dom_debugger_agent);

  session->Append(InspectorDOMSnapshotAgent::Create(inspected_frames_.Get(),
                                                    dom_debugger_agent));

  session->Append(new InspectorAnimationAgent(inspected_frames_.Get(),
                                              css_agent, session->V8Session()));

  session->Append(InspectorMemoryAgent::Create(inspected_frames_.Get()));

  session->Append(InspectorPerformanceAgent::Create(inspected_frames_.Get()));

  session->Append(
      InspectorApplicationCacheAgent::Create(inspected_frames_.Get()));

  InspectorWorkerAgent* worker_agent =
      new InspectorWorkerAgent(inspected_frames_.Get());
  session->Append(worker_agent);

  InspectorTracingAgent* tracing_agent = InspectorTracingAgent::Create(
      this, worker_agent, inspected_frames_.Get());
  tracing_agents_.Set(session_id, tracing_agent);
  session->Append(tracing_agent);


  InspectorPageAgent* page_agent = InspectorPageAgent::Create(
      inspected_frames_.Get(), this, resource_content_loader_.Get(),
      session->V8Session());
  page_agents_.Set(session_id, page_agent);
  session->Append(page_agent);

  session->Append(new InspectorLogAgent(
      &inspected_frames_->Root()->GetPage()->GetConsoleMessageStorage(),
      inspected_frames_->Root()->GetPerformanceMonitor(),
      session->V8Session()));

  InspectorOverlayAgent* overlay_agent =
      new InspectorOverlayAgent(web_local_frame_impl_, inspected_frames_.Get(),
                                session->V8Session(), dom_agent);
  overlay_agents_.Set(session_id, overlay_agent);
  session->Append(overlay_agent);

  session->Append(new InspectorIOAgent(isolate, session->V8Session()));

  session->Append(new InspectorAuditsAgent(network_agent));

  tracing_agent->SetLayerTreeId(layer_tree_id_);

  if (include_view_agents_) {
    // TODO(dgozman): we should actually pass the view instead of frame, but
    // during remote->local transition we cannot access mainFrameImpl() yet, so
    // we have to store the frame which will become the main frame later.
    session->Append(new InspectorEmulationAgent(web_local_frame_impl_));
  }

  // Call session init callbacks registered from higher layers
  CoreInitializer::GetInstance().InitInspectorAgentSession(
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

void WebDevToolsAgentImpl::SetClient(WebDevToolsAgentImpl::Client* client) {
  client_ = client;
}

void WebDevToolsAgentImpl::Attach(int session_id) {
  if (!session_id || sessions_.find(session_id) != sessions_.end())
    return;
  InitializeSession(session_id, nullptr);
}

void WebDevToolsAgentImpl::Reattach(int session_id, const String& saved_state) {
  if (!session_id || sessions_.find(session_id) != sessions_.end())
    return;
  String state = saved_state;
  InspectorSession* session = InitializeSession(session_id, &state);
  session->Restore();
}

void WebDevToolsAgentImpl::Detach(int session_id) {
  if (!session_id || sessions_.find(session_id) == sessions_.end())
    return;
  DestroySession(session_id);
}

void WebDevToolsAgentImpl::DetachSession(int session_id) {
  Detach(session_id);
  auto it = io_sessions_.find(session_id);
  if (it != io_sessions_.end()) {
    it->value->DeleteSoon();
    io_sessions_.erase(it);
  }
  main_sessions_.erase(session_id);
  hosts_.erase(session_id);
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

void WebDevToolsAgentImpl::ShowReloadingBlanket() {
  for (auto& it : overlay_agents_)
    it.value->ShowReloadingBlanket();
}

void WebDevToolsAgentImpl::HideReloadingBlanket() {
  for (auto& it : overlay_agents_)
    it.value->HideReloadingBlanket();
}

void WebDevToolsAgentImpl::DispatchOnInspectorBackend(int session_id,
                                                      int call_id,
                                                      const String& method,
                                                      const String& message) {
  if (!Attached())
    return;
  if (ShouldInterruptForMethod(method))
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

  WebPoint point = point_in_root_frame;
  if (web_local_frame_impl_->ViewImpl() &&
      web_local_frame_impl_->ViewImpl()->Client()) {
    WebFloatRect rect(point.x, point.y, 0, 0);
    web_local_frame_impl_->ViewImpl()->Client()->ConvertWindowToViewport(&rect);
    point = WebPoint(rect.x, rect.y);
  }

  auto agent_it = overlay_agents_.find(session_id);
  if (agent_it == overlay_agents_.end())
    return;
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kMove | HitTestRequest::kReadOnly |
      HitTestRequest::kAllowChildFrameContent;
  HitTestRequest request(hit_type);
  WebMouseEvent dummy_event(WebInputEvent::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WTF::CurrentTimeTicksInMilliseconds());
  dummy_event.SetPositionInWidget(point.x, point.y);
  IntPoint transformed_point = FlooredIntPoint(
      TransformWebMouseEvent(web_local_frame_impl_->GetFrameView(), dummy_event)
          .PositionInRootFrame());
  HitTestResult result(
      request, web_local_frame_impl_->GetFrameView()->RootFrameToContents(
                   transformed_point));
  web_local_frame_impl_->GetFrame()->ContentLayoutObject()->HitTest(result);
  Node* node = result.InnerNode();
  if (!node && web_local_frame_impl_->GetFrame()->GetDocument())
    node = web_local_frame_impl_->GetFrame()->GetDocument()->documentElement();
  agent_it->value->Inspect(node);
}

void WebDevToolsAgentImpl::SendProtocolMessage(int session_id,
                                               int call_id,
                                               const String& response,
                                               const String& state) {
  DCHECK(Attached());
  // Make tests more predictable by flushing all sessions before sending
  // protocol response in any of them.
  if (LayoutTestSupport::IsRunningLayoutTest() && call_id)
    FlushProtocolNotifications();

  if (client_) {
    client_->SendProtocolMessage(session_id, call_id, response, state);
    return;
  }

  auto it = hosts_.find(session_id);
  if (it == hosts_.end())
    return;

  bool single_chunk = response.length() < kMaxDevToolsMessageChunkSize;
  for (size_t pos = 0; pos < response.length();
       pos += kMaxDevToolsMessageChunkSize) {
    mojom::blink::DevToolsMessageChunkPtr chunk =
        mojom::blink::DevToolsMessageChunk::New();
    chunk->is_first = pos == 0;
    chunk->message_size = chunk->is_first ? response.length() * 2 : 0;
    chunk->is_last = pos + kMaxDevToolsMessageChunkSize >= response.length();
    chunk->call_id = chunk->is_last ? call_id : 0;
    chunk->post_state =
        chunk->is_last && !state.IsNull() ? state : g_empty_string;
    chunk->data = single_chunk
                      ? response
                      : response.Substring(pos, kMaxDevToolsMessageChunkSize);
    it->value->DispatchProtocolMessage(std::move(chunk));
  }
}

void WebDevToolsAgentImpl::PageLayoutInvalidated(bool resized) {
  for (auto& it : overlay_agents_)
    it.value->PageLayoutInvalidated(resized);
}

bool WebDevToolsAgentImpl::IsInspectorLayer(GraphicsLayer* layer) {
  for (auto& it : overlay_agents_) {
    if (it.value->IsInspectorLayer(layer))
      return true;
  }
  return false;
}

String WebDevToolsAgentImpl::EvaluateInOverlayForTesting(const String& script) {
  String result;
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

void WebDevToolsAgentImpl::DispatchBufferedTouchEvents() {
  for (auto& it : overlay_agents_)
    it.value->DispatchBufferedTouchEvents();
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

}  // namespace blink
