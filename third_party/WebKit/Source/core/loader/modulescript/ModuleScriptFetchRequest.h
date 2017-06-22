// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleScriptFetchRequest_h
#define ModuleScriptFetchRequest_h

#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/Referrer.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

// A ModuleScriptFetchRequest is a "parameter object" for
// Modulator::fetch{,New}SingleModule to avoid the methods having too many
// arguments.
// In terms of spec, a ModuleScriptFetchRequest carries the arguments
// for "fetch a single module script" algorithm:
// https://html.spec.whatwg.org/#fetch-a-single-module-script
// and "internal module script graph fetching procedure":
// https://html.spec.whatwg.org/#internal-module-script-graph-fetching-procedure
// EXCEPT for:
// - an ancestor list ("internal module script graph fetching procedure" only)
// - a top-level module fetch flag
// - a module map settings object
// - a fetch client settings object
// - a destination
class ModuleScriptFetchRequest final {
  STACK_ALLOCATED();

 public:
  ModuleScriptFetchRequest(const KURL& url,
                           const String& nonce,
                           ParserDisposition parser_state,
                           WebURLRequest::FetchCredentialsMode credentials_mode)
      : ModuleScriptFetchRequest(url,
                                 nonce,
                                 parser_state,
                                 credentials_mode,
                                 Referrer::NoReferrer(),
                                 TextPosition::MinimumPosition()) {}
  ~ModuleScriptFetchRequest() = default;

  const KURL& Url() const { return url_; }
  const String& Nonce() const { return nonce_; }
  const ParserDisposition& ParserState() const { return parser_state_; }
  WebURLRequest::FetchCredentialsMode CredentialsMode() const {
    return credentials_mode_;
  }
  const AtomicString& GetReferrer() const { return referrer_; }
  const TextPosition& GetReferrerPosition() const { return referrer_position_; }

 private:
  // Referrer is set only for internal module script fetch algorithms triggered
  // from ModuleTreeLinker to fetch descendant module scripts.
  friend class ModuleTreeLinker;
  ModuleScriptFetchRequest(const KURL& url,
                           const String& nonce,
                           ParserDisposition parser_state,
                           WebURLRequest::FetchCredentialsMode credentials_mode,
                           const String& referrer,
                           const TextPosition& referrer_position)
      : url_(url),
        nonce_(nonce),
        parser_state_(parser_state),
        credentials_mode_(credentials_mode),
        referrer_(referrer),
        referrer_position_(referrer_position) {}

  const KURL url_;
  const String nonce_;
  const ParserDisposition parser_state_;
  const WebURLRequest::FetchCredentialsMode credentials_mode_;
  const AtomicString referrer_;
  const TextPosition referrer_position_;
};

}  // namespace blink

#endif
