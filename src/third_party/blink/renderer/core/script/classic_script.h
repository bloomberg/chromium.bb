// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_CLASSIC_SCRIPT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_CLASSIC_SCRIPT_H_

#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"

namespace blink {

class CORE_EXPORT ClassicScript final : public Script {
 public:
  // For scripts specified in the HTML spec.
  // Please leave spec comments and spec links that explain given argument
  // values at callers.
  ClassicScript(const ScriptSourceCode& script_source_code,
                const KURL& base_url,
                const ScriptFetchOptions& fetch_options,
                SanitizeScriptErrors sanitize_script_errors)
      : Script(fetch_options, base_url),
        script_source_code_(script_source_code),
        sanitize_script_errors_(sanitize_script_errors) {}

  // For scripts not specified in the HTML spec.
  //
  // New callers should use SanitizeScriptErrors::kSanitize as a safe default
  // value, while some existing callers uses kDoNotSanitize to preserve existing
  // behavior.
  // TODO(crbug/1112266): Use kSanitize for all existing callers if possible, or
  // otherwise add comments why kDoNotSanitize should be used.
  static ClassicScript* CreateUnspecifiedScript(
      const ScriptSourceCode&,
      SanitizeScriptErrors = SanitizeScriptErrors::kSanitize);

  void Trace(Visitor*) const override;

  const ScriptSourceCode& GetScriptSourceCode() const {
    return script_source_code_;
  }
  SanitizeScriptErrors GetSanitizeScriptErrors() const {
    return sanitize_script_errors_;
  }

  // TODO(crbug.com/1111134): Methods with ExecuteScriptPolicy are declared and
  // overloaded here to avoid modifying Script::RunScript*(), because this is a
  // tentative interface. When crbug/1111134 is done, these should be gone.
  // TODO(crbug.com/1111134): Refactor RunScript*() interfaces.
  void RunScript(LocalDOMWindow*) override;
  void RunScript(LocalDOMWindow*, ExecuteScriptPolicy);
  bool RunScriptOnWorkerOrWorklet(WorkerOrWorkletGlobalScope&) override;

  // Unlike RunScript() and RunScriptOnWorkerOrWorklet(), callers of the
  // following methods must enter a v8::HandleScope before calling.
  // TODO(crbug.com/1129743): Use ScriptEvaluationResult instead of
  // v8::Local<v8::Value> as the return type.
  ScriptEvaluationResult RunScriptOnScriptStateAndReturnValue(
      ScriptState*,
      ExecuteScriptPolicy =
          ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled,
      V8ScriptRunner::RethrowErrorsOption =
          V8ScriptRunner::RethrowErrorsOption::DoNotRethrow());
  v8::Local<v8::Value> RunScriptAndReturnValue(
      LocalDOMWindow*,
      ExecuteScriptPolicy =
          ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled);
  v8::Local<v8::Value> RunScriptInIsolatedWorldAndReturnValue(LocalDOMWindow*,
                                                              int32_t world_id);

 private:
  mojom::blink::ScriptType GetScriptType() const override {
    return mojom::blink::ScriptType::kClassic;
  }

  std::pair<size_t, size_t> GetClassicScriptSizes() const override;

  const ScriptSourceCode script_source_code_;
  const SanitizeScriptErrors sanitize_script_errors_;
};

}  // namespace blink

#endif
