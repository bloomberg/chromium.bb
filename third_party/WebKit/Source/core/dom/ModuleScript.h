// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleScript_h
#define ModuleScript_h

#include "bindings/core/v8/ScriptModule.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

// https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-instantiation-state
enum class ModuleInstantiationState {
  Uninstantiated,
  Errored,
  Instantiated,
};

// ModuleScript is a model object for the "module script" spec concept.
// https://html.spec.whatwg.org/multipage/webappapis.html#module-script
class CORE_EXPORT ModuleScript final
    : public GarbageCollectedFinalized<ModuleScript> {
 public:
  static ModuleScript* create(
      ScriptModule record,
      const KURL& baseURL,
      const String& nonce,
      ParserDisposition parserState,
      WebURLRequest::FetchCredentialsMode credentialsMode) {
    return new ModuleScript(record, baseURL, nonce, parserState,
                            credentialsMode);
  }
  ~ModuleScript() = default;

  ScriptModule& record() { return m_record; }
  void clearRecord() { m_record = ScriptModule(); }
  const KURL& baseURL() const { return m_baseURL; }

  ParserDisposition parserState() const { return m_parserState; }
  WebURLRequest::FetchCredentialsMode credentialsMode() const {
    return m_credentialsMode;
  }
  const String& nonce() const { return m_nonce; }

  ModuleInstantiationState instantiationState() const {
    return m_instantiationState;
  }

  DECLARE_TRACE();

 private:
  ModuleScript(ScriptModule record,
               const KURL& baseURL,
               const String& nonce,
               ParserDisposition parserState,
               WebURLRequest::FetchCredentialsMode credentialsMode)
      : m_record(record),
        m_baseURL(baseURL),
        m_nonce(nonce),
        m_parserState(parserState),
        m_credentialsMode(credentialsMode) {}

  // Note: A "module script"'s "setttings object" is ommitted, as we currently
  // always have access to the corresponding Modulator when operating on a
  // ModuleScript instance.
  // https://html.spec.whatwg.org/multipage/webappapis.html#settings-object

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-module-record
  ScriptModule m_record;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-base-url
  const KURL m_baseURL;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-instantiation-state
  ModuleInstantiationState m_instantiationState =
      ModuleInstantiationState::Uninstantiated;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-instantiation-error
  // TODO(kouhei): Add a corresponding member.

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-nonce
  const String m_nonce;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-parser
  const ParserDisposition m_parserState;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-credentials-mode
  const WebURLRequest::FetchCredentialsMode m_credentialsMode;
};

}  // namespace blink

#endif
