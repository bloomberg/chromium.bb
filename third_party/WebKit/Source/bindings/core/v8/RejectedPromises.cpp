// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/RejectedPromises.h"

#include <memory>
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/events/EventTarget.h"
#include "core/events/PromiseRejectionEvent.h"
#include "core/inspector/ThreadDebugger.h"
#include "platform/WebTaskRunner.h"
#include "platform/bindings/ScopedPersistent.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

static const unsigned kMaxReportedHandlersPendingResolution = 1000;

class RejectedPromises::Message final {
 public:
  static std::unique_ptr<Message> Create(
      ScriptState* script_state,
      v8::Local<v8::Promise> promise,
      v8::Local<v8::Value> exception,
      const String& error_message,
      std::unique_ptr<SourceLocation> location,
      AccessControlStatus cors_status) {
    return WTF::WrapUnique(new Message(script_state, promise, exception,
                                       error_message, std::move(location),
                                       cors_status));
  }

  bool IsCollected() { return collected_ || !script_state_->ContextIsValid(); }

  bool HasPromise(v8::Local<v8::Value> promise) {
    ScriptState::Scope scope(script_state_);
    return promise == promise_.NewLocal(script_state_->GetIsolate());
  }

  void Report() {
    if (!script_state_->ContextIsValid())
      return;
    // If execution termination has been triggered, quietly bail out.
    if (script_state_->GetIsolate()->IsExecutionTerminating())
      return;
    ExecutionContext* execution_context = ExecutionContext::From(script_state_);
    if (!execution_context)
      return;

    ScriptState::Scope scope(script_state_);
    v8::Local<v8::Value> value = promise_.NewLocal(script_state_->GetIsolate());
    v8::Local<v8::Value> reason =
        exception_.NewLocal(script_state_->GetIsolate());
    // Either collected or https://crbug.com/450330
    if (value.IsEmpty() || !value->IsPromise())
      return;
    DCHECK(!HasHandler());

    EventTarget* target = execution_context->ErrorEventTarget();
    if (target && !execution_context->ShouldSanitizeScriptError(resource_name_,
                                                                cors_status_)) {
      PromiseRejectionEventInit init;
      init.setPromise(ScriptPromise(script_state_, value));
      init.setReason(ScriptValue(script_state_, reason));
      init.setCancelable(true);
      PromiseRejectionEvent* event = PromiseRejectionEvent::Create(
          script_state_, EventTypeNames::unhandledrejection, init);
      // Log to console if event was not canceled.
      should_log_to_console_ =
          target->DispatchEvent(event) == DispatchEventResult::kNotCanceled;
    }

    if (should_log_to_console_) {
      ThreadDebugger* debugger =
          ThreadDebugger::From(script_state_->GetIsolate());
      if (debugger) {
        promise_rejection_id_ = debugger->PromiseRejected(
            script_state_->GetContext(), error_message_, reason,
            std::move(location_));
      }
    }

    location_.reset();
  }

  void Revoke() {
    ExecutionContext* execution_context = ExecutionContext::From(script_state_);
    if (!execution_context)
      return;

    ScriptState::Scope scope(script_state_);
    v8::Local<v8::Value> value = promise_.NewLocal(script_state_->GetIsolate());
    v8::Local<v8::Value> reason =
        exception_.NewLocal(script_state_->GetIsolate());
    // Either collected or https://crbug.com/450330
    if (value.IsEmpty() || !value->IsPromise())
      return;

    EventTarget* target = execution_context->ErrorEventTarget();
    if (target && !execution_context->ShouldSanitizeScriptError(resource_name_,
                                                                cors_status_)) {
      PromiseRejectionEventInit init;
      init.setPromise(ScriptPromise(script_state_, value));
      init.setReason(ScriptValue(script_state_, reason));
      PromiseRejectionEvent* event = PromiseRejectionEvent::Create(
          script_state_, EventTypeNames::rejectionhandled, init);
      target->DispatchEvent(event);
    }

    if (should_log_to_console_ && promise_rejection_id_) {
      ThreadDebugger* debugger =
          ThreadDebugger::From(script_state_->GetIsolate());
      if (debugger) {
        debugger->PromiseRejectionRevoked(script_state_->GetContext(),
                                          promise_rejection_id_);
      }
    }
  }

  void MakePromiseWeak() {
    DCHECK(!promise_.IsEmpty());
    DCHECK(!promise_.IsWeak());
    promise_.SetWeak(this, &Message::DidCollectPromise);
    exception_.SetWeak(this, &Message::DidCollectException);
  }

  void MakePromiseStrong() {
    DCHECK(!promise_.IsEmpty());
    DCHECK(promise_.IsWeak());
    promise_.ClearWeak();
    exception_.ClearWeak();
  }

  bool HasHandler() {
    DCHECK(!IsCollected());
    ScriptState::Scope scope(script_state_);
    v8::Local<v8::Value> value = promise_.NewLocal(script_state_->GetIsolate());
    return v8::Local<v8::Promise>::Cast(value)->HasHandler();
  }

