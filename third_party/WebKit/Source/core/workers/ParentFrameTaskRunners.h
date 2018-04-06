// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ParentFrameTaskRunners_h
#define ParentFrameTaskRunners_h

#include <memory>
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/TaskTypeTraits.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"

namespace blink {

// Represents a set of task runners of the parent execution context, or default
// task runners for the current thread if no execution context is available.
// TODO(japhet): Rename to something like ParentExecutionContextTaskRunners.
class CORE_EXPORT ParentFrameTaskRunners final
    : public GarbageCollectedFinalized<ParentFrameTaskRunners>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ParentFrameTaskRunners);

 public:
  // Returns task runners associated with a given context. This must be called
  // on the context's context thread, that is, the thread where the context was
  // created.
  static ParentFrameTaskRunners* Create(ExecutionContext*);

  // Returns default task runners of the current thread. This can be called from
  // any threads. This must be used only for shared workers, service workers and
  // tests that don't have a parent frame.
  static ParentFrameTaskRunners* Create();

  // Might return nullptr for unsupported task types. This can be called from
  // any threads.
  scoped_refptr<base::SingleThreadTaskRunner> Get(TaskType)
      LOCKS_EXCLUDED(mutex_);

  void Trace(blink::Visitor*) override;

 private:
  using TaskRunnerHashMap = HashMap<TaskType,
                                    scoped_refptr<base::SingleThreadTaskRunner>,
                                    WTF::IntHash<TaskType>,
                                    TaskTypeTraits>;

  // ExecutionContext could be nullptr if the worker is not associated with a
  // particular context.
  explicit ParentFrameTaskRunners(ExecutionContext*);

  void ContextDestroyed(ExecutionContext*) LOCKS_EXCLUDED(mutex_) override;

  Mutex mutex_;
  TaskRunnerHashMap task_runners_ GUARDED_BY(mutex_);

  DISALLOW_COPY_AND_ASSIGN(ParentFrameTaskRunners);
};

}  // namespace blink

#endif  // ParentFrameTaskRunners_h
