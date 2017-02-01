// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleScriptFetchRequest_h
#define ModuleScriptFetchRequest_h

#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/Referrer.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/text/WTFString.h"

namespace blink {

// A ModuleScriptFetchRequest is a "parameter object" for
// Modulator::fetch{,New}SingleModule to avoid the methods having too many
// arguments.
// In terms of spec, a ModuleScriptFetchRequest carries most of the arguments
// for "fetch a single module script" algorithm:
// https://html.spec.whatwg.org/#fetch-a-single-module-script
class ModuleScriptFetchRequest final {
  STACK_ALLOCATED();

 public:
  ModuleScriptFetchRequest(const KURL& url,
                           const String& nonce,
                           ParserDisposition parserState,
                           WebURLRequest::FetchCredentialsMode credentialsMode)
      : ModuleScriptFetchRequest(url,
                                 nonce,
                                 parserState,
                                 credentialsMode,
                                 Referrer::noReferrer()) {}
  ~ModuleScriptFetchRequest() = default;

  const KURL& url() const { return m_url; }
  const String& nonce() const { return m_nonce; }
  const ParserDisposition& parserState() const { return m_parserState; }
  WebURLRequest::FetchCredentialsMode credentialsMode() const {
    return m_credentialsMode;
  }
  const AtomicString& referrer() const { return m_referrer; }

 private:
  // Referrer is set only for internal module script fetch algorithms triggered
  // from ModuleTreeLinker to fetch descendant module scripts.
  friend class ModuleTreeLinker;
  ModuleScriptFetchRequest(const KURL& url,
                           const String& nonce,
                           ParserDisposition parserState,
                           WebURLRequest::FetchCredentialsMode credentialsMode,
                           const String& referrer)
      : m_url(url),
        m_nonce(nonce),
        m_parserState(parserState),
        m_credentialsMode(credentialsMode),
        m_referrer(referrer) {}

  const KURL m_url;
  const String m_nonce;
  const ParserDisposition m_parserState;
  const WebURLRequest::FetchCredentialsMode m_credentialsMode;
  const AtomicString m_referrer;
};

}  // namespace blink

#endif
