// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/DummyModulator.h"

namespace blink {

DummyModulator::DummyModulator() {}

DummyModulator::~DummyModulator() {}

DEFINE_TRACE(DummyModulator) {}

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

}  // namespace blink
