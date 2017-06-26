// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ModulePendingScript.h"

#include "core/dom/ScriptLoader.h"
#include "core/frame/LocalFrame.h"

namespace blink {

ModulePendingScriptTreeClient::ModulePendingScriptTreeClient()
    : module_script_(this, nullptr), pending_script_(this, nullptr) {}

void ModulePendingScriptTreeClient::SetPendingScript(
    ModulePendingScript* pending_script) {
  DCHECK(!pending_script_);
  pending_script_ = pending_script;

  if (finished_) {
    pending_script_->NotifyModuleTreeLoadFinished();
  }
}

void ModulePendingScriptTreeClient::NotifyModuleTreeLoadFinished(
    ModuleScript* module_script) {
  DCHECK(!module_script || module_script->IsErrored() ||
         module_script->State() == ModuleInstantiationState::kInstantiated);
  DCHECK(!finished_);
  finished_ = true;
  module_script_ = module_script;

  if (pending_script_)
    pending_script_->NotifyModuleTreeLoadFinished();
}

DEFINE_TRACE(ModulePendingScriptTreeClient) {
  visitor->Trace(module_script_);
  visitor->Trace(pending_script_);
  ModuleTreeClient::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(ModulePendingScriptTreeClient) {
  visitor->TraceWrappers(module_script_);
  visitor->TraceWrappers(pending_script_);
  ModuleTreeClient::TraceWrappers(visitor);
}

ModulePendingScript::ModulePendingScript(ScriptElementBase* element,
                                         ModulePendingScriptTreeClient* client)
    : PendingScript(element, TextPosition()),
      module_tree_client_(this, client) {
  CHECK(this->GetElement());
  DCHECK(module_tree_client_);
  client->SetPendingScript(this);
}

ModulePendingScript::~ModulePendingScript() {}

void ModulePendingScript::DisposeInternal() {
  module_tree_client_ = nullptr;
}

DEFINE_TRACE(ModulePendingScript) {
  visitor->Trace(module_tree_client_);
  PendingScript::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(ModulePendingScript) {
  visitor->TraceWrappers(module_tree_client_);
  PendingScript::TraceWrappers(visitor);
}

void ModulePendingScript::NotifyModuleTreeLoadFinished() {
  CHECK(!IsReady());
  ready_ = true;

  if (Client())
    Client()->PendingScriptFinished(this);
}

Script* ModulePendingScript::GetSource(const KURL& document_url,
                                       bool& error_occurred) const {
  CHECK(IsReady());
  error_occurred = ErrorOccurred();
  return GetModuleScript();
}

bool ModulePendingScript::ErrorOccurred() const {
  CHECK(IsReady());
  return !GetModuleScript();
}

}  // namespace blink
