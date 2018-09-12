// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_THREAD_POOL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_THREAD_POOL_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class AbortSignal;
class Document;
class SerializedScriptValue;
class ThreadPoolMessagingProxy;

class ThreadPool final : public GarbageCollected<ThreadPool>,
                         public Supplement<Document> {
  USING_GARBAGE_COLLECTED_MIXIN(ThreadPool);

 public:
  static const char kSupplementName[];

  static ThreadPool* From(Document&);

  void PostTask(scoped_refptr<SerializedScriptValue> task,
                ScriptPromiseResolver*,
                AbortSignal*,
                const Vector<scoped_refptr<SerializedScriptValue>>& arguments,
                TaskType);

  void Trace(blink::Visitor*) final;

 private:
  ThreadPool(Document&);
  ~ThreadPool() = default;

  friend ThreadPoolMessagingProxy;
  ThreadPoolMessagingProxy* GetProxyForTaskType(TaskType);
  void TaskCompleted(size_t task_id,
                     bool was_rejected,
                     scoped_refptr<SerializedScriptValue> result);
  void AbortTask(size_t task_id, TaskType task_type);

  Member<Document> document_;
  HeapVector<Member<ThreadPoolMessagingProxy>> context_proxies_;
  size_t next_task_id_ = 1;
  HeapHashMap<int, Member<ScriptPromiseResolver>> resolvers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_THREAD_POOL_H_