 private:
  Message(ScriptState* script_state,
          v8::Local<v8::Promise> promise,
          v8::Local<v8::Value> exception,
          const String& error_message,
          std::unique_ptr<SourceLocation> location,
          AccessControlStatus cors_status)
      : script_state_(script_state),
        promise_(script_state->GetIsolate(), promise),
        exception_(script_state->GetIsolate(), exception),
        error_message_(error_message),
        resource_name_(location->Url()),
        location_(std::move(location)),
        promise_rejection_id_(0),
        collected_(false),
        should_log_to_console_(true),
        cors_status_(cors_status) {}

  static void DidCollectPromise(const v8::WeakCallbackInfo<Message>& data) {
    data.GetParameter()->collected_ = true;
    data.GetParameter()->promise_.Clear();
  }

  static void DidCollectException(const v8::WeakCallbackInfo<Message>& data) {
    data.GetParameter()->exception_.Clear();
  }

  ScriptState* script_state_;
  ScopedPersistent<v8::Promise> promise_;
  ScopedPersistent<v8::Value> exception_;
  String error_message_;
  String resource_name_;
  std::unique_ptr<SourceLocation> location_;
  unsigned promise_rejection_id_;
  bool collected_;
  bool should_log_to_console_;
  AccessControlStatus cors_status_;
};

RejectedPromises::RejectedPromises() {}

RejectedPromises::~RejectedPromises() {}

void RejectedPromises::RejectedWithNoHandler(
    ScriptState* script_state,
    v8::PromiseRejectMessage data,
    const String& error_message,
    std::unique_ptr<SourceLocation> location,
    AccessControlStatus cors_status) {
  queue_.push_back(Message::Create(script_state, data.GetPromise(),
                                   data.GetValue(), error_message,
                                   std::move(location), cors_status));
}

void RejectedPromises::HandlerAdded(v8::PromiseRejectMessage data) {
  // First look it up in the pending messages and fast return, it'll be covered
  // by processQueue().
  for (auto it = queue_.begin(); it != queue_.end(); ++it) {
    if (!(*it)->IsCollected() && (*it)->HasPromise(data.GetPromise())) {
      queue_.erase(it);
      return;
    }
  }

  // Then look it up in the reported errors.
  for (size_t i = 0; i < reported_as_errors_.size(); ++i) {
    std::unique_ptr<Message>& message = reported_as_errors_.at(i);
    if (!message->IsCollected() && message->HasPromise(data.GetPromise())) {
      message->MakePromiseStrong();
      Platform::Current()
          ->CurrentThread()
          ->Scheduler()
          ->TimerTaskRunner()
          ->PostTask(BLINK_FROM_HERE,
                     WTF::Bind(&RejectedPromises::RevokeNow,
                               RefPtr<RejectedPromises>(this),
                               WTF::Passed(std::move(message))));
      reported_as_errors_.erase(i);
      return;
    }
  }
}

std::unique_ptr<RejectedPromises::MessageQueue>
RejectedPromises::CreateMessageQueue() {
  return WTF::MakeUnique<MessageQueue>();
}

void RejectedPromises::Dispose() {
  if (queue_.IsEmpty())
    return;

  std::unique_ptr<MessageQueue> queue = CreateMessageQueue();
  queue->Swap(queue_);
  ProcessQueueNow(std::move(queue));
}

void RejectedPromises::ProcessQueue() {
  if (queue_.IsEmpty())
    return;

  std::unique_ptr<MessageQueue> queue = CreateMessageQueue();
  queue->Swap(queue_);
  Platform::Current()
      ->CurrentThread()
      ->Scheduler()
      ->TimerTaskRunner()
      ->PostTask(BLINK_FROM_HERE, WTF::Bind(&RejectedPromises::ProcessQueueNow,
                                            PassRefPtr<RejectedPromises>(this),
                                            WTF::Passed(std::move(queue))));
}

void RejectedPromises::ProcessQueueNow(std::unique_ptr<MessageQueue> queue) {
  // Remove collected handlers.
  for (size_t i = 0; i < reported_as_errors_.size();) {
    if (reported_as_errors_.at(i)->IsCollected())
      reported_as_errors_.erase(i);
    else
      ++i;
  }

  while (!queue->IsEmpty()) {
    std::unique_ptr<Message> message = queue->TakeFirst();
    if (message->IsCollected())
      continue;
    if (!message->HasHandler()) {
      message->Report();
      message->MakePromiseWeak();
      reported_as_errors_.push_back(std::move(message));
      if (reported_as_errors_.size() > kMaxReportedHandlersPendingResolution)
        reported_as_errors_.erase(0,
                                  kMaxReportedHandlersPendingResolution / 10);
    }
  }
}

void RejectedPromises::RevokeNow(std::unique_ptr<Message> message) {
  message->Revoke();
}

}  // namespace blink
