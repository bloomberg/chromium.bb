// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ParentFrameTaskRunners_h
#define ParentFrameTaskRunners_h

#include <memory>
#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/TaskTypeTraits.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

class LocalFrame;
class WebTaskRunner;

// Represents a set of task runners of the parent (or associated) document's
// frame, or default task runners of the main thread.
//
// This observes LocalFrame lifecycle only when this is created with LocalFrame.
class CORE_EXPORT ParentFrameTaskRunners final
    : public GarbageCollectedFinalized<ParentFrameTaskRunners>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ParentFrameTaskRunners);
  WTF_MAKE_NONCOPYABLE(ParentFrameTaskRunners);

 public:
  // Returns task runners associated with a given frame. This must be called on
  // the frame's context thread, that is, the main thread. The given frame must
  // have a valid execution context.
  static ParentFrameTaskRunners* Create(LocalFrame&);

  // Returns default task runners of the main thread. This can be called from
  // any threads. This must be used only for shared workers, service workers and
  // tests that don't have a parent frame.
  static ParentFrameTaskRunners* Create();

  // Might return nullptr for unsupported task types. This can be called from
  // any threads.
  RefPtr<WebTaskRunner> Get(TaskType);

  virtual void Trace(blink::Visitor*);

 private:
  using TaskRunnerHashMap = HashMap<TaskType,
                                    RefPtr<WebTaskRunner>,
                                    WTF::IntHash<TaskType>,
                                    TaskTypeTraits>;

  // LocalFrame could be nullptr if the worker is not associated with a
  // particular local frame.
  explicit ParentFrameTaskRunners(LocalFrame*);

  void ContextDestroyed(ExecutionContext*) override;

  Mutex task_runners_mutex_;
  TaskRunnerHashMap task_runners_;
};

}  // namespace blink

#endif  // ParentFrameTaskRunners_h
