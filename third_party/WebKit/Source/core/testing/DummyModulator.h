// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DummyModulator_h
#define DummyModulator_h

#include "core/dom/Modulator.h"
#include "platform/heap/Handle.h"

namespace blink {

class ModuleScriptLoaderClient;
class ScriptModuleResolver;
class WebTaskRunner;
class ModuleScriptFetchRequest;

// DummyModulator provides empty Modulator interface implementation w/
// NOTREACHED().
//
// DummyModulator is useful for unit-testing.
// Not all module implementation components require full-blown Modulator
// implementation. Unit tests can implement a subset of Modulator interface
// which is exercised from unit-under-test.
class DummyModulator : public GarbageCollectedFinalized<DummyModulator>,
                       public Modulator {
  USING_GARBAGE_COLLECTED_MIXIN(DummyModulator);

 public:
  DummyModulator();
  virtual ~DummyModulator();
  DECLARE_TRACE();

  ScriptModuleResolver* scriptModuleResolver() override;
  WebTaskRunner* taskRunner() override;
  void fetchNewSingleModule(const ModuleScriptFetchRequest&,
                            ModuleGraphLevel,
                            ModuleScriptLoaderClient*) override;
};

}  // namespace blink

#endif  // DummyModulator_h
