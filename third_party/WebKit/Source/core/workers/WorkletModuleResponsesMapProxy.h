// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletModuleResponsesMapProxy_h
#define WorkletModuleResponsesMapProxy_h

#include "core/CoreExport.h"
#include "core/workers/WorkletModuleResponsesMap.h"
#include "platform/WebTaskRunner.h"
#include "platform/heap/Heap.h"

namespace blink {

// WorkletModuleResponsesMapProxy serves as a proxy to talk to
// WorkletModuleResponsesMap on the main thread (outside_settings) from
// WorkletGlobalScope on the worklet context thread (inside_settings). The
// constructor and all public functions must be called on the worklet context
// thread.
class CORE_EXPORT WorkletModuleResponsesMapProxy
    : public GarbageCollectedFinalized<WorkletModuleResponsesMapProxy> {
 public:
  using Client = WorkletModuleResponsesMap::Client;

  static WorkletModuleResponsesMapProxy* Create(
      WorkletModuleResponsesMap*,
      scoped_refptr<WebTaskRunner> outside_settings_task_runner,
      scoped_refptr<WebTaskRunner> inside_settings_task_runner);

  void ReadEntry(const FetchParameters&, Client*);

  void Trace(blink::Visitor*);

 private:
  WorkletModuleResponsesMapProxy(
      WorkletModuleResponsesMap*,
      scoped_refptr<WebTaskRunner> outside_settings_task_runner,
      scoped_refptr<WebTaskRunner> inside_settings_task_runner);

  void ReadEntryOnMainThread(std::unique_ptr<CrossThreadFetchParametersData>,
                             Client*);

  CrossThreadPersistent<WorkletModuleResponsesMap> module_responses_map_;
  scoped_refptr<WebTaskRunner> outside_settings_task_runner_;
  scoped_refptr<WebTaskRunner> inside_settings_task_runner_;
};

}  // namespace blink

#endif  // WorkletModuleResponsesMapProxy_h
