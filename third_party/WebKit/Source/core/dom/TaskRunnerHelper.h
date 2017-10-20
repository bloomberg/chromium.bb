// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TaskRunnerHelper_h
#define TaskRunnerHelper_h

#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashTraits.h"
#include "public/platform/TaskType.h"

namespace blink {

class Document;
class ExecutionContext;
class LocalFrame;
class ScriptState;
class WebTaskRunner;
class WorkerOrWorkletGlobalScope;
class WorkerThread;

// A set of helper functions to get a WebTaskRunner for TaskType and a context
// object. The posted tasks are guaranteed to run in a sequence if they have the
// same TaskType and the context objects belong to the same frame.
class CORE_EXPORT TaskRunnerHelper final {
  STATIC_ONLY(TaskRunnerHelper);

 public:
  static scoped_refptr<WebTaskRunner> Get(TaskType, LocalFrame*);
  static scoped_refptr<WebTaskRunner> Get(TaskType, Document*);
  static scoped_refptr<WebTaskRunner> Get(TaskType, ExecutionContext*);
  static scoped_refptr<WebTaskRunner> Get(TaskType, ScriptState*);
  static scoped_refptr<WebTaskRunner> Get(TaskType,
                                          WorkerOrWorkletGlobalScope*);

  // Returns a WebTaskRunner that is associated to the worker / worklet global
  // scope that corresponds to the given WorkerThread. Note that WorkerThread is
  // a per-global-scope object while the naming might sound differently.
  // TODO(nhiroki): Rename WorkerThread to something that clarifies it's a
  // per-global-scope object.
  static scoped_refptr<WebTaskRunner> Get(TaskType, WorkerThread*);
};

}  // namespace blink

#endif
