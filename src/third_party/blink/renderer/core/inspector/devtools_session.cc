// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/devtools_session.h"

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/inspector/devtools_agent.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_session_state.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/protocol/Protocol.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

namespace {
const char kV8StateKey[] = "v8";
bool ShouldInterruptForMethod(const String& method) {
  // Keep in sync with DevToolsSession::ShouldSendOnIO.
  // TODO(dgozman): find a way to share this.
  return method == "Debugger.pause" || method == "Debugger.setBreakpoint" ||
         method == "Debugger.setBreakpointByUrl" ||
         method == "Debugger.removeBreakpoint" ||
         method == "Debugger.setBreakpointsActive" ||
         method == "Performance.getMetrics" || method == "Page.crash" ||
         method == "Runtime.terminateExecution" ||
         method == "Emulation.setScriptExecutionDisabled";
}
}  // namespace

// Created and stored in unique_ptr on UI.
// Binds request, receives messages and destroys on IO.
class DevToolsSession::IOSession : public mojom::blink::DevToolsSession {
 public:
  IOSession(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
            scoped_refptr<InspectorTaskRunner> inspector_task_runner,
            CrossThreadWeakPersistent<::blink::DevToolsSession> session,
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
    DCHECK(ShouldInterruptForMethod(method));
    // Crash renderer.
    if (method == "Page.crash")
      CHECK(false);
    inspector_task_runner_->AppendTask(
        CrossThreadBind(&DevToolsSession::DispatchProtocolCommand, session_,
                        call_id, method, message));
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<InspectorTaskRunner> inspector_task_runner_;
  CrossThreadWeakPersistent<::blink::DevToolsSession> session_;
  mojo::Binding<mojom::blink::DevToolsSession> binding_;

  DISALLOW_COPY_AND_ASSIGN(IOSession);
};

DevToolsSession::DevToolsSession(
    DevToolsAgent* agent,
    mojom::blink::DevToolsSessionHostAssociatedPtrInfo host_ptr_info,
    mojom::blink::DevToolsSessionAssociatedRequest main_request,
    mojom::blink::DevToolsSessionRequest io_request,
    mojom::blink::DevToolsSessionStatePtr reattach_session_state)
    : agent_(agent),
      binding_(this, std::move(main_request)),
      inspector_backend_dispatcher_(new protocol::UberDispatcher(this)),
      session_state_(std::move(reattach_session_state)),
      v8_session_state_(kV8StateKey),
      v8_session_state_json_(&v8_session_state_, /*default_value=*/String()) {
  io_session_ =
      new IOSession(agent_->io_task_runner_, agent_->inspector_task_runner_,
                    WrapCrossThreadWeakPersistent(this), std::move(io_request));

  host_ptr_.Bind(std::move(host_ptr_info));
  host_ptr_.set_connection_error_handler(
      WTF::Bind(&DevToolsSession::Detach, WrapWeakPersistent(this)));

  bool restore = !!session_state_.ReattachState();
  v8_session_state_.InitFrom(&session_state_);
  agent_->client_->AttachSession(this, restore);
  agent_->probe_sink_->addDevToolsSession(this);
  if (restore) {
    for (wtf_size_t i = 0; i < agents_.size(); i++)
      agents_[i]->Restore();
  }
}

DevToolsSession::~DevToolsSession() {
  DCHECK(IsDetached());
}

void DevToolsSession::ConnectToV8(v8_inspector::V8Inspector* inspector,
                                  int context_group_id) {
  v8_session_ =
      inspector->connect(context_group_id, this,
                         ToV8InspectorStringView(v8_session_state_json_.Get()));
}

bool DevToolsSession::IsDetached() {
  return !host_ptr_.is_bound();
}

void DevToolsSession::Append(InspectorAgent* agent) {
  agents_.push_back(agent);
  agent->Init(agent_->probe_sink_.Get(), inspector_backend_dispatcher_.get(),
              &session_state_);
}

void DevToolsSession::Detach() {
  agent_->client_->DebuggerTaskStarted();
  agent_->client_->DetachSession(this);
  agent_->sessions_.erase(this);
  binding_.Close();
  host_ptr_.reset();
  io_session_->DeleteSoon();
  io_session_ = nullptr;
  agent_->probe_sink_->removeDevToolsSession(this);
  inspector_backend_dispatcher_.reset();
  for (wtf_size_t i = agents_.size(); i > 0; i--)
    agents_[i - 1]->Dispose();
  agents_.clear();
  v8_session_.reset();
  agent_->client_->DebuggerTaskFinished();
}

void DevToolsSession::FlushProtocolNotifications() {
  flushProtocolNotifications();
}

void DevToolsSession::DispatchProtocolCommand(int call_id,
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
  if (IsDetached())
    return;
  agent_->client_->DebuggerTaskStarted();
  if (v8_inspector::V8InspectorSession::canDispatchMethod(
          ToV8InspectorStringView(method))) {
    v8_session_->dispatchProtocolMessage(ToV8InspectorStringView(message));
  } else {
    inspector_backend_dispatcher_->dispatch(
        call_id, method, protocol::StringUtil::parseJSON(message), message);
  }
  agent_->client_->DebuggerTaskFinished();
}

void DevToolsSession::DidStartProvisionalLoad(LocalFrame* frame) {
  if (v8_session_ && agent_->inspected_frames_->Root() == frame) {
    v8_session_->setSkipAllPauses(true);
    v8_session_->resume();
  }
}

void DevToolsSession::DidFailProvisionalLoad(LocalFrame* frame) {
  if (v8_session_ && agent_->inspected_frames_->Root() == frame)
    v8_session_->setSkipAllPauses(false);
}

void DevToolsSession::DidCommitLoad(LocalFrame* frame, DocumentLoader*) {
  for (wtf_size_t i = 0; i < agents_.size(); i++)
    agents_[i]->DidCommitLoadForLocalFrame(frame);
  if (v8_session_ && agent_->inspected_frames_->Root() == frame)
    v8_session_->setSkipAllPauses(false);
}

void DevToolsSession::sendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  SendProtocolResponse(call_id, message->serialize());
}

