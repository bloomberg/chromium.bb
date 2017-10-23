// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/SuspendableScriptExecutor.h"

#include <memory>
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8PersistentValueVector.h"
#include "bindings/core/v8/WindowProxy.h"
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/frame/LocalFrame.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebVector.h"
#include "public/web/WebScriptExecutionCallback.h"

namespace blink {

namespace {

class WebScriptExecutor : public SuspendableScriptExecutor::Executor {
 public:
  WebScriptExecutor(const HeapVector<ScriptSourceCode>& sources,
                    int world_id,
                    bool user_gesture);

  Vector<v8::Local<v8::Value>> Execute(LocalFrame*) override;

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(sources_);
    SuspendableScriptExecutor::Executor::Trace(visitor);
  }

 private:
  HeapVector<ScriptSourceCode> sources_;
  int world_id_;
  bool user_gesture_;
};

WebScriptExecutor::WebScriptExecutor(
    const HeapVector<ScriptSourceCode>& sources,
    int world_id,
    bool user_gesture)
    : sources_(sources), world_id_(world_id), user_gesture_(user_gesture) {}

Vector<v8::Local<v8::Value>> WebScriptExecutor::Execute(LocalFrame* frame) {
  std::unique_ptr<UserGestureIndicator> indicator;
  if (user_gesture_) {
    indicator =
        Frame::NotifyUserActivation(frame, UserGestureToken::kNewGesture);
  }

  Vector<v8::Local<v8::Value>> results;
  if (world_id_) {
    frame->GetScriptController().ExecuteScriptInIsolatedWorld(
        world_id_, sources_, &results);
  } else {
    v8::Local<v8::Value> script_value =
        frame->GetScriptController().ExecuteScriptInMainWorldAndReturnValue(
            sources_.front());
    results.push_back(script_value);
  }

  return results;
}

class V8FunctionExecutor : public SuspendableScriptExecutor::Executor {
 public:
  V8FunctionExecutor(v8::Isolate*,
                     v8::Local<v8::Function>,
                     v8::Local<v8::Value> receiver,
                     int argc,
                     v8::Local<v8::Value> argv[]);

  Vector<v8::Local<v8::Value>> Execute(LocalFrame*) override;

 private:
  ScopedPersistent<v8::Function> function_;
  ScopedPersistent<v8::Value> receiver_;
  V8PersistentValueVector<v8::Value> args_;
  scoped_refptr<UserGestureToken> gesture_token_;
};

V8FunctionExecutor::V8FunctionExecutor(v8::Isolate* isolate,
                                       v8::Local<v8::Function> function,
                                       v8::Local<v8::Value> receiver,
                                       int argc,
                                       v8::Local<v8::Value> argv[])
    : function_(isolate, function),
      receiver_(isolate, receiver),
      args_(isolate),
      gesture_token_(UserGestureIndicator::CurrentToken()) {
  args_.ReserveCapacity(argc);
  for (int i = 0; i < argc; ++i)
    args_.Append(argv[i]);
}

Vector<v8::Local<v8::Value>> V8FunctionExecutor::Execute(LocalFrame* frame) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  Vector<v8::Local<v8::Value>> results;
  v8::Local<v8::Value> single_result;
  Vector<v8::Local<v8::Value>> args;
  args.ReserveCapacity(args_.Size());
  for (size_t i = 0; i < args_.Size(); ++i)
    args.push_back(args_.Get(i));
  {
    std::unique_ptr<UserGestureIndicator> gesture_indicator;
    if (gesture_token_) {
      gesture_indicator =
          WTF::WrapUnique(new UserGestureIndicator(std::move(gesture_token_)));
    }
    if (V8ScriptRunner::CallFunction(function_.NewLocal(isolate),
                                     frame->GetDocument(),
                                     receiver_.NewLocal(isolate), args.size(),
                                     args.data(), ToIsolate(frame))
            .ToLocal(&single_result))
      results.push_back(single_result);
  }
  return results;
}

}  // namespace

