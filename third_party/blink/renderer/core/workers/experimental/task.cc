// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/experimental/task.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_task.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/workers/experimental/task_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/experimental/thread_pool.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"

namespace blink {

// worker_thread_ only.
class TaskBase::AsyncFunctionCompleted : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                TaskBase* task,
                                                State state) {
    return (new AsyncFunctionCompleted(script_state, task, state))
        ->BindToV8Function();
  }

  ScriptValue Call(ScriptValue v) override {
    task_->TaskCompletedOnWorkerThread(v.V8Value(), state_);
    return ScriptValue();
  }

 private:
  AsyncFunctionCompleted(ScriptState* script_state, TaskBase* task, State state)
      : ScriptFunction(script_state), task_(task), state_(state) {}
  CrossThreadPersistent<TaskBase> task_;
  const State state_;
};

TaskBase::TaskBase(TaskType task_type,
                   ScriptState* script_state,
                   const ScriptValue& function,
                   const String& function_name)
    : task_type_(task_type),
      self_keep_alive_(this),
      function_name_(function_name.IsolatedCopy()) {
  DCHECK(IsMainThread());
  DCHECK(task_type_ == TaskType::kUserInteraction ||
         task_type_ == TaskType::kIdleTask);
  // TODO(japhet): Handle serialization failures
  v8::Isolate* isolate = script_state->GetIsolate();
  if (!function.IsEmpty()) {
    function_ = SerializedScriptValue::SerializeAndSwallowExceptions(
        isolate, function.V8Value()->ToString(isolate));
  }
}

void TaskBase::InitializeArgumentsOnMainThread(
    ThreadPoolThreadProvider* thread_provider,
    ScriptState* script_state,
    const Vector<ScriptValue>& arguments) {
  v8::Isolate* isolate = script_state->GetIsolate();
  arguments_.resize(arguments.size());
  HeapVector<Member<TaskBase>> prerequisites;
  Vector<size_t> prerequisites_indices;
  for (size_t i = 0; i < arguments_.size(); i++) {
    // Normal case: if the argument isn't a Task, just serialize it.
    if (!V8Task::hasInstance(arguments[i].V8Value(), isolate)) {
      arguments_[i].serialized_value =
          SerializedScriptValue::SerializeAndSwallowExceptions(
              isolate, arguments[i].V8Value());
      continue;
    }
    TaskBase* prerequisite =
        ToScriptWrappable(arguments[i].V8Value().As<v8::Object>())
            ->ToImpl<Task>();
    prerequisites.push_back(prerequisite);
    prerequisites_indices.push_back(i);
  }

  worker_thread_ = SelectThread(prerequisites, thread_provider);
  worker_thread_->IncrementTasksInProgressCount();

  if (prerequisites.IsEmpty()) {
    MutexLocker lock(mutex_);
    MaybeStartTask();
    return;
  }

  // Prior to this point, other Task instances don't have a reference
  // to |this| yet, so no need to lock mutex_. RegisterDependencies() populates
  // those references, so RegisterDependencies() and any logic thereafter must
  // consider the potential for data races.
  RegisterDependencies(prerequisites, prerequisites_indices);
}

// static
ThreadPoolThread* TaskBase::SelectThread(
    const HeapVector<Member<TaskBase>>& prerequisites,
    ThreadPoolThreadProvider* thread_provider) {
  DCHECK(IsMainThread());
  HashCountedSet<ThreadPoolThread*> prerequisite_location_counts;
  size_t max_prerequisite_location_count = 0;
  ThreadPoolThread* max_prerequisite_thread = nullptr;
  for (TaskBase* prerequisite : prerequisites) {
    // Track which thread the prerequisites will run on. Put this task on the
    // thread where the most prerequisites reside.
    ThreadPoolThread* thread = prerequisite->worker_thread_;
    prerequisite_location_counts.insert(thread);
    unsigned thread_count = prerequisite_location_counts.count(thread);
    if (thread_count > max_prerequisite_location_count) {
      max_prerequisite_location_count = thread_count;
      max_prerequisite_thread = thread;
    }
  }
  return max_prerequisite_thread ? max_prerequisite_thread
                                 : thread_provider->GetLeastBusyThread();
}

