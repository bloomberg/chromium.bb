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

#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"

#include <v8-inspector.h>
#include <memory>
#include <utility>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_scoped_page_pauser.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/exported/web_settings_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/core/inspector/devtools_agent.h"
#include "third_party/blink/renderer/core/inspector/devtools_session.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_animation_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_application_cache_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_debugger_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_snapshot_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_io_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_layer_tree_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_log_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_memory_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_overlay_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_page_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_performance_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_container.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_content_loader.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

bool IsMainFrame(WebLocalFrameImpl* frame) {
  // TODO(dgozman): sometimes view->mainFrameImpl() does return null, even
  // though |frame| is meant to be main frame.  See http://crbug.com/526162.
  return frame->ViewImpl() && !frame->Parent();
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

  static void PauseForPageWait(WebLocalFrameImpl* frame) {
    if (instance_)
      instance_->RunForPageWait(frame);
  }

 private:
  ClientMessageLoopAdapter(
      std::unique_ptr<Platform::NestedMessageLoopRunner> message_loop)
      : running_for_debug_break_(false),
        running_for_page_wait_(false),
        message_loop_(std::move(message_loop)) {
    DCHECK(message_loop_.get());
  }

  void Run(LocalFrame* frame) override {
    if (running_for_debug_break_)
      return;

    running_for_debug_break_ = true;
    if (!running_for_page_wait_)
      RunLoop(WebLocalFrameImpl::FromFrame(frame));
  }

  void RunForPageWait(WebLocalFrameImpl* frame) {
    if (running_for_page_wait_)
      return;

    running_for_page_wait_ = true;
    if (!running_for_debug_break_)
      RunLoop(frame);
  }

  void RunLoop(WebLocalFrameImpl* frame) {
    // 0. Flush pending frontend messages.
    WebDevToolsAgentImpl* agent = frame->DevToolsAgentImpl();
    agent->FlushProtocolNotifications();

    // 1. Disable input events.
    WebFrameWidgetBase::SetIgnoreInputEvents(true);
    for (auto* const view : WebViewImpl::AllInstances())
      view->GetChromeClient().NotifyPopupOpeningObservers();

    // 2. Disable active objects
    page_pauser_ = WebScopedPagePauser::Create();

    // 3. Process messages until quitNow is called.
    message_loop_->Run();
  }

  void QuitNow() override {
    if (running_for_debug_break_) {
      running_for_debug_break_ = false;
      if (!running_for_page_wait_)
        DoQuit();
    }
  }

  void RunIfWaitingForDebugger(LocalFrame* frame) override {
    if (!running_for_page_wait_)
      return;
    running_for_page_wait_ = false;
    if (!running_for_debug_break_)
      DoQuit();
  }

  void DoQuit() {
    // Undo steps (3), (2) and (1) from above.
    // NOTE: This code used to be above right after the |mesasge_loop_->Run()|
    // code, but it is moved here to support browser-side navigation.
    message_loop_->QuitNow();
    page_pauser_.reset();
    WebFrameWidgetBase::SetIgnoreInputEvents(false);
  }

  bool running_for_debug_break_;
  bool running_for_page_wait_;
  std::unique_ptr<Platform::NestedMessageLoopRunner> message_loop_;
  std::unique_ptr<WebScopedPagePauser> page_pauser_;

  static ClientMessageLoopAdapter* instance_;
};

ClientMessageLoopAdapter* ClientMessageLoopAdapter::instance_ = nullptr;