SuspendableScriptExecutor* SuspendableScriptExecutor::Create(
    LocalFrame* frame,
    scoped_refptr<DOMWrapperWorld> world,
    const HeapVector<ScriptSourceCode>& sources,
    bool user_gesture,
    WebScriptExecutionCallback* callback) {
  ScriptState* script_state = ToScriptState(frame, *world);
  return new SuspendableScriptExecutor(
      frame, script_state, callback,
      new WebScriptExecutor(sources, world->GetWorldId(), user_gesture));
}

void SuspendableScriptExecutor::CreateAndRun(
    LocalFrame* frame,
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Function> function,
    v8::Local<v8::Value> receiver,
    int argc,
    v8::Local<v8::Value> argv[],
    WebScriptExecutionCallback* callback) {
  ScriptState* script_state = ScriptState::From(context);
  if (!script_state->ContextIsValid()) {
    if (callback)
      callback->Completed(Vector<v8::Local<v8::Value>>());
    return;
  }
  SuspendableScriptExecutor* executor = new SuspendableScriptExecutor(
      frame, script_state, callback,
      new V8FunctionExecutor(isolate, function, receiver, argc, argv));
  executor->Run();
}

void SuspendableScriptExecutor::ContextDestroyed(
    ExecutionContext* destroyed_context) {
  SuspendableTimer::ContextDestroyed(destroyed_context);
  if (callback_)
    callback_->Completed(Vector<v8::Local<v8::Value>>());
  Dispose();
}

SuspendableScriptExecutor::SuspendableScriptExecutor(
    LocalFrame* frame,
    ScriptState* script_state,
    WebScriptExecutionCallback* callback,
    Executor* executor)
    : SuspendableTimer(frame->GetDocument(), TaskType::kJavascriptTimer),
      script_state_(script_state),
      callback_(callback),
      blocking_option_(kNonBlocking),
      keep_alive_(this),
      executor_(executor) {
  CHECK(script_state_);
  CHECK(script_state_->ContextIsValid());
}

SuspendableScriptExecutor::~SuspendableScriptExecutor() {}

void SuspendableScriptExecutor::Fired() {
  ExecuteAndDestroySelf();
}

void SuspendableScriptExecutor::Run() {
  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);
  if (!context->IsContextSuspended()) {
    SuspendIfNeeded();
    ExecuteAndDestroySelf();
    return;
  }
  StartOneShot(0, BLINK_FROM_HERE);
  SuspendIfNeeded();
}

void SuspendableScriptExecutor::RunAsync(BlockingOption blocking) {
  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);
  blocking_option_ = blocking;
  if (blocking_option_ == kOnloadBlocking)
    ToDocument(GetExecutionContext())->IncrementLoadEventDelayCount();

  StartOneShot(0, BLINK_FROM_HERE);
  SuspendIfNeeded();
}

void SuspendableScriptExecutor::ExecuteAndDestroySelf() {
  CHECK(script_state_->ContextIsValid());

  if (callback_)
    callback_->WillExecute();

  ScriptState::Scope script_scope(script_state_.get());
  Vector<v8::Local<v8::Value>> results =
      executor_->Execute(ToDocument(GetExecutionContext())->GetFrame());

  // The script may have removed the frame, in which case contextDestroyed()
  // will have handled the disposal/callback.
  if (!script_state_->ContextIsValid())
    return;

  if (blocking_option_ == kOnloadBlocking)
    ToDocument(GetExecutionContext())->DecrementLoadEventDelayCount();

  if (callback_)
    callback_->Completed(results);

  Dispose();
}

void SuspendableScriptExecutor::Dispose() {
  // Remove object as a ContextLifecycleObserver.
  SuspendableObject::ClearContext();
  keep_alive_.Clear();
  Stop();
}

void SuspendableScriptExecutor::Trace(blink::Visitor* visitor) {
  visitor->Trace(executor_);
  SuspendableTimer::Trace(visitor);
}

}  // namespace blink
