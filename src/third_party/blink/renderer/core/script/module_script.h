// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULE_SCRIPT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULE_SCRIPT_H_

#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "v8/include/v8.h"

namespace blink {

// ModuleScript is a model object for the "module script" spec concept.
// https://html.spec.whatwg.org/C/#module-script
class CORE_EXPORT ModuleScript final : public Script, public NameClient {
 public:
  // https://html.spec.whatwg.org/C/#creating-a-module-script
  static ModuleScript* Create(
      const ParkableString& source_text,
      SingleCachedMetadataHandler*,
      ScriptSourceLocationType,
      Modulator*,
      const KURL& source_url,
      const KURL& base_url,
      const ScriptFetchOptions&,
      const TextPosition& start_position = TextPosition::MinimumPosition());

  // Mostly corresponds to Create() but accepts ModuleRecord as the argument
  // and allows null ModuleRecord.
  static ModuleScript* CreateForTest(
      Modulator*,
      ModuleRecord,
      const KURL& base_url,
      const ScriptFetchOptions& = ScriptFetchOptions());

  ModuleScript(Modulator* settings_object,
               ModuleRecord record,
               const KURL& source_url,
               const KURL& base_url,
               const ScriptFetchOptions&,
               const ParkableString& source_text,
               const TextPosition& start_position,
               ModuleRecordProduceCacheData*);
  ~ModuleScript() override = default;

  ModuleRecord Record() const;
  bool HasEmptyRecord() const;

  // Note: ParseError-related methods should only be used from ModuleTreeLinker
  //       or unit tests. You probably want to check |*ErrorToRethrow*()|
  //       instead.

  void SetParseErrorAndClearRecord(ScriptValue error);
  bool HasParseError() const { return !parse_error_.IsEmpty(); }

  // CreateParseError() retrieves |parse_error_| as a ScriptValue.
  ScriptValue CreateParseError() const;

  void SetErrorToRethrow(ScriptValue error);
  bool HasErrorToRethrow() const { return !error_to_rethrow_.IsEmpty(); }
  ScriptValue CreateErrorToRethrow() const;

  const TextPosition& StartPosition() const { return start_position_; }

  // Resolves a module specifier with the module script's base URL.
  KURL ResolveModuleSpecifier(const String& module_request,
                              String* failure_reason = nullptr) const;

  void Trace(blink::Visitor*) override;
  const char* NameInHeapSnapshot() const override { return "ModuleScript"; }

  void ProduceCache();

 private:
  static ModuleScript* CreateInternal(const ParkableString& source_text,
                                      Modulator*,
                                      ModuleRecord,
                                      const KURL& source_url,
                                      const KURL& base_url,
                                      const ScriptFetchOptions&,
                                      const TextPosition&,
                                      ModuleRecordProduceCacheData*);

  mojom::ScriptType GetScriptType() const override {
    return mojom::ScriptType::kModule;
  }
  void RunScript(LocalFrame*, const SecurityOrigin*) override;
  String InlineSourceTextForCSP() const override;

  friend class ModulatorImplBase;
  friend class ModuleTreeLinkerTestModulator;
  friend class ModuleScriptTest;

  // https://html.spec.whatwg.org/C/#settings-object
  Member<Modulator> settings_object_;

  // https://html.spec.whatwg.org/C/#concept-script-record
  // TODO(keishi): Visitor only defines a trace method for v8::Value so this
  // needs to be cast.
  GC_PLUGIN_IGNORE("757708")
  TraceWrapperV8Reference<v8::Module> record_;

  // https://html.spec.whatwg.org/C/#concept-script-parse-error
  //
  // |record_|, |parse_error_| and |error_to_rethrow_| are wrapper traced and
  // kept alive via one or more of following reference graphs:
  // * non-inline module script case
  //   DOMWindow -> Modulator/ModulatorImpl -> ModuleMap -> ModuleMap::Entry
  //   -> ModuleScript
  // * inline module script case, before the PendingScript is created.
  //   DOMWindow -> Modulator/ModulatorImpl -> ModuleTreeLinkerRegistry
  //   -> ModuleTreeLinker -> ModuleScript
  // * inline module script case, after the PendingScript is created.
  //   HTMLScriptElement -> ScriptLoader -> ModulePendingScript
  //   -> ModulePendingScriptTreeClient -> ModuleScript.
  // * inline module script case, queued in HTMLParserScriptRunner,
  //   when HTMLScriptElement is removed before execution.
  //   Document -> HTMLDocumentParser -> HTMLParserScriptRunner
  //   -> ModulePendingScript -> ModulePendingScriptTreeClient
  //   -> ModuleScript.
  // * inline module script case, queued in ScriptRunner.
  //   Document -> ScriptRunner -> ScriptLoader -> ModulePendingScript
  //   -> ModulePendingScriptTreeClient -> ModuleScript.
  // All the classes/references on the graphs above should be
  // Member<>/etc.,
  //
  // A parse error and an error to rethrow belong to a script, not to a
  // |parse_error_| and |error_to_rethrow_| should belong to a script (i.e.
  // blink::Script) according to the spec, but are put here in ModuleScript,
  // because:
  // - Error handling for classic and module scripts are somehow separated and
  //   there are no urgent motivation for merging the error handling and placing
  //   the errors in Script, and
  // - Classic scripts are handled according to the spec before
  //   https://github.com/whatwg/html/pull/2991. This shouldn't cause any
  //   observable functional changes, and updating the classic script handling
  //   will require moderate code changes (e.g. to move compilation timing).
  TraceWrapperV8Reference<v8::Value> parse_error_;

  // https://html.spec.whatwg.org/C/#concept-script-error-to-rethrow
  TraceWrapperV8Reference<v8::Value> error_to_rethrow_;

  // For CSP check.
  const ParkableString source_text_;

  const TextPosition start_position_;
  mutable HashMap<String, KURL> specifier_to_url_cache_;
  KURL source_url_;

  // Only for ProduceCache(). ModuleScript keeps |produce_cache_data| because:
  // - CompileModule() and ProduceCache() should be called at different
  //   timings, and
  // - There are no persistent object that can hold this in
  //   bindings/core/v8 side. ModuleRecord should be short-lived and is
  //   constructed every time in ModuleScript::Record().
  //
  // Cleared once ProduceCache() is called, to avoid
  // calling V8CodeCache::ProduceCache() multiple times, as a ModuleScript
  // can appear multiple times in multiple module graphs.
  Member<ModuleRecordProduceCacheData> produce_cache_data_;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const ModuleScript&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULE_SCRIPT_H_
