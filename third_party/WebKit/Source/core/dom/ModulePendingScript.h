// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModulePendingScript_h
#define ModulePendingScript_h

#include "core/dom/Modulator.h"
#include "core/dom/ModuleScript.h"
#include "core/dom/PendingScript.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"

namespace blink {

class ModulePendingScript;

// ModulePendingScriptTreeClient is used to connect from Modulator::FetchTree()
// to ModulePendingScript. Because ModulePendingScript is created after
// Modulator::FetchTree() is called, ModulePendingScriptTreeClient is
// registered as ModuleTreeClient to FetchTree() first, and later
// ModulePendingScript is supplied to ModulePendingScriptTreeClient via
// SetPendingScript() and is notified of module tree load finish.
class ModulePendingScriptTreeClient final : public ModuleTreeClient {
 public:
  static ModulePendingScriptTreeClient* Create() {
    return new ModulePendingScriptTreeClient();
  }
  virtual ~ModulePendingScriptTreeClient() = default;

  void SetPendingScript(ModulePendingScript* client);

  ModuleScript* GetModuleScript() const { return module_script_; }

  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  ModulePendingScriptTreeClient();

  // Implements ModuleTreeClient
  void NotifyModuleTreeLoadFinished(ModuleScript*) override;

  bool finished_ = false;
  TraceWrapperMember<ModuleScript> module_script_;
  TraceWrapperMember<ModulePendingScript> pending_script_;
};

// PendingScript for a module script
// https://html.spec.whatwg.org/#module-script.
class CORE_EXPORT ModulePendingScript : public PendingScript {
 public:
  static ModulePendingScript* Create(ScriptElementBase* element,
                                     ModulePendingScriptTreeClient* client) {
    return new ModulePendingScript(element, client);
  }

  ~ModulePendingScript() override;

  void NotifyModuleTreeLoadFinished();

  ModuleScript* GetModuleScript() const {
    return module_tree_client_->GetModuleScript();
  }

  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  ModulePendingScript(ScriptElementBase*, ModulePendingScriptTreeClient*);

  // PendingScript
  ScriptType GetScriptType() const override { return ScriptType::kModule; }
  Script* GetSource(const KURL& document_url,
                    bool& error_occurred) const override;
  bool IsReady() const override { return ready_; }
  bool IsExternal() const override { return true; }
  bool ErrorOccurred() const override;
  bool WasCanceled() const override { return false; }

  void StartStreamingIfPossible(Document*, ScriptStreamer::Type) override {}
  KURL UrlForClassicScript() const override {
    NOTREACHED();
    return KURL();
  }
  void RemoveFromMemoryCache() override { NOTREACHED(); }

  void DisposeInternal() override;

  void CheckState() const override {}

  TraceWrapperMember<ModulePendingScriptTreeClient> module_tree_client_;
  bool ready_ = false;
};

}  // namespace blink

#endif  // ModulePendingScript_h
