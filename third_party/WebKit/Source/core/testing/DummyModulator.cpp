// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/DummyModulator.h"

namespace blink {

DummyModulator::DummyModulator() {}

DummyModulator::~DummyModulator() {}

DEFINE_TRACE(DummyModulator) {}

ReferrerPolicy DummyModulator::referrerPolicy() {
  NOTREACHED();
  return ReferrerPolicyDefault;
}

SecurityOrigin* DummyModulator::securityOrigin() {
  NOTREACHED();
  return nullptr;
}

ScriptModuleResolver* DummyModulator::scriptModuleResolver() {
  NOTREACHED();
  return nullptr;
}

WebTaskRunner* DummyModulator::taskRunner() {
  NOTREACHED();
  return nullptr;
};

void DummyModulator::fetchNewSingleModule(const ModuleScriptFetchRequest&,
                                          ModuleGraphLevel,
                                          ModuleScriptLoaderClient*) {
  NOTREACHED();
}

ScriptModule DummyModulator::compileModule(const String& script,
                                           const String& urlStr) {
  NOTREACHED();
  return ScriptModule();
}

}  // namespace blink
