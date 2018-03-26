// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerModulatorImpl_h
#define WorkerModulatorImpl_h

#include "core/script/ModulatorImplBase.h"

namespace blink {

class ModuleScriptFetcher;
class ScriptState;

// WorkerModulatorImpl is the Modulator implementation used in worker contexts
// (that means, not main documents). Module operations depending on the Worker
// context should be implemented in this class, not in ModulatorImplBase.
class WorkerModulatorImpl final : public ModulatorImplBase {
 public:
  static ModulatorImplBase* Create(scoped_refptr<ScriptState>);

  // Implements ModulatorImplBase.
  ModuleScriptFetcher* CreateModuleScriptFetcher() override;

 private:
  explicit WorkerModulatorImpl(scoped_refptr<ScriptState>);

  // Implements ModulatorImplBase.
  bool IsDynamicImportForbidden(String* reason) override;
};

}  // namespace blink

#endif  // WorkerModulatorImpl_h
