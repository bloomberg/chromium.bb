// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/DummyModulator.h"

#include "bindings/core/v8/ScriptValue.h"

namespace blink {

DummyModulator::DummyModulator() {}

DummyModulator::~DummyModulator() {}

DEFINE_TRACE(DummyModulator) {
  Modulator::Trace(visitor);
}

ReferrerPolicy DummyModulator::GetReferrerPolicy() {
  NOTREACHED();
  return kReferrerPolicyDefault;
}

SecurityOrigin* DummyModulator::GetSecurityOrigin() {
  NOTREACHED();
  return nullptr;
}

ScriptModuleResolver* DummyModulator::GetScriptModuleResolver() {
  NOTREACHED();
  return nullptr;
}

WebTaskRunner* DummyModulator::TaskRunner() {
  NOTREACHED();
  return nullptr;
};

void DummyModulator::FetchTree(const ModuleScriptFetchRequest&,
                               ModuleTreeClient*) {
  NOTREACHED();
}

void DummyModulator::FetchTreeInternal(const ModuleScriptFetchRequest&,
                                       const AncestorList&,
                                       ModuleGraphLevel,
                                       ModuleTreeClient*) {
  NOTREACHED();
};

void DummyModulator::FetchSingle(const ModuleScriptFetchRequest&,
                                 ModuleGraphLevel,
                                 SingleModuleClient*) {
  NOTREACHED();
}

ModuleScript* DummyModulator::GetFetchedModuleScript(const KURL&) {
  NOTREACHED();
  return nullptr;
}

void DummyModulator::FetchNewSingleModule(const ModuleScriptFetchRequest&,
                                          ModuleGraphLevel,
                                          ModuleScriptLoaderClient*) {
  NOTREACHED();
}

ScriptModule DummyModulator::CompileModule(const String& script,
                                           const String& url_str,
                                           AccessControlStatus) {
  NOTREACHED();
  return ScriptModule();
}

ScriptValue DummyModulator::InstantiateModule(ScriptModule) {
  NOTREACHED();
  return ScriptValue();
}

ScriptValue DummyModulator::GetInstantiationError(const ModuleScript*) {
  NOTREACHED();
  return ScriptValue();
}

Vector<String> DummyModulator::ModuleRequestsFromScriptModule(ScriptModule) {
  NOTREACHED();
  return Vector<String>();
}

void DummyModulator::ExecuteModule(const ModuleScript*) {
  NOTREACHED();
}

}  // namespace blink
