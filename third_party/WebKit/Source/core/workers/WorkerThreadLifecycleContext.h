// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerThreadLifecycleContext_h
#define WorkerThreadLifecycleContext_h

#include "core/CoreExport.h"
#include "platform/LifecycleNotifier.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class WorkerThreadLifecycleObserver;

// Used for notifying observers on the main thread of worker thread termination.
// The lifetime of this class is equal to that of WorkerThread. Created and
// destructed on the main thread.
class CORE_EXPORT WorkerThreadLifecycleContext final
    : public GarbageCollectedFinalized<WorkerThreadLifecycleContext>,
      public LifecycleNotifier<WorkerThreadLifecycleContext,
                               WorkerThreadLifecycleObserver> {
  USING_GARBAGE_COLLECTED_MIXIN(WorkerThreadLifecycleContext);
  WTF_MAKE_NONCOPYABLE(WorkerThreadLifecycleContext);

 public:
  WorkerThreadLifecycleContext();
  ~WorkerThreadLifecycleContext() override;
  void NotifyContextDestroyed() override;

 private:
  friend class WorkerThreadLifecycleObserver;
  bool was_context_destroyed_ = false;
};

}  // namespace blink

#endif  // WorkerThreadLifecycleContext_h
