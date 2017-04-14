// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleScript_h
#define ModuleScript_h

#include "bindings/core/v8/ScriptModule.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/TraceWrapperV8Reference.h"
#include "core/CoreExport.h"
#include "core/dom/Modulator.h"
#include "core/dom/Script.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

// https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-instantiation-state
enum class ModuleInstantiationState {
  kUninstantiated,
  kErrored,
  kInstantiated,
};

// ModuleScript is a model object for the "module script" spec concept.
// https://html.spec.whatwg.org/multipage/webappapis.html#module-script
class CORE_EXPORT ModuleScript final : public Script, public TraceWrapperBase {
 public:
  static ModuleScript* Create(
      Modulator* settings_object,
      ScriptModule record,
      const KURL& base_url,
      const String& nonce,
      ParserDisposition parser_state,
      WebURLRequest::FetchCredentialsMode credentials_mode) {
    return new ModuleScript(settings_object, record, base_url, nonce,
                            parser_state, credentials_mode);
  }
  ~ModuleScript() override = default;

  ScriptModule& Record() { return record_; }
  void ClearRecord() { record_ = ScriptModule(); }
  const KURL& BaseURL() const { return base_url_; }

  ModuleInstantiationState InstantiationState() const {
    return instantiation_state_;
  }

  void SetInstantiationSuccess();
  void SetInstantiationError(v8::Isolate*, v8::Local<v8::Value> error);

  ParserDisposition ParserState() const { return parser_state_; }
  WebURLRequest::FetchCredentialsMode CredentialsMode() const {
    return credentials_mode_;
  }
  const String& Nonce() const { return nonce_; }

  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  ModuleScript(Modulator* settings_object,
               ScriptModule record,
               const KURL& base_url,
               const String& nonce,
               ParserDisposition parser_state,
               WebURLRequest::FetchCredentialsMode credentials_mode)
      : settings_object_(settings_object),
        record_(record),
        base_url_(base_url),
        instantiation_error_(this),
        nonce_(nonce),
        parser_state_(parser_state),
        credentials_mode_(credentials_mode) {}

  ScriptType GetScriptType() const override { return ScriptType::kModule; }
  bool IsEmpty() const override;
  bool CheckMIMETypeBeforeRunScript(Document* context_document,
                                    const SecurityOrigin*) const override;
  void RunScript(LocalFrame*, const SecurityOrigin*) const override;
  String InlineSourceTextForCSP() const override;

  // https://html.spec.whatwg.org/multipage/webappapis.html#settings-object
  Member<Modulator> settings_object_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-module-record
  ScriptModule record_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-base-url
  const KURL base_url_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-instantiation-state
  ModuleInstantiationState instantiation_state_ =
      ModuleInstantiationState::kUninstantiated;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-instantiation-error
  //
  // |instantiation_error_| is TraceWrappers()ed and kept alive via the path of
  // v8::Context -> PerContextData -> Modulator/ModulatorImpl
  // -> ModuleMap -> ModuleMap::Entry -> ModuleScript -> instantiation_error_.
  // All the classes/references on the path above should be
  // TraceWrapperBase/TraceWrapperMember<>/etc.,
  // but other references to those classes can be normal Member<>.
  TraceWrapperV8Reference<v8::Value> instantiation_error_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-nonce
  const String nonce_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-parser
  const ParserDisposition parser_state_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-credentials-mode
  const WebURLRequest::FetchCredentialsMode credentials_mode_;
};

}  // namespace blink

#endif
