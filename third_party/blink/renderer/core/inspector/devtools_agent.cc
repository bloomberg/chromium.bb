// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/devtools_agent.h"

#include <v8-inspector.h>
#include <memory>

#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/inspector_session.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/layout_test_support.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// --------- DevToolsAgent::Session -------------

class DevToolsAgent::Session : public GarbageCollectedFinalized<Session>,
                               public mojom::blink::DevToolsSession,
                               public InspectorSession::Client {
 public:
  Session(DevToolsAgent*,
          mojom::blink::DevToolsSessionHostAssociatedPtrInfo host_ptr_info,
          mojom::blink::DevToolsSessionAssociatedRequest main_request,
          mojom::blink::DevToolsSessionRequest io_request,
          mojom::blink::DevToolsSessionStatePtr reattach_session_state);
  ~Session() override;

  virtual void Trace(blink::Visitor*);
  void Detach();

  InspectorSession* inspector_session() { return inspector_session_.Get(); }

 private:
  class IOSession;

  // mojom::blink::DevToolsSession implementation.
  void DispatchProtocolCommand(int call_id,
                               const String& method,
                               const String& message) override;

  // InspectorSession::Client implementation.
  void SendProtocolResponse(
      int session_id,
      int call_id,
      const String& response,
      mojom::blink::DevToolsSessionStatePtr updates) override;
  void SendProtocolNotification(
      int session_id,
      const String& message,
      mojom::blink::DevToolsSessionStatePtr updates) override;

  void DispatchProtocolCommandInternal(int call_id,
                                       const String& method,
                                       const String& message);

  Member<DevToolsAgent> agent_;
  mojo::AssociatedBinding<mojom::blink::DevToolsSession> binding_;
  mojom::blink::DevToolsSessionHostAssociatedPtr host_ptr_;
  IOSession* io_session_;
  Member<InspectorSession> inspector_session_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

// Created and stored in unique_ptr on UI.
// Binds request, receives messages and destroys on IO.
class DevToolsAgent::Session::IOSession : public mojom::blink::DevToolsSession {
 public:
  IOSession(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
            scoped_refptr<InspectorTaskRunner> inspector_task_runner,
            CrossThreadWeakPersistent<DevToolsAgent::Session> session,
            mojom::blink::DevToolsSessionRequest request)
      : io_task_runner_(io_task_runner),
        inspector_task_runner_(inspector_task_runner),
        session_(std::move(session)),
        binding_(this) {
    io_task_runner->PostTask(
        FROM_HERE, ConvertToBaseCallback(CrossThreadBind(
                       &IOSession::BindInterface, CrossThreadUnretained(this),
                       WTF::Passed(std::move(request)))));
  }

  ~IOSession() override {}

  void BindInterface(mojom::blink::DevToolsSessionRequest request) {
    binding_.Bind(std::move(request), io_task_runner_);
  }

  void DeleteSoon() { io_task_runner_->DeleteSoon(FROM_HERE, this); }

  // mojom::blink::DevToolsSession implementation.
  void DispatchProtocolCommand(int call_id,
                               const String& method,
                               const String& message) override {
    DCHECK(InspectorSession::ShouldInterruptForMethod(method));
    // Crash renderer.
    if (method == "Page.crash")
      CHECK(false);
    inspector_task_runner_->AppendTask(
        CrossThreadBind(&DevToolsAgent::Session::DispatchProtocolCommand,
                        session_, call_id, method, message));
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<InspectorTaskRunner> inspector_task_runner_;
  CrossThreadWeakPersistent<DevToolsAgent::Session> session_;
  mojo::Binding<mojom::blink::DevToolsSession> binding_;

  DISALLOW_COPY_AND_ASSIGN(IOSession);
};

DevToolsAgent::Session::Session(
    DevToolsAgent* agent,
    mojom::blink::DevToolsSessionHostAssociatedPtrInfo host_ptr_info,
    mojom::blink::DevToolsSessionAssociatedRequest request,
    mojom::blink::DevToolsSessionRequest io_request,
    mojom::blink::DevToolsSessionStatePtr reattach_session_state)
    : agent_(agent), binding_(this, std::move(request)) {
  io_session_ =
      new IOSession(agent_->io_task_runner_, agent_->inspector_task_runner_,
                    WrapCrossThreadWeakPersistent(this), std::move(io_request));

  host_ptr_.Bind(std::move(host_ptr_info));
  host_ptr_.set_connection_error_handler(
      WTF::Bind(&DevToolsAgent::Session::Detach, WrapWeakPersistent(this)));
  inspector_session_ =
      agent_->client_->AttachSession(this, std::move(reattach_session_state));
}

DevToolsAgent::Session::~Session() {
  DCHECK(!host_ptr_.is_bound());
}

void DevToolsAgent::Session::Trace(blink::Visitor* visitor) {
  visitor->Trace(agent_);
  visitor->Trace(inspector_session_);
}

void DevToolsAgent::Session::Detach() {
  agent_->client_->DebuggerTaskStarted();
  agent_->client_->DetachSession(inspector_session_.Get());
  agent_->sessions_.erase(this);
  binding_.Close();
  host_ptr_.reset();
  io_session_->DeleteSoon();
  io_session_ = nullptr;
  inspector_session_->Dispose();
  agent_->client_->DebuggerTaskFinished();
}

void DevToolsAgent::Session::SendProtocolResponse(
    int session_id,
    int call_id,
    const String& response,
    mojom::blink::DevToolsSessionStatePtr updates) {
  if (!host_ptr_.is_bound())
    return;
  // Make tests more predictable by flushing all sessions before sending
  // protocol response in any of them.
  if (LayoutTestSupport::IsRunningLayoutTest())
    agent_->FlushProtocolNotifications();
  host_ptr_->DispatchProtocolResponse(response, call_id, std::move(updates));
}

void DevToolsAgent::Session::SendProtocolNotification(
    int session_id,
    const String& message,
    mojom::blink::DevToolsSessionStatePtr updates) {
  if (!host_ptr_.is_bound())
    return;
  host_ptr_->DispatchProtocolNotification(message, std::move(updates));
}

void DevToolsAgent::Session::DispatchProtocolCommand(int call_id,
                                                     const String& method,
                                                     const String& message) {
  // IOSession does not provide ordering guarantees relative to
  // Session, so a command may come to IOSession after Session is detached,
  // and get posted to main thread to this method.
  //
  // At the same time, Session may not be garbage collected yet
  // (even though already detached), and CrossThreadWeakPersistent<Session>
  // will still be valid.
  //
  // Both these factors combined may lead to this method being called after
  // detach, so we have to check it here.
  if (!host_ptr_.is_bound())
    return;
  agent_->client_->DebuggerTaskStarted();
  inspector_session_->DispatchProtocolMessage(call_id, method, message);
  agent_->client_->DebuggerTaskFinished();
}

// --------- DevToolsAgent -------------

// static
DevToolsAgent* DevToolsAgent::From(ExecutionContext* execution_context) {
  if (!execution_context)
    return nullptr;
  if (auto* scope = DynamicTo<WorkerGlobalScope>(execution_context)) {
    return scope->GetThread()
        ->GetWorkerInspectorController()
        ->GetDevToolsAgent();
  }
  if (auto* document = DynamicTo<Document>(execution_context)) {
    LocalFrame* frame = document->GetFrame();
    if (!frame)
      return nullptr;
    WebLocalFrameImpl* web_frame =
        WebLocalFrameImpl::FromFrame(frame->LocalFrameRoot());
    if (!web_frame)
      return nullptr;
    return web_frame->DevToolsAgentImpl()->GetDevToolsAgent();
  }
  return nullptr;
}

DevToolsAgent::DevToolsAgent(
    Client* client,
    scoped_refptr<InspectorTaskRunner> inspector_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : client_(client),
      binding_(this),
      associated_binding_(this),
      inspector_task_runner_(std::move(inspector_task_runner)),
      io_task_runner_(std::move(io_task_runner)) {}

DevToolsAgent::~DevToolsAgent() {}

void DevToolsAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(sessions_);
}

void DevToolsAgent::Dispose() {
  HeapHashSet<Member<Session>> copy(sessions_);
  for (auto& session : copy)
    session->Detach();
  CleanupConnection();
}

void DevToolsAgent::BindRequest(
    mojom::blink::DevToolsAgentHostPtrInfo host_ptr_info,
    mojom::blink::DevToolsAgentRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!binding_);
  DCHECK(!associated_binding_);
  binding_.Bind(std::move(request), std::move(task_runner));
  host_ptr_.Bind(std::move(host_ptr_info));
  host_ptr_.set_connection_error_handler(
      WTF::Bind(&DevToolsAgent::CleanupConnection, WrapWeakPersistent(this)));
}

void DevToolsAgent::BindRequest(
    mojom::blink::DevToolsAgentHostAssociatedPtrInfo host_ptr_info,
    mojom::blink::DevToolsAgentAssociatedRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!binding_);
  DCHECK(!associated_binding_);
  associated_binding_.Bind(std::move(request), std::move(task_runner));
  associated_host_ptr_.Bind(std::move(host_ptr_info));
  associated_host_ptr_.set_connection_error_handler(
      WTF::Bind(&DevToolsAgent::CleanupConnection, WrapWeakPersistent(this)));
}

