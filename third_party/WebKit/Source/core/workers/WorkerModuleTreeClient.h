// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerModuleTreeClient_h
#define WorkerModuleTreeClient_h

#include "core/script/Modulator.h"
#include "platform/heap/GarbageCollected.h"

namespace blink {

class ModuleScript;

// A ModuleTreeClient that lives on the worker context's thread.
class WorkerModuleTreeClient final : public ModuleTreeClient {
 public:
  explicit WorkerModuleTreeClient(Modulator*);

  // Implements ModuleTreeClient.
  void NotifyModuleTreeLoadFinished(ModuleScript*) final;

  void Trace(blink::Visitor*) override;

 private:
  Member<Modulator> modulator_;
};

}  // namespace blink

#endif  // WorkerModuleTreeClient_h
