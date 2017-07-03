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
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/TextPosition.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

// https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-state
enum class ModuleInstantiationState {
  kUninstantiated,
  kErrored,
  kInstantiated,
};

const char* ModuleInstantiationStateToString(ModuleInstantiationState);

// ModuleScript is a model object for the "module script" spec concept.
// https://html.spec.whatwg.org/multipage/webappapis.html#module-script
class CORE_EXPORT ModuleScript final : public Script, public TraceWrapperBase {
 public:
  // https://html.spec.whatwg.org/#creating-a-module-script
  static ModuleScript* Create(
      const String& source_text,
      Modulator*,
      const KURL& base_url,
      const String& nonce,
      ParserDisposition,
      WebURLRequest::FetchCredentialsMode,
      AccessControlStatus,
      const TextPosition& start_position = TextPosition::MinimumPosition());

  // Mostly corresponds to Create() but accepts ScriptModule as the argument
  // and allows null ScriptModule.
  static ModuleScript* CreateForTest(Modulator*,
                                     ScriptModule,
                                     const KURL& base_url,
                                     const String& nonce,
                                     ParserDisposition,
                                     WebURLRequest::FetchCredentialsMode);

  ~ModuleScript() override = default;

  ScriptModule Record() const;
  bool HasEmptyRecord() const;
  const KURL& BaseURL() const { return base_url_; }

  ModuleInstantiationState State() const { return state_; }

  // Corresponds to spec concept: module script's record's [[Status]]
  ScriptModuleState RecordStatus() const;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-has-instantiated
  bool HasInstantiated() const;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-is-errored
  bool IsErrored() const { return record_.IsEmpty(); }

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-set-pre-instantiation-error
  void SetErrorAndClearRecord(ScriptValue error);

  // Implements Step 7.2 of:
  // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure
  void SetInstantiationSuccess();

  v8::Local<v8::Value> CreateError(v8::Isolate* isolate) const {
    return preinstantiation_error_.NewLocal(isolate);
  }

  ParserDisposition ParserState() const { return parser_state_; }
  WebURLRequest::FetchCredentialsMode CredentialsMode() const {
    return credentials_mode_;
  }
  const String& Nonce() const { return nonce_; }

  const TextPosition& StartPosition() const { return start_position_; }

  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  ModuleScript(Modulator* settings_object,
               ScriptModule record,
               const KURL& base_url,
               const String& nonce,
               ParserDisposition parser_state,
               WebURLRequest::FetchCredentialsMode credentials_mode,
               const String& source_text,
               const TextPosition& start_position);

  static ModuleScript* CreateInternal(const String& source_text,
                                      Modulator*,
                                      ScriptModule,
                                      const KURL& base_url,
                                      const String& nonce,
                                      ParserDisposition,
                                      WebURLRequest::FetchCredentialsMode,
                                      const TextPosition&);

  ScriptType GetScriptType() const override { return ScriptType::kModule; }
  bool CheckMIMETypeBeforeRunScript(Document* context_document,
                                    const SecurityOrigin*) const override;
  void RunScript(LocalFrame*, const SecurityOrigin*) const override;
  String InlineSourceTextForCSP() const override;

  friend class ModulatorImpl;
  friend class ModuleTreeLinkerTestModulator;
  // Access this func only via ModulatorImpl::GetError(),
  // or via Modulator mocks for unit tests.
  // TODO(kouhei): Needs update after V8 change. The error may also be stored
  // inside record_.
  v8::Local<v8::Value> CreateErrorInternal(v8::Isolate* isolate) const {
    return preinstantiation_error_.NewLocal(isolate);
  }

  // https://html.spec.whatwg.org/multipage/webappapis.html#settings-object
  Member<Modulator> settings_object_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-module-record
  TraceWrapperV8Reference<v8::Module> record_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-base-url
  const KURL base_url_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-instantiation-state
  ModuleInstantiationState state_ = ModuleInstantiationState::kUninstantiated;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-pre-instantiation-error
  //
  // |record_| and |preinstantiation_error_| are TraceWrappers()ed and kept
  // alive via one or more of following reference graphs:
  // * non-inline module script case
  //   DOMWindow -> Modulator/ModulatorImpl -> ModuleMap -> ModuleMap::Entry
  //   -> ModuleScript
  // * inline module script case, before the PendingScript is created.
  //   DOMWindow -> Modulator/ModulatorImpl -> ModuleTreeLinkerRegistry
  //   -> ModuleTreeLinker -> ModuleScript
  // * inline module script case, after the PendingScript is created.
  //   HTMLScriptElement -> ScriptLoader -> PendingScript -> ModulePendingScript
  //   -> ModulePendingScriptTreeClient -> ModuleScript.
  //
  // All the classes/references on the graphs above should be
  // TraceWrapperBase/TraceWrapperMember<>/etc.,
  TraceWrapperV8Reference<v8::Value> preinstantiation_error_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-nonce
  const String nonce_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-parser
  const ParserDisposition parser_state_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-credentials-mode
  const WebURLRequest::FetchCredentialsMode credentials_mode_;

  // For CSP check.
  const String source_text_;

  const TextPosition start_position_;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const ModuleScript&);

}  // namespace blink

#endif
