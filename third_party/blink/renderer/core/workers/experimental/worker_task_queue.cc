// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/experimental/worker_task_queue.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/workers/experimental/task.h"
#include "third_party/blink/renderer/core/workers/experimental/thread_pool.h"

namespace blink {

WorkerTaskQueue* WorkerTaskQueue::Create(ExecutionContext* context,
                                         const String& type,
                                         ExceptionState& exception_state) {
  if (context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The context provided is invalid.");
    return nullptr;
  }

  auto* document = DynamicTo<Document>(context);
  if (!document) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "WorkerTaskQueue can only be constructed from a document.");
    return nullptr;
  }

  DCHECK(type == "user-interaction" || type == "background");
  TaskType task_type = type == "user-interaction" ? TaskType::kUserInteraction
                                                  : TaskType::kIdleTask;
  return new WorkerTaskQueue(document, task_type);
}

WorkerTaskQueue::WorkerTaskQueue(Document* document, TaskType task_type)
    : document_(document), task_type_(task_type) {}

ScriptPromise WorkerTaskQueue::postFunction(
    ScriptState* script_state,
    const ScriptValue& task,
    AbortSignal* signal,
    const Vector<ScriptValue>& arguments) {
  DCHECK(document_->IsContextThread());
  DCHECK(task.IsFunction());
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  scoped_refptr<SerializedScriptValue> serialized_task =
      SerializedScriptValue::SerializeAndSwallowExceptions(
          script_state->GetIsolate(),
          task.V8Value()->ToString(script_state->GetIsolate()));
  if (!serialized_task) {
    resolver->Reject();
    return resolver->Promise();
  }

  Vector<scoped_refptr<SerializedScriptValue>> serialized_arguments;
  serialized_arguments.ReserveInitialCapacity(arguments.size());
  for (auto& argument : arguments) {
    scoped_refptr<SerializedScriptValue> serialized_argument =
        SerializedScriptValue::SerializeAndSwallowExceptions(
            script_state->GetIsolate(), argument.V8Value());
    if (!serialized_argument) {
      resolver->Reject();
      return resolver->Promise();
    }
    serialized_arguments.push_back(serialized_argument);
  }

  ThreadPool::From(*document_)
      ->PostTask(std::move(serialized_task), resolver, signal,
                 std::move(serialized_arguments), task_type_);
  return resolver->Promise();
}

Task* WorkerTaskQueue::postTask(ScriptState* script_state,
                                const ScriptValue& function,
                                const Vector<ScriptValue>& arguments) {
  DCHECK(document_->IsContextThread());
  DCHECK(function.IsFunction());

  ThreadPoolTask* thread_pool_task =
      new ThreadPoolTask(ThreadPool::From(*document_),
                         script_state->GetIsolate(), function, arguments);
  return new Task(thread_pool_task);
}

void WorkerTaskQueue::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(document_);
}

}  // namespace blink
