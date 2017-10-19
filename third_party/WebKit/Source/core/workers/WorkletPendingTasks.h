// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletPendingTasks_h
#define WorkletPendingTasks_h

#include "core/CoreExport.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "platform/heap/Heap.h"

namespace blink {

// Implementation of the "pending tasks struct":
// https://drafts.css-houdini.org/worklets/#pending-tasks-struct
//
// This also implements a part of the "fetch and invoke a worklet script"
// algorithm:
// https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
//
// All functions must be accessed on the main thread.
class CORE_EXPORT WorkletPendingTasks final
    : public GarbageCollected<WorkletPendingTasks> {
 public:
  WorkletPendingTasks(int counter, ScriptPromiseResolver*);

  // Sets |counter_| to -1 and rejects the promise.
  void Abort();

  // Decrements |counter_| and resolves the promise if the counter becomes 0.
  void DecrementCounter();

  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(resolver_); }

 private:
  // The number of pending tasks. -1 indicates these tasks are aborted and
  // |resolver_| already rejected the promise.
  int counter_;

  Member<ScriptPromiseResolver> resolver_;
};

}  // namespace blink

#endif  // WorkletPendingTasks_h
