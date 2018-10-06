// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_THREAD_POOL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_THREAD_POOL_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class AbortSignal;
class Document;
class SerializedScriptValue;
class ThreadPoolMessagingProxy;
class ThreadPoolObjectProxy;
class WorkerThread;

class ThreadPoolThread final : public WorkerThread {
 public:
  ThreadPoolThread(ExecutionContext*, ThreadPoolObjectProxy&);
  ~ThreadPoolThread() override = default;

  void IncrementTasksInProgressCount() {
    DCHECK(IsMainThread());
    tasks_in_progress_++;
  }
  void DecrementTasksInProgressCount() {
    DCHECK(IsMainThread());
    DCHECK_GT(tasks_in_progress_, 0u);
    tasks_in_progress_--;
  }
  size_t GetTasksInProgressCount() const {
    DCHECK(IsMainThread());
    return tasks_in_progress_;
  }

 private:
  WorkerBackingThread& GetWorkerBackingThread() override {
    return *worker_backing_thread_;
  }
  void ClearWorkerBackingThread() override { worker_backing_thread_ = nullptr; }

  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params) override;

  WebThreadType GetThreadType() const override {
    // TODO(japhet): Replace with WebThreadType::kThreadPoolWorkerThread.
    return WebThreadType::kDedicatedWorkerThread;
  }
  std::unique_ptr<WorkerBackingThread> worker_backing_thread_;
  size_t tasks_in_progress_ = 0;
};

class ThreadPool final : public GarbageCollectedFinalized<ThreadPool>,
                         public Supplement<Document>,
                         public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ThreadPool);
  EAGERLY_FINALIZE();

 public:
  static const char kSupplementName[];

  static ThreadPool* From(Document&);
  ~ThreadPool();

  void PostTask(scoped_refptr<SerializedScriptValue> task,
                ScriptPromiseResolver*,
                AbortSignal*,
                const Vector<scoped_refptr<SerializedScriptValue>>& arguments,
                TaskType);

  ThreadPoolThread* GetLeastBusyThread();

  void ContextDestroyed(ExecutionContext*) override;

  void Trace(blink::Visitor*) final;

 private:
  ThreadPool(Document&);

  friend ThreadPoolMessagingProxy;
  ThreadPoolMessagingProxy* GetProxyForTaskType(TaskType);
  void CreateProxyAtId(size_t proxy_id);

  void TaskCompleted(size_t task_id,
                     bool was_rejected,
                     scoped_refptr<SerializedScriptValue> result);
  void AbortTask(size_t task_id, TaskType task_type);

  HeapVector<Member<ThreadPoolMessagingProxy>> context_proxies_;
  size_t next_task_id_ = 1;
  HeapHashMap<int, Member<ScriptPromiseResolver>> resolvers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_THREAD_POOL_H_
