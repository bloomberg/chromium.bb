// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletModuleTreeClient_h
#define WorkletModuleTreeClient_h

#include "core/script/Modulator.h"
#include "core/workers/WorkletPendingTasks.h"
#include "platform/WebTaskRunner.h"
#include "platform/heap/GarbageCollected.h"

namespace blink {

class ModuleScript;

// A ModuleTreeClient that lives on the worklet context's thread.
class WorkletModuleTreeClient final : public ModuleTreeClient {
 public:
  WorkletModuleTreeClient(
      Modulator*,
      scoped_refptr<WebTaskRunner> outside_settings_task_runner,
      WorkletPendingTasks*);

  // Implements ModuleTreeClient.
  void NotifyModuleTreeLoadFinished(ModuleScript*) final;

  void Trace(blink::Visitor*) override;

 private:
  Member<Modulator> modulator_;
  scoped_refptr<WebTaskRunner> outside_settings_task_runner_;
  CrossThreadPersistent<WorkletPendingTasks> pending_tasks_;
};

}  // namespace blink

#endif  // WorkletModuleTreeClient_h