void WebDevToolsAgentImpl::AttachSession(DevToolsSession* session,
                                         bool restore) {
  if (!network_agents_.size())
    Thread::Current()->AddTaskObserver(this);

  ClientMessageLoopAdapter::EnsureMainThreadDebuggerCreated();
  MainThreadDebugger* main_thread_debugger = MainThreadDebugger::Instance();
  v8::Isolate* isolate = V8PerIsolateData::MainThreadIsolate();
  InspectedFrames* inspected_frames = inspected_frames_.Get();

  int context_group_id =
      main_thread_debugger->ContextGroupId(inspected_frames->Root());
  session->ConnectToV8(main_thread_debugger->GetV8Inspector(),
                       context_group_id);

  InspectorDOMAgent* dom_agent = MakeGarbageCollected<InspectorDOMAgent>(
      isolate, inspected_frames, session->V8Session());
  session->Append(dom_agent);

  auto* layer_tree_agent =
      MakeGarbageCollected<InspectorLayerTreeAgent>(inspected_frames, this);
  session->Append(layer_tree_agent);

  InspectorNetworkAgent* network_agent =
      MakeGarbageCollected<InspectorNetworkAgent>(inspected_frames, nullptr,
                                                  session->V8Session());
  session->Append(network_agent);

  auto* css_agent = MakeGarbageCollected<InspectorCSSAgent>(
      dom_agent, inspected_frames, network_agent,
      resource_content_loader_.Get(), resource_container_.Get());
  session->Append(css_agent);

  InspectorDOMDebuggerAgent* dom_debugger_agent =
      MakeGarbageCollected<InspectorDOMDebuggerAgent>(isolate, dom_agent,
                                                      session->V8Session());
  session->Append(dom_debugger_agent);

  InspectorPerformanceAgent* performance_agent =
      MakeGarbageCollected<InspectorPerformanceAgent>(inspected_frames);
  session->Append(performance_agent);

  session->Append(MakeGarbageCollected<InspectorDOMSnapshotAgent>(
      inspected_frames, dom_debugger_agent));

  session->Append(MakeGarbageCollected<InspectorAnimationAgent>(
      inspected_frames, css_agent, session->V8Session()));

  session->Append(MakeGarbageCollected<InspectorMemoryAgent>(inspected_frames));

  session->Append(
      MakeGarbageCollected<InspectorApplicationCacheAgent>(inspected_frames));

  auto* page_agent = MakeGarbageCollected<InspectorPageAgent>(
      inspected_frames, this, resource_content_loader_.Get(),
      session->V8Session());
  session->Append(page_agent);

  session->Append(MakeGarbageCollected<InspectorLogAgent>(
      &inspected_frames->Root()->GetPage()->GetConsoleMessageStorage(),
      inspected_frames->Root()->GetPerformanceMonitor(), session->V8Session()));

  InspectorOverlayAgent* overlay_agent =
      MakeGarbageCollected<InspectorOverlayAgent>(
          web_local_frame_impl_.Get(), inspected_frames, session->V8Session(),
          dom_agent);
  session->Append(overlay_agent);

  session->Append(
      MakeGarbageCollected<InspectorIOAgent>(isolate, session->V8Session()));

  session->Append(MakeGarbageCollected<InspectorAuditsAgent>(
      network_agent,
      &inspected_frames->Root()->GetPage()->GetInspectorIssueStorage()));

  session->Append(MakeGarbageCollected<InspectorMediaAgent>(inspected_frames));

  // TODO(dgozman): we should actually pass the view instead of frame, but
  // during remote->local transition we cannot access mainFrameImpl() yet, so
  // we have to store the frame which will become the main frame later.
  session->Append(MakeGarbageCollected<InspectorEmulationAgent>(
      web_local_frame_impl_.Get()));

  // Call session init callbacks registered from higher layers.
  CoreInitializer::GetInstance().InitInspectorAgentSession(
      session, include_view_agents_, dom_agent, inspected_frames,
      web_local_frame_impl_->ViewImpl()->GetPage());

  if (node_to_inspect_) {
    overlay_agent->Inspect(node_to_inspect_);
    node_to_inspect_ = nullptr;
  }

  network_agents_.insert(session, network_agent);
  page_agents_.insert(session, page_agent);
  overlay_agents_.insert(session, overlay_agent);
}

