// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WORKER_RESOURCE_TIMING_NOTIFIER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WORKER_RESOURCE_TIMING_NOTIFIER_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/worker_resource_timing_notifier.h"

namespace blink {

class ExecutionContext;

// The implementation of WorkerResourceTimingNotifier that dispatches resource
// timing info to an execution context which is associated with the instance of
// this class. Thread safety: For the ctor and dtor, these must be called on the
// sequence of the execution context. For AddResourceTiming(), it can be called
// on a different sequence from the sequence of the execution context. In that
// case, this creates a copy of the given resource timing and passes it to the
// execution context's sequence via PostCrossThreadTask.
class CORE_EXPORT WorkerResourceTimingNotifierImpl final
    : public WorkerResourceTimingNotifier {
 public:
  WorkerResourceTimingNotifierImpl();
  explicit WorkerResourceTimingNotifierImpl(ExecutionContext&);
  ~WorkerResourceTimingNotifierImpl() override = default;

  void AddResourceTiming(const WebResourceTimingInfo&,
                         const AtomicString& initiator_type) override;

 private:
  void AddCrossThreadResourceTiming(const WebResourceTimingInfo&,
                                    const String& initiator_type);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  CrossThreadWeakPersistent<ExecutionContext> execution_context_;

  DISALLOW_COPY_AND_ASSIGN(WorkerResourceTimingNotifierImpl);
};

// NullWorkerResourceTimingNotifier does nothing when AddResourceTiming() is
// called. This is used for toplevel shared/service worker script fetch.
class CORE_EXPORT NullWorkerResourceTimingNotifier final
    : public WorkerResourceTimingNotifier {
 public:
  NullWorkerResourceTimingNotifier() = default;
  ~NullWorkerResourceTimingNotifier() override = default;

  void AddResourceTiming(const WebResourceTimingInfo&,
                         const AtomicString& initiator_type) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NullWorkerResourceTimingNotifier);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WORKER_RESOURCE_TIMING_NOTIFIER_IMPL_H_
