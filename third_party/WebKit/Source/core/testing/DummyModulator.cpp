// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/DummyModulator.h"

#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/ScriptModuleResolver.h"

namespace blink {

namespace {

class EmptyScriptModuleResolver final : public ScriptModuleResolver {
 public:
  EmptyScriptModuleResolver() {}

  // We ignore {Unr,R}egisterModuleScript() calls caused by
  // ModuleScript::CreateForTest().
  void RegisterModuleScript(ModuleScript*) override {}
  void UnregisterModuleScript(ModuleScript*) override {}

  ScriptModule Resolve(const String& specifier,
                       const ScriptModule& referrer,
                       ExceptionState&) override {
    NOTREACHED();
    return ScriptModule();
  }
};

}  // namespace

DummyModulator::DummyModulator() : resolver_(new EmptyScriptModuleResolver()) {}

DummyModulator::~DummyModulator() {}

DEFINE_TRACE(DummyModulator) {
  visitor->Trace(resolver_);
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

ScriptState* DummyModulator::GetScriptState() {
  NOTREACHED();
  return nullptr;
}

ScriptModuleResolver* DummyModulator::GetScriptModuleResolver() {
  return resolver_.Get();
}

WebTaskRunner* DummyModulator::TaskRunner() {
  NOTREACHED();
  return nullptr;
};

void DummyModulator::FetchTree(const ModuleScriptFetchRequest&,
                               ModuleTreeClient*) {
  NOTREACHED();
}

void DummyModulator::FetchSingle(const ModuleScriptFetchRequest&,
                                 ModuleGraphLevel,
                                 SingleModuleClient*) {
  NOTREACHED();
}

void DummyModulator::FetchDescendantsForInlineScript(ModuleScript*,
                                                     ModuleTreeClient*) {
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

bool DummyModulator::HasValidContext() {
  return true;
}

void DummyModulator::ResolveDynamically(const String&,
                                        const KURL&,
                                        const ReferrerScriptInfo&,
                                        ScriptPromiseResolver*) {
  NOTREACHED();
}

ScriptModule DummyModulator::CompileModule(const String& script,
                                           const String& url_str,
                                           AccessControlStatus,
                                           WebURLRequest::FetchCredentialsMode,
                                           const String& nonce,
                                           ParserDisposition,
                                           const TextPosition&,
                                           ExceptionState&) {
  NOTREACHED();
  return ScriptModule();
}

ScriptValue DummyModulator::InstantiateModule(ScriptModule) {
  NOTREACHED();
  return ScriptValue();
}

ScriptModuleState DummyModulator::GetRecordStatus(ScriptModule) {
  NOTREACHED();
  return ScriptModuleState::kErrored;
}

ScriptValue DummyModulator::GetError(const ModuleScript*) {
  NOTREACHED();
  return ScriptValue();
}

Vector<Modulator::ModuleRequest> DummyModulator::ModuleRequestsFromScriptModule(
    ScriptModule) {
  NOTREACHED();
  return Vector<ModuleRequest>();
}

ScriptValue DummyModulator::ExecuteModule(const ModuleScript*,
                                          CaptureEvalErrorFlag) {
  NOTREACHED();
  return ScriptValue();
}

ModuleScriptFetcher* DummyModulator::CreateModuleScriptFetcher() {
  NOTREACHED();
  return nullptr;
}

}  // namespace blink