// Should only be called from constructor. Split out in to a helper because
// clang appears to exempt constructors from thread safety analysis.
void TaskBase::RegisterDependencies(
    const HeapVector<Member<TaskBase>>& prerequisites,
    const Vector<size_t>& prerequisites_indices) {
  DCHECK(IsMainThread());
  {
    MutexLocker lock(mutex_);
    prerequisites_remaining_ = prerequisites.size();
  }

  for (size_t i = 0; i < prerequisites.size(); i++) {
    TaskBase* prerequisite = prerequisites[i];
    size_t prerequisite_index = prerequisites_indices[i];

    {
      MutexLocker lock(prerequisite->mutex_);
      if (!prerequisite->HasFinished()) {
        prerequisite->dependents_.emplace_back(
            MakeGarbageCollected<Dependent>(this, prerequisite_index));
        continue;
      }
    }

    PostCrossThreadTask(
        *prerequisite->worker_thread_->GetTaskRunner(task_type_), FROM_HERE,
        CrossThreadBind(&TaskBase::PassResultToDependentOnWorkerThread,
                        WrapCrossThreadPersistent(prerequisite),
                        prerequisite_index, WrapCrossThreadPersistent(this)));
  }
}

TaskBase::~TaskBase() {
  DCHECK(IsMainThread());
  DCHECK(HasFinished());
  DCHECK(!function_);
  DCHECK(arguments_.IsEmpty());
  DCHECK(!prerequisites_remaining_);
  DCHECK(dependents_.IsEmpty());
}

scoped_refptr<SerializedScriptValue> TaskBase::GetSerializedResult() {
  DCHECK(IsMainThread() || worker_thread_->IsCurrentThread());
  MutexLocker lock(mutex_);
  DCHECK(HasFinished());
  if (!serialized_result_ && worker_thread_->IsCurrentThread()) {
    DCHECK(v8_result_);
    ScriptState::Scope scope(
        worker_thread_->GlobalScope()->ScriptController()->GetScriptState());
    v8::Isolate* isolate = ToIsolate(worker_thread_->GlobalScope());
    serialized_result_ = SerializedScriptValue::SerializeAndSwallowExceptions(
        isolate, v8_result_->GetResult(isolate));
  }
  return serialized_result_;
}

void TaskBase::PassResultToDependentOnWorkerThread(size_t dependent_index,
                                                   TaskBase* dependent) {
  DCHECK(worker_thread_->IsCurrentThread());
  bool failed = false;
  {
    MutexLocker lock(mutex_);
    DCHECK(HasFinished());
    failed = state_ == State::kFailed;
  }

  // Only serialize if the dependent needs the result on a different thread.
  // Otherwise, use the unserialized result from v8.
  scoped_refptr<SerializedScriptValue> serialized_result =
      dependent->IsTargetThreadForArguments() ? nullptr : GetSerializedResult();
  V8ResultHolder* v8_result =
      dependent->IsTargetThreadForArguments() ? v8_result_.Get() : nullptr;
  dependent->PrerequisiteFinished(dependent_index, v8_result, serialized_result,
                                  failed);
}

void TaskBase::PrerequisiteFinished(
    size_t index,
    V8ResultHolder* v8_result,
    scoped_refptr<SerializedScriptValue> serialized_result,
    bool failed) {
  DCHECK(v8_result || serialized_result);
  DCHECK(!v8_result || !serialized_result);

  MutexLocker lock(mutex_);
  DCHECK(state_ == State::kPending || state_ == State::kCancelPending);
  DCHECK_GT(prerequisites_remaining_, 0u);
  prerequisites_remaining_--;
  if (failed)
    AdvanceState(State::kCancelPending);
  arguments_[index].v8_value = v8_result;
  arguments_[index].serialized_value = serialized_result;
  MaybeStartTask();
}

void TaskBase::MaybeStartTask() {
  if (prerequisites_remaining_)
    return;
  DCHECK(state_ == State::kPending || state_ == State::kCancelPending);
  PostCrossThreadTask(*worker_thread_->GetTaskRunner(task_type_), FROM_HERE,
                      CrossThreadBind(&TaskBase::StartTaskOnWorkerThread,
                                      WrapCrossThreadPersistent(this)));
}

