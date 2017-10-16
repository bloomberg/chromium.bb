// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClassicScript_h
#define ClassicScript_h

#include "bindings/core/v8/ScriptSourceCode.h"
#include "core/CoreExport.h"
#include "core/dom/Script.h"
#include "platform/loader/fetch/ScriptFetchOptions.h"

namespace blink {

class CORE_EXPORT ClassicScript final : public Script {
 public:
  static ClassicScript* Create(const ScriptSourceCode& script_source_code,
                               const ScriptFetchOptions& fetch_options) {
    return new ClassicScript(script_source_code, fetch_options);
  }

  DECLARE_TRACE();

  const ScriptSourceCode& GetScriptSourceCode() const {
    return script_source_code_;
  }

 private:
  ClassicScript(const ScriptSourceCode& script_source_code,
                const ScriptFetchOptions& fetch_options)
      : Script(fetch_options), script_source_code_(script_source_code) {}

  ScriptType GetScriptType() const override { return ScriptType::kClassic; }
  bool CheckMIMETypeBeforeRunScript(Document* context_document,
                                    const SecurityOrigin*) const override;
  void RunScript(LocalFrame*, const SecurityOrigin*) const override;
  String InlineSourceTextForCSP() const override {
    return script_source_code_.Source();
  }

  const ScriptSourceCode script_source_code_;
};

}  // namespace blink

#endif