// static
WebDevToolsAgentImpl* WebDevToolsAgentImpl::CreateForFrame(
    WebLocalFrameImpl* frame) {
  return MakeGarbageCollected<WebDevToolsAgentImpl>(frame, IsMainFrame(frame));
}

// static
WebDevToolsAgentImpl* WebDevToolsAgentImpl::CreateForWorker(
    WebLocalFrameImpl* frame) {
  return MakeGarbageCollected<WebDevToolsAgentImpl>(frame, true);
}

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebLocalFrameImpl* web_local_frame_impl,
    bool include_view_agents)
    : web_local_frame_impl_(web_local_frame_impl),
      probe_sink_(web_local_frame_impl_->GetFrame()->GetProbeSink()),
      resource_content_loader_(
          MakeGarbageCollected<InspectorResourceContentLoader>(
              web_local_frame_impl_->GetFrame())),
      inspected_frames_(MakeGarbageCollected<InspectedFrames>(
          web_local_frame_impl_->GetFrame())),
      resource_container_(
          MakeGarbageCollected<InspectorResourceContainer>(inspected_frames_)),
      include_view_agents_(include_view_agents) {
  DCHECK(IsMainThread());
  agent_ = MakeGarbageCollected<DevToolsAgent>(
      this, inspected_frames_.Get(), probe_sink_.Get(),
      web_local_frame_impl_->GetFrame()->GetInspectorTaskRunner(),
      Platform::Current()->GetIOTaskRunner());
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl() {}

void WebDevToolsAgentImpl::Trace(Visitor* visitor) {
  visitor->Trace(agent_);
  visitor->Trace(network_agents_);
  visitor->Trace(page_agents_);
  visitor->Trace(overlay_agents_);
  visitor->Trace(web_local_frame_impl_);
  visitor->Trace(probe_sink_);
  visitor->Trace(resource_content_loader_);
  visitor->Trace(inspected_frames_);
  visitor->Trace(resource_container_);
  visitor->Trace(node_to_inspect_);
}

void WebDevToolsAgentImpl::WillBeDestroyed() {
  DCHECK(web_local_frame_impl_->GetFrame());
  DCHECK(inspected_frames_->Root()->View());
  agent_->Dispose();
  resource_content_loader_->Dispose();
}

void WebDevToolsAgentImpl::BindReceiver(
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsAgentHost> host_remote,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsAgent> receiver) {
  agent_->BindReceiver(
      std::move(host_remote), std::move(receiver),
      web_local_frame_impl_->GetTaskRunner(TaskType::kInternalInspector));
}

void WebDevToolsAgentImpl::DetachSession(DevToolsSession* session) {
  network_agents_.erase(session);
  page_agents_.erase(session);
  overlay_agents_.erase(session);
  if (!network_agents_.size())
    Thread::Current()->RemoveTaskObserver(this);
}

void WebDevToolsAgentImpl::InspectElement(
    const gfx::Point& point_in_local_root) {
  WebFloatRect rect(point_in_local_root.x(), point_in_local_root.y(), 0, 0);
  web_local_frame_impl_->FrameWidgetImpl()->Client()->ConvertWindowToViewport(
      &rect);
  gfx::PointF point(rect.x, rect.y);

  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kMove | HitTestRequest::kReadOnly |
      HitTestRequest::kAllowChildFrameContent;
  HitTestRequest request(hit_type);
  WebMouseEvent dummy_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            base::TimeTicks::Now());
  dummy_event.SetPositionInWidget(point);
  IntPoint transformed_point = FlooredIntPoint(
      TransformWebMouseEvent(web_local_frame_impl_->GetFrameView(), dummy_event)
          .PositionInRootFrame());
  HitTestLocation location(
      web_local_frame_impl_->GetFrameView()->ConvertFromRootFrame(
          transformed_point));
  HitTestResult result(request, location);
  web_local_frame_impl_->GetFrame()->ContentLayoutObject()->HitTest(location,
                                                                    result);
  Node* node = result.InnerNode();
  if (!node && web_local_frame_impl_->GetFrame()->GetDocument())
    node = web_local_frame_impl_->GetFrame()->GetDocument()->documentElement();

  if (!overlay_agents_.IsEmpty()) {
    for (auto& it : overlay_agents_)
      it.value->Inspect(node);
  } else {
    node_to_inspect_ = node;
  }
}