bool TaskBase::WillStartTaskOnWorkerThread() {
  DCHECK(worker_thread_->IsCurrentThread());
  {
    MutexLocker lock(mutex_);
    DCHECK(!prerequisites_remaining_);
    switch (state_) {
      case State::kPending:
        AdvanceState(State::kStarted);
        break;
      case State::kCancelPending:
        return false;
      case State::kStarted:
      case State::kCompleted:
      case State::kFailed:
        NOTREACHED();
        break;
    }
  }
  return true;
}

void TaskBase::TaskCompletedOnWorkerThread(v8::Local<v8::Value> v8_result,
                                           State state) {
  DCHECK(worker_thread_->IsCurrentThread());
  v8_result_ =
      new V8ResultHolder(ToIsolate(worker_thread_->GlobalScope()), v8_result);
  function_ = nullptr;
  arguments_.clear();

  Vector<CrossThreadPersistent<Dependent>> dependents_to_notify;
  {
    MutexLocker lock(mutex_);
    AdvanceState(state);
    dependents_to_notify.swap(dependents_);
  }

  for (auto& dependent : dependents_to_notify)
    PassResultToDependentOnWorkerThread(dependent->index, dependent->task);

  PostCrossThreadTask(
      *worker_thread_->GetParentExecutionContextTaskRunners()->Get(
          TaskType::kInternalWorker),
      FROM_HERE,
      CrossThreadBind(&TaskBase::TaskCompleted, WrapCrossThreadPersistent(this),
                      state == State::kCompleted));
}

void TaskBase::RunTaskOnWorkerThread() {
  DCHECK(worker_thread_->IsCurrentThread());
  // No other thread should be touching function_ or arguments_ at this point,
  // so no mutex needed while actually running the task.
  WorkerOrWorkletGlobalScope* global_scope = worker_thread_->GlobalScope();
  ScriptState::Scope scope(global_scope->ScriptController()->GetScriptState());
  v8::Isolate* isolate = ToIsolate(global_scope);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Function> script_function;
  v8::Local<v8::Value> receiver;
  if (function_) {
    String core_script =
        "(" + ToCoreString(function_->Deserialize(isolate).As<v8::String>()) +
        ")";
    v8::MaybeLocal<v8::Script> script =
        v8::Script::Compile(context, V8String(isolate, core_script));
    script_function = script.ToLocalChecked()
                          ->Run(context)
                          .ToLocalChecked()
                          .As<v8::Function>();
    receiver = script_function;
  } else if (worker_thread_->IsWorklet()) {
    TaskWorkletGlobalScope* task_worklet_global_scope =
        static_cast<TaskWorkletGlobalScope*>(global_scope);
    script_function = task_worklet_global_scope->GetProcessFunctionForName(
        function_name_, isolate);
    receiver =
        task_worklet_global_scope->GetInstanceForName(function_name_, isolate);
  }

  if (script_function.IsEmpty()) {
    TaskCompletedOnWorkerThread(V8String(isolate, "Invalid task"),
                                State::kFailed);
    return;
  }

  Vector<v8::Local<v8::Value>> params(arguments_.size());
  for (size_t i = 0; i < arguments_.size(); i++) {
    DCHECK(!arguments_[i].serialized_value || !arguments_[i].v8_value);
    if (arguments_[i].serialized_value)
      params[i] = arguments_[i].serialized_value->Deserialize(isolate);
    else
      params[i] = arguments_[i].v8_value->GetResult(isolate);
  }

  v8::TryCatch block(isolate);
  v8::MaybeLocal<v8::Value> ret =
      script_function->Call(context, receiver, params.size(), params.data());
  if (block.HasCaught()) {
    TaskCompletedOnWorkerThread(block.Exception()->ToString(isolate),
                                State::kFailed);
    return;
  }

  DCHECK(!ret.IsEmpty());
  v8::Local<v8::Value> return_value = ret.ToLocalChecked();
  if (return_value->IsPromise()) {
    v8::Local<v8::Promise> promise = return_value.As<v8::Promise>();
    if (promise->State() == v8::Promise::kPending) {
      // Wait for the promise to resolve before calling
      // TaskCompletedOnWorkerThread.
      ScriptState* script_state = ScriptState::Current(isolate);
      ScriptPromise(script_state, promise)
          .Then(AsyncFunctionCompleted::CreateFunction(script_state, this,
                                                       State::kCompleted),
                AsyncFunctionCompleted::CreateFunction(script_state, this,
                                                       State::kFailed));
      return;
    }
    return_value = promise->Result();
  }
  TaskCompletedOnWorkerThread(return_value, State::kCompleted);
}

