// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleScript_h
#define ModuleScript_h

#include "bindings/core/v8/ScriptModule.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "core/dom/Modulator.h"
#include "core/dom/Script.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/TextPosition.h"

namespace blink {

// ModuleScript is a model object for the "module script" spec concept.
// https://html.spec.whatwg.org/multipage/webappapis.html#module-script
class CORE_EXPORT ModuleScript final : public Script, public TraceWrapperBase {
 public:
  // https://html.spec.whatwg.org/#creating-a-module-script
  static ModuleScript* Create(
      const String& source_text,
      Modulator*,
      const KURL& base_url,
      const ScriptFetchOptions&,
      AccessControlStatus,
      const TextPosition& start_position = TextPosition::MinimumPosition());

  // Mostly corresponds to Create() but accepts ScriptModule as the argument
  // and allows null ScriptModule.
  static ModuleScript* CreateForTest(
      Modulator*,
      ScriptModule,
      const KURL& base_url,
      const ScriptFetchOptions& = ScriptFetchOptions());

  ~ModuleScript() override = default;

  ScriptModule Record() const;
  bool HasEmptyRecord() const;
  const KURL& BaseURL() const { return base_url_; }

  void SetParseErrorAndClearRecord(ScriptValue error);
  bool HasParseError() const { return !parse_error_.IsEmpty(); }
  ScriptValue CreateParseError() const;

  void SetErrorToRethrow(ScriptValue error);
  bool HasErrorToRethrow() const { return !error_to_rethrow_.IsEmpty(); }
  ScriptValue CreateErrorToRethrow() const;

  const TextPosition& StartPosition() const { return start_position_; }

  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

 private:
  ModuleScript(Modulator* settings_object,
               ScriptModule record,
               const KURL& base_url,
               const ScriptFetchOptions&,
               const String& source_text,
               const TextPosition& start_position);

  static ModuleScript* CreateInternal(const String& source_text,
                                      Modulator*,
                                      ScriptModule,
                                      const KURL& base_url,
                                      const ScriptFetchOptions&,
                                      const TextPosition&);

  ScriptType GetScriptType() const override { return ScriptType::kModule; }
  void RunScript(LocalFrame*, const SecurityOrigin*) const override;
  String InlineSourceTextForCSP() const override;

  friend class ModulatorImplBase;
  friend class ModuleTreeLinkerTestModulator;

  // https://html.spec.whatwg.org/multipage/webappapis.html#settings-object
  Member<Modulator> settings_object_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-module-record
  TraceWrapperV8Reference<v8::Module> record_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-base-url
  const KURL base_url_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-parse-error
  //
  // |record_|, |parse_error_| and |error_to_rethrow_| are TraceWrappers()ed and
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
  // TraceWrapperBase/TraceWrapperMember<>/etc.,
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

  // https://html.spec.whatwg.org/multipage/webappapis.html##concept-script-error-to-rethrow
  TraceWrapperV8Reference<v8::Value> error_to_rethrow_;

  // For CSP check.
  const String source_text_;

  const TextPosition start_position_;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const ModuleScript&);

}  // namespace blink

#endif