void DevToolsAgent::AttachDevToolsSession(
    mojom::blink::DevToolsSessionHostAssociatedPtrInfo host,
    mojom::blink::DevToolsSessionAssociatedRequest session_request,
    mojom::blink::DevToolsSessionRequest io_session_request,
    mojom::blink::DevToolsSessionStatePtr reattach_session_state) {
  client_->DebuggerTaskStarted();
  Session* session = new Session(
      this, std::move(host), std::move(session_request),
      std::move(io_session_request), std::move(reattach_session_state));
  sessions_.insert(session);
  client_->DebuggerTaskFinished();
}

void DevToolsAgent::InspectElement(const WebPoint& point) {
  client_->InspectElement(point);
}

void DevToolsAgent::FlushProtocolNotifications() {
  for (auto& session : sessions_)
    session->inspector_session()->flushProtocolNotifications();
}

void DevToolsAgent::ReportChildWorkers(bool report, bool wait_for_debugger) {
  report_child_workers_ = report;
  pause_child_workers_on_start_ = wait_for_debugger;
  if (!report_child_workers_)
    return;
  auto workers = std::move(unreported_child_worker_threads_);
  for (auto& it : workers)
    ReportChildWorker(std::move(it.value));
}

void DevToolsAgent::ChildWorkerThreadCreated(
    WorkerThread* worker_thread,
    const KURL& url,
    mojom::blink::DevToolsAgentRequest& agent_request,
    mojom::blink::DevToolsAgentHostPtrInfo& host_ptr_info,
    bool& wait_for_debugger) {
  auto data = std::make_unique<WorkerData>();
  data->url = url;
  agent_request = mojo::MakeRequest(&data->agent_ptr);
  data->host_request = mojo::MakeRequest(&host_ptr_info);
  data->devtools_worker_token = worker_thread->GetDevToolsWorkerToken();
  data->waiting_for_debugger = pause_child_workers_on_start_;
  wait_for_debugger = pause_child_workers_on_start_;

  if (report_child_workers_) {
    ReportChildWorker(std::move(data));
  } else {
    unreported_child_worker_threads_.insert(worker_thread, std::move(data));
  }
}

void DevToolsAgent::ChildWorkerThreadTerminated(WorkerThread* worker_thread) {
  unreported_child_worker_threads_.erase(worker_thread);
}

void DevToolsAgent::ReportChildWorker(std::unique_ptr<WorkerData> data) {
  if (host_ptr_.is_bound()) {
    host_ptr_->ChildWorkerCreated(
        std::move(data->agent_ptr), std::move(data->host_request),
        std::move(data->url), data->devtools_worker_token,
        data->waiting_for_debugger);
  } else if (associated_host_ptr_.is_bound()) {
    associated_host_ptr_->ChildWorkerCreated(
        std::move(data->agent_ptr), std::move(data->host_request),
        std::move(data->url), data->devtools_worker_token,
        data->waiting_for_debugger);
  }
}

void DevToolsAgent::CleanupConnection() {
  binding_.Close();
  associated_binding_.Close();
  host_ptr_.reset();
  associated_host_ptr_.reset();
  report_child_workers_ = false;
  pause_child_workers_on_start_ = false;
}

}  // namespace blink