void TaskBase::TaskCompleted(bool was_successful) {
  DCHECK(IsMainThread());
  worker_thread_->DecrementTasksInProgressCount();
  self_keep_alive_.Clear();
}

void TaskBase::AdvanceState(State new_state) {
  switch (new_state) {
    case State::kPending:
      NOTREACHED() << "kPending should only be set via initialization";
      break;
    case State::kStarted:
      DCHECK_EQ(State::kPending, state_);
      break;
    case State::kCancelPending:
      DCHECK(state_ == State::kPending || state_ == State::kCancelPending);
      break;
    case State::kCompleted:
      DCHECK_EQ(State::kStarted, state_);
      break;
    case State::kFailed:
      DCHECK(state_ == State::kStarted || state_ == State::kCancelPending);
      break;
  }
  state_ = new_state;
}

Vector<ScriptValue> GetResolverArgument(ScriptState* script_state, Task* task) {
  v8::Isolate* isolate = script_state->GetIsolate();
  return Vector<ScriptValue>({ScriptValue(
      script_state,
      ToV8(task, isolate->GetCurrentContext()->Global(), isolate))});
}

ScriptPromise Task::result(ScriptState* script_state) {
  DCHECK(IsMainThread());
  if (!resolve_task_) {
    resolve_task_ =
        MakeGarbageCollected<ResolveTask>(script_state, task_type_, this);
  }
  return resolve_task_->GetPromise();
}

void Task::cancel() {
  DCHECK(IsMainThread());
  MutexLocker lock(mutex_);
  if (state_ == State::kPending)
    AdvanceState(State::kCancelPending);
}

void Task::StartTaskOnWorkerThread() {
  DCHECK(worker_thread_->IsCurrentThread());
  if (!WillStartTaskOnWorkerThread()) {
    WorkerOrWorkletGlobalScope* global_scope = worker_thread_->GlobalScope();
    v8::Isolate* isolate = ToIsolate(global_scope);
    ScriptState::Scope scope(
        global_scope->ScriptController()->GetScriptState());
    TaskCompletedOnWorkerThread(V8String(isolate, "Task aborted"),
                                State::kFailed);
    return;
  }

  RunTaskOnWorkerThread();
}

void Task::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  TaskBase::Trace(visitor);
  visitor->Trace(resolve_task_);
}

ResolveTask::ResolveTask(ScriptState* script_state,
                         TaskType task_type,
                         Task* prerequisite)
    : TaskBase(task_type, script_state, ScriptValue(), String()),
      resolver_(ScriptPromiseResolver::Create(script_state)) {
  DCHECK(IsMainThread());
  // It's safe to pass a nullptr ThreadPoolThreadProivder here because it
  // is only used to select a thread if there are no prerequisites, but a
  // ResolveTask always has exactly one prerequisite.
  InitializeArgumentsOnMainThread(
      nullptr, script_state, GetResolverArgument(script_state, prerequisite));
}

void ResolveTask::StartTaskOnWorkerThread() {
  // Just take the sole argument and use it as the return value that will be
  // given to the promise resolver.
  {
    MutexLocker lock(mutex_);
    serialized_result_ = arguments_[0].serialized_value;
  }
  TaskCompletedOnWorkerThread(
      v8::Local<v8::Value>(),
      WillStartTaskOnWorkerThread() ? State::kCompleted : State::kFailed);
}

void ResolveTask::TaskCompleted(bool was_successful) {
  DCHECK(IsMainThread());

  ScriptState* script_state = resolver_->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  v8::Local<v8::Value> value =
      GetSerializedResult()->Deserialize(script_state->GetIsolate());
  if (was_successful)
    resolver_->Resolve(value);
  else
    resolver_->Reject(v8::Exception::Error(value.As<v8::String>()));
  TaskBase::TaskCompleted(was_successful);
}

void ResolveTask::Trace(Visitor* visitor) {
  TaskBase::Trace(visitor);
  visitor->Trace(resolver_);
}

}  // namespace blink
