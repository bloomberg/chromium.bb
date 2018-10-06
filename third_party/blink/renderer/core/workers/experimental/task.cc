// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/experimental/task.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_task.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/workers/experimental/thread_pool.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"

namespace blink {

ThreadPoolTask::ThreadPoolTask(ThreadPool* thread_pool,
                               v8::Isolate* isolate,
                               const ScriptValue& function,
                               const Vector<ScriptValue>& arguments)
    : self_keep_alive_(base::AdoptRef(this)), arguments_(arguments.size()) {
  DCHECK(IsMainThread());

  // TODO(japhet): Handle serialization failures
  function_ = SerializedScriptValue::SerializeAndSwallowExceptions(
      isolate, function.V8Value()->ToString(isolate));

  Vector<ThreadPoolTask*> prerequisites;
  Vector<size_t> prerequisites_indices;
  for (size_t i = 0; i < arguments_.size(); i++) {
    // Normal case: if the argument isn't a Task, just serialize it.
    if (!V8Task::hasInstance(arguments[i].V8Value(), isolate)) {
      arguments_[i].serialized_value =
          SerializedScriptValue::SerializeAndSwallowExceptions(
              isolate, arguments[i].V8Value());
      continue;
    }
    ThreadPoolTask* prerequisite =
        ToScriptWrappable(arguments[i].V8Value().As<v8::Object>())
            ->ToImpl<Task>()
            ->GetThreadPoolTask();
    prerequisites.push_back(prerequisite);
    prerequisites_indices.push_back(i);
  }

  worker_thread_ = SelectThread(prerequisites, thread_pool);
  worker_thread_->IncrementTasksInProgressCount();

  if (prerequisites.IsEmpty()) {
    MaybeStartTask();
    return;
  }

  // Other ThreadPoolTask instances don't have a reference to |this| yet, so
  // no need to lock mutex_. RegisterDependencies() populates those references,
  // so any logic after this point must consider the potential for data races.
  RegisterDependencies(prerequisites, prerequisites_indices);
}

// static
ThreadPoolThread* ThreadPoolTask::SelectThread(
    const Vector<ThreadPoolTask*>& prerequisites,
    ThreadPool* thread_pool) {
  DCHECK(IsMainThread());
  HashCountedSet<WorkerThread*> prerequisite_location_counts;
  size_t max_prerequisite_location_count = 0;
  ThreadPoolThread* max_prerequisite_thread = nullptr;
  for (ThreadPoolTask* prerequisite : prerequisites) {
    // For prerequisites that are not yet complete, track which thread the task
    // will run on. Put this task on the thread where the most prerequisites
    // reside. This is slightly imprecise, because a task may complete before
    // registering dependent tasks below.
    if (ThreadPoolThread* thread = prerequisite->GetScheduledThread()) {
      prerequisite_location_counts.insert(thread);
      unsigned thread_count = prerequisite_location_counts.count(thread);
      if (thread_count > max_prerequisite_location_count) {
        max_prerequisite_location_count = thread_count;
        max_prerequisite_thread = thread;
      }
    }
  }
  return max_prerequisite_thread ? max_prerequisite_thread
                                 : thread_pool->GetLeastBusyThread();
}

ThreadPoolThread* ThreadPoolTask::GetScheduledThread() {
  DCHECK(IsMainThread());
  MutexLocker lock(mutex_);
  return state_ == State::kCompleted ? nullptr : worker_thread_;
}

// Should only be called from constructor. Split out in to a helper because
// clang appears to exempt constructors from thread safety analysis.
void ThreadPoolTask::RegisterDependencies(
    const Vector<ThreadPoolTask*>& prerequisites,
    const Vector<size_t>& prerequisites_indices) {
  DCHECK(IsMainThread());
  {
    MutexLocker lock(mutex_);
    prerequisites_remaining_ = prerequisites.size();
  }

  for (size_t i = 0; i < prerequisites.size(); i++) {
    ThreadPoolTask* prerequisite = prerequisites[i];
    size_t prerequisite_index = prerequisites_indices[i];
    scoped_refptr<SerializedScriptValue> result =
        prerequisite->RegisterDependencyIfNotComplete(this, prerequisite_index);
    if (result)
      PrerequisiteFinished(prerequisite_index, v8::Local<v8::Value>(), result);
  }
}

scoped_refptr<SerializedScriptValue>
ThreadPoolTask::RegisterDependencyIfNotComplete(ThreadPoolTask* dependent,
                                                size_t index) {
  DCHECK(IsMainThread());
  MutexLocker lock(mutex_);
  if (state_ == State::kCompleted)
    return serialized_result_;
  dependents_.insert(std::make_unique<Dependent>(dependent, index));
  return nullptr;
}

ThreadPoolTask::~ThreadPoolTask() {
  DCHECK(IsMainThread());
  DCHECK(!resolver_);
  DCHECK_EQ(State::kCompleted, state_);
  DCHECK(!function_);
  DCHECK(arguments_.IsEmpty());
  DCHECK(!prerequisites_remaining_);
  DCHECK(dependents_.IsEmpty());
}

void ThreadPoolTask::PrerequisiteFinished(
    size_t prerequisite_index,
    v8::Local<v8::Value> v8_result,
    scoped_refptr<SerializedScriptValue> result) {
  MutexLocker lock(mutex_);
  DCHECK(state_ == State::kPending || state_ == State::kCancelled);
  DCHECK_GT(prerequisites_remaining_, 0u);
  prerequisites_remaining_--;
  // If the result of the prerequisite doesn't need to move between threads,
  // save the deserialized v8::Value for later use.
  if (worker_thread_->IsCurrentThread() && !v8_result.IsEmpty()) {
    arguments_[prerequisite_index].v8_value =
        std::make_unique<ScopedPersistent<v8::Value>>(
            ToIsolate(worker_thread_->GlobalScope()), v8_result);
  } else {
    arguments_[prerequisite_index].serialized_value = result;
  }
  MaybeStartTask();
}

void ThreadPoolTask::MaybeStartTask() {
  if (prerequisites_remaining_)
    return;
  DCHECK(state_ == State::kPending || state_ == State::kCancelled);
  PostCrossThreadTask(*worker_thread_->GetTaskRunner(TaskType::kInternalWorker),
                      FROM_HERE,
                      CrossThreadBind(&ThreadPoolTask::StartTaskOnWorkerThread,
                                      CrossThreadUnretained(this)));
}

void ThreadPoolTask::StartTaskOnWorkerThread() {
  DCHECK(worker_thread_->IsCurrentThread());

  bool was_cancelled = false;
  {
    MutexLocker lock(mutex_);
    DCHECK(!prerequisites_remaining_);
    switch (state_) {
      case State::kPending:
        state_ = State::kStarted;
        break;
      case State::kCancelled:
        was_cancelled = true;
        break;
      case State::kStarted:
      case State::kCompleted:
        NOTREACHED();
        break;
    }
  }

  WorkerOrWorkletGlobalScope* global_scope = worker_thread_->GlobalScope();
  v8::Isolate* isolate = ToIsolate(global_scope);
  ScriptState::Scope scope(global_scope->ScriptController()->GetScriptState());

  scoped_refptr<SerializedScriptValue> local_result;
  v8::Local<v8::Value> return_value;
  if (was_cancelled) {
    local_result = SerializedScriptValue::Create();
  } else {
    return_value = RunTaskOnWorkerThread(isolate);
    local_result = SerializedScriptValue::SerializeAndSwallowExceptions(
        isolate, return_value);
  }

  function_ = nullptr;
  arguments_.clear();

  HashSet<std::unique_ptr<Dependent>> dependents_to_notify;
  {
    MutexLocker lock(mutex_);
    serialized_result_ = local_result;
    state_ = State::kCompleted;
    dependents_to_notify.swap(dependents_);
  }

  for (auto& dependent : dependents_to_notify) {
    dependent->task->PrerequisiteFinished(dependent->index, return_value,
                                          local_result);
  }

  PostCrossThreadTask(
      *worker_thread_->GetParentExecutionContextTaskRunners()->Get(
          TaskType::kInternalWorker),
      FROM_HERE,
      CrossThreadBind(&ThreadPoolTask::TaskCompleted,
                      CrossThreadUnretained(this)));
  // TaskCompleted maye delete |this| at any time after this point.
}

v8::Local<v8::Value> ThreadPoolTask::RunTaskOnWorkerThread(
    v8::Isolate* isolate) {
  DCHECK(worker_thread_->IsCurrentThread());
  // No other thread should be touching function_ or arguments_ at this point,
  // so no mutex needed while actually running the task.
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  String core_script =
      "(" + ToCoreString(function_->Deserialize(isolate).As<v8::String>()) +
      ")";
  v8::MaybeLocal<v8::Script> script = v8::Script::Compile(
      isolate->GetCurrentContext(), V8String(isolate, core_script));
  v8::Local<v8::Function> script_function =
      script.ToLocalChecked()->Run(context).ToLocalChecked().As<v8::Function>();

  Vector<v8::Local<v8::Value>> params(arguments_.size());
  for (size_t i = 0; i < arguments_.size(); i++) {
    DCHECK(!arguments_[i].serialized_value || !arguments_[i].v8_value);
    if (arguments_[i].serialized_value)
      params[i] = arguments_[i].serialized_value->Deserialize(isolate);
    else
      params[i] = arguments_[i].v8_value->NewLocal(isolate);
  }

  v8::TryCatch block(isolate);
  v8::MaybeLocal<v8::Value> ret = script_function->Call(
      context, script_function, params.size(), params.data());
  DCHECK_EQ(ret.IsEmpty(), block.HasCaught());

  v8::Local<v8::Value> return_value;
  if (!ret.IsEmpty()) {
    return_value = ret.ToLocalChecked();
    if (return_value->IsPromise())
      return_value = return_value.As<v8::Promise>()->Result();
  } else {
    return_value = block.Exception()->ToString(isolate);
  }
  return return_value;
}

void ThreadPoolTask::TaskCompleted() {
  DCHECK(IsMainThread());
#if DCHECK_IS_ON
  {
    MutexLocker lock(mutex_);
    DCHECK_EQ(State::kCompleted, state_);
  }
#endif
  if (resolver_ && resolver_->GetScriptState()->ContextIsValid()) {
    resolver_->Resolve(GetResult(resolver_->GetScriptState()));
    resolver_ = nullptr;
  }
  worker_thread_->DecrementTasksInProgressCount();
  self_keep_alive_.reset();
  // |this| may be deleted here.
}

ScriptValue ThreadPoolTask::GetResult(ScriptState* script_state) {
  DCHECK(IsMainThread());
  MutexLocker lock(mutex_);
  if (state_ != State::kCompleted) {
    DCHECK(!serialized_result_);
    DCHECK(deserialized_result_.IsEmpty());
    if (!resolver_)
      resolver_ = ScriptPromiseResolver::Create(script_state);
    return resolver_->Promise().GetScriptValue();
  }
  if (deserialized_result_.IsEmpty()) {
    ScriptState::Scope scope(script_state);
    deserialized_result_ = ScriptValue(
        script_state,
        serialized_result_->Deserialize(script_state->GetIsolate()));
  }
  return deserialized_result_;
}

void ThreadPoolTask::Cancel() {
  DCHECK(IsMainThread());
  MutexLocker lock(mutex_);
  if (state_ == State::kPending)
    state_ = State::kCancelled;
}

}  // namespace blink