void DevToolsSession::fallThrough(int call_id,
                                  const String& method,
                                  const String& message) {
  // There's no other layer to handle the command.
  NOTREACHED();
}

void DevToolsSession::sendResponse(
    int call_id,
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  // We can potentially avoid copies if WebString would convert to utf8 right
  // from StringView, but it uses StringImpl itself, so we don't create any
  // extra copies here.
  SendProtocolResponse(call_id, ToCoreString(message->string()));
}

void DevToolsSession::SendProtocolResponse(int call_id, const String& message) {
  if (IsDetached())
    return;
  flushProtocolNotifications();
  if (v8_session_)
    v8_session_state_json_.Set(ToCoreString(v8_session_->stateJSON()));
  // Make tests more predictable by flushing all sessions before sending
  // protocol response in any of them.
  if (WebTestSupport::IsRunningWebTest())
    agent_->FlushProtocolNotifications();
  host_ptr_->DispatchProtocolResponse(message, call_id,
                                      session_state_.TakeUpdates());
}

class DevToolsSession::Notification {
 public:
  static std::unique_ptr<Notification> CreateForBlink(
      std::unique_ptr<protocol::Serializable> notification) {
    return std::unique_ptr<Notification>(
        new Notification(std::move(notification)));
  }

  static std::unique_ptr<Notification> CreateForV8(
      std::unique_ptr<v8_inspector::StringBuffer> notification) {
    return std::unique_ptr<Notification>(
        new Notification(std::move(notification)));
  }

  explicit Notification(std::unique_ptr<protocol::Serializable> notification)
      : blink_notification_(std::move(notification)) {}

  explicit Notification(
      std::unique_ptr<v8_inspector::StringBuffer> notification)
      : v8_notification_(std::move(notification)) {}

  String Serialize() {
    if (blink_notification_) {
      serialized_ = blink_notification_->serialize();
      blink_notification_.reset();
    } else if (v8_notification_) {
      serialized_ = ToCoreString(v8_notification_->string());
      v8_notification_.reset();
    }
    return serialized_;
  }

 private:
  std::unique_ptr<protocol::Serializable> blink_notification_;
  std::unique_ptr<v8_inspector::StringBuffer> v8_notification_;
  String serialized_;
};

void DevToolsSession::sendProtocolNotification(
    std::unique_ptr<protocol::Serializable> notification) {
  if (IsDetached())
    return;
  notification_queue_.push_back(
      Notification::CreateForBlink(std::move(notification)));
}

void DevToolsSession::sendNotification(
    std::unique_ptr<v8_inspector::StringBuffer> notification) {
  if (IsDetached())
    return;
  notification_queue_.push_back(
      Notification::CreateForV8(std::move(notification)));
}

void DevToolsSession::flushProtocolNotifications() {
  if (IsDetached())
    return;
  for (wtf_size_t i = 0; i < agents_.size(); i++)
    agents_[i]->FlushPendingProtocolNotifications();
  if (!notification_queue_.size())
    return;
  if (v8_session_)
    v8_session_state_json_.Set(ToCoreString(v8_session_->stateJSON()));
  for (wtf_size_t i = 0; i < notification_queue_.size(); ++i) {
    host_ptr_->DispatchProtocolNotification(notification_queue_[i]->Serialize(),
                                            session_state_.TakeUpdates());
  }
  notification_queue_.clear();
}

void DevToolsSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(agent_);
  visitor->Trace(agents_);
}

}  // namespace blink
