// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_TASK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_TASK_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/workers/experimental/thread_pool_thread.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {
class SerializedScriptValue;

// Runs |function| with |arguments| on a thread from the given ThreadPool.
// Scans |arguments| for Task objects, and registers those as dependencies,
// passing the result of those tasks in place of the Task arguments.
// All public functions are main-thread-only.
// Task keeps itself alive via a SelfKeepAlive until the
// the task completes and reports itself done on the main thread via
// TaskCompleted().
class Task final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Called on main thread
  Task(ThreadPoolThreadProvider*,
       ScriptState*,
       const ScriptValue& function,
       const Vector<ScriptValue>& arguments,
       TaskType);
  Task(ThreadPoolThreadProvider*,
       ScriptState*,
       const String& function_name,
       const Vector<ScriptValue>& arguments,
       TaskType);
  ~Task() override;
  // Returns a promise that will be resolved with the result when it completes.
  ScriptPromise result();
  void cancel() LOCKS_EXCLUDED(mutex_);

  void Trace(Visitor*) override;

 private:
  enum class State { kPending, kStarted, kCancelPending, kCompleted, kFailed };

  Task(ThreadPoolThreadProvider*,
       ScriptState*,
       const ScriptValue& function,
       const String& function_name,
       const Vector<ScriptValue>& arguments,
       TaskType);

  class AsyncFunctionCompleted;

  void StartTaskOnWorkerThread() LOCKS_EXCLUDED(mutex_);
  void RunTaskOnWorkerThread();
  void TaskCompletedOnWorkerThread(v8::Local<v8::Value> return_value, State)
      LOCKS_EXCLUDED(mutex_);

  // Called on ANY thread (main thread, worker_thread_, or a sibling worker).
  void MaybeStartTask() EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void PrerequisiteFinished(size_t prerequisite_index,
                            v8::Local<v8::Value>,
                            scoped_refptr<SerializedScriptValue>,
                            State) LOCKS_EXCLUDED(mutex_);
  bool HasFinished() const EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return state_ == State::kCompleted || state_ == State::kFailed;
  }
  void AdvanceState(State new_state) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Called on main thread
  static ThreadPoolThread* SelectThread(
      const HeapVector<Member<Task>>& prerequisites,
      ThreadPoolThreadProvider*);
  ThreadPoolThread* GetScheduledThread() LOCKS_EXCLUDED(mutex_);
  void RegisterDependencies(const HeapVector<Member<Task>>& prerequisites,
                            const Vector<size_t>& prerequisite_indices)
      LOCKS_EXCLUDED(mutex_);
  void TaskCompleted();

  // worker_thread_ is selected in the constructor and not changed thereafter.
  ThreadPoolThread* worker_thread_ = nullptr;
  const TaskType task_type_;

  // Main thread only
  SelfKeepAlive<Task> self_keep_alive_;
  Member<ScriptPromiseResolver> resolver_;

  // Created in constructor on the main thread, consumed and cleared on
  // worker_thread_. Those steps can't overlap, so no mutex_ required.
  scoped_refptr<SerializedScriptValue> function_;
  const String function_name_;

  // Created and populated with non-prerequiste parameters on the main thread.
  // Each prerequisite writes its return value into arguments_ from its thread.
  // If the prequisite and this have the same worker_thread_, there is no need
  // to serialize and deserialize the argument, so v8_argument will be populated
  // with the v8::Value returned by the prerequisite.
  // Consumed and cleared on worker_thread_.
  // Only requires mutex_ when writing prerequisite results, at other times
  // either the main thread or the worker_thread_ has sole access.
  struct Argument {
    scoped_refptr<SerializedScriptValue> serialized_value;
    std::unique_ptr<ScopedPersistent<v8::Value>> v8_value;
  };
  Vector<Argument> arguments_;

  // Read on main thread, write on worker_thread_.
  scoped_refptr<SerializedScriptValue> serialized_result_ GUARDED_BY(mutex_);

  // Read/write on both main thread and worker_thread_.
  State state_ GUARDED_BY(mutex_) = State::kPending;

  // Initialized in constructor on main thread, each completed prerequisite
  // decrements from the prerequisite's thread or main thread.
  size_t prerequisites_remaining_ GUARDED_BY(mutex_) = 0u;

  // Elements added from main thread. Cleared on completion on worker_thread_.
  // Each element in dependents_ is not yet in the kCompleted state.
  struct Dependent final : public GarbageCollected<Dependent> {
   public:
    Dependent(Task* task, size_t index) : task(task), index(index) {
      DCHECK(IsMainThread());
    }
    void Trace(Visitor* visitor) { visitor->Trace(task); }
    Member<Task> task;
    // The index in the dependent's argument array where this result should go.
    size_t index;
  };
  Vector<CrossThreadPersistent<Dependent>> dependents_ GUARDED_BY(mutex_);

  Mutex mutex_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_TASK_H_