void WebDevToolsAgentImpl::DebuggerTaskStarted() {
  probe::WillStartDebuggerTask(probe_sink_);
}

void WebDevToolsAgentImpl::DebuggerTaskFinished() {
  probe::DidFinishDebuggerTask(probe_sink_);
}

void WebDevToolsAgentImpl::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  resource_container_->DidCommitLoadForLocalFrame(frame);
  resource_content_loader_->DidCommitLoadForLocalFrame(frame);
}

bool WebDevToolsAgentImpl::ScreencastEnabled() {
  for (auto& it : page_agents_) {
    if (it.value->ScreencastEnabled())
      return true;
  }
  return false;
}

void WebDevToolsAgentImpl::PageLayoutInvalidated(bool resized) {
  for (auto& it : overlay_agents_)
    it.value->PageLayoutInvalidated(resized);
}

void WebDevToolsAgentImpl::DidShowNewWindow() {
  if (!wait_for_debugger_when_shown_)
    return;
  wait_for_debugger_when_shown_ = false;
  WaitForDebugger();
}

void WebDevToolsAgentImpl::WaitForDebuggerWhenShown() {
  wait_for_debugger_when_shown_ = true;
}

void WebDevToolsAgentImpl::WaitForDebugger() {
  ClientMessageLoopAdapter::PauseForPageWait(web_local_frame_impl_);
}

bool WebDevToolsAgentImpl::IsInspectorLayer(const cc::Layer* layer) {
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

void WebDevToolsAgentImpl::UpdateOverlaysPrePaint() {
  for (auto& it : overlay_agents_)
    it.value->UpdatePrePaint();
}

void WebDevToolsAgentImpl::PaintOverlays(GraphicsContext& context) {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  for (auto& it : overlay_agents_)
    it.value->PaintOverlay(context);
}

void WebDevToolsAgentImpl::DispatchBufferedTouchEvents() {
  for (auto& it : overlay_agents_)
    it.value->DispatchBufferedTouchEvents();
}

WebInputEventResult WebDevToolsAgentImpl::HandleInputEvent(
    const WebInputEvent& event) {
  for (auto& it : overlay_agents_) {
    auto result = it.value->HandleInputEvent(event);
    if (result != WebInputEventResult::kNotHandled)
      return result;
  }
  return WebInputEventResult::kNotHandled;
}

String WebDevToolsAgentImpl::NavigationInitiatorInfo(LocalFrame* frame) {
  for (auto& it : network_agents_) {
    String initiator = it.value->NavigationInitiatorInfo(frame);
    if (!initiator.IsNull())
      return initiator;
  }
  return String();
}

void WebDevToolsAgentImpl::FlushProtocolNotifications() {
  agent_->FlushProtocolNotifications();
}

void WebDevToolsAgentImpl::WillProcessTask(
    const base::PendingTask& pending_task,
    bool was_blocked_or_low_priority) {
  if (network_agents_.IsEmpty())
    return;
  ThreadDebugger::IdleFinished(V8PerIsolateData::MainThreadIsolate());
}

void WebDevToolsAgentImpl::DidProcessTask(
    const base::PendingTask& pending_task) {
  if (network_agents_.IsEmpty())
    return;
  ThreadDebugger::IdleStarted(V8PerIsolateData::MainThreadIsolate());
  FlushProtocolNotifications();
}

}  // namespace blink
