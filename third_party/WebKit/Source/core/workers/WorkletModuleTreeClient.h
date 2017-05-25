// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Modulator.h"
#include "core/workers/WorkletPendingTasks.h"
#include "platform/heap/GarbageCollected.h"

namespace blink {

class ModuleScript;

class WorkletModuleTreeClient final
    : public GarbageCollectedFinalized<WorkletModuleTreeClient>,
      public ModuleTreeClient {
  USING_GARBAGE_COLLECTED_MIXIN(WorkletModuleTreeClient);

 public:
  WorkletModuleTreeClient(Modulator*, WorkletPendingTasks*);

  // Implements ModuleTreeClient.
  void NotifyModuleTreeLoadFinished(ModuleScript*) final;

  DECLARE_VIRTUAL_TRACE();

 private:
  Member<Modulator> modulator_;
  Member<WorkletPendingTasks> pending_tasks_;
};

}  // namespace blink
