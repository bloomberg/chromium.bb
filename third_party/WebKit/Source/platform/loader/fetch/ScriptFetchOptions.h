// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptFetchOptions_h
#define ScriptFetchOptions_h

#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

// ScriptFetchOptions corresponds to the spec concept "script fetch options".
// https://html.spec.whatwg.org/multipage/webappapis.html#script-fetch-options
class ScriptFetchOptions final {
 public:
  // https://html.spec.whatwg.org/multipage/webappapis.html#default-classic-script-fetch-options
  // "The default classic script fetch options are a script fetch options whose
  // cryptographic nonce is the empty string, integrity metadata is the empty
  // string, parser metadata is "not-parser-inserted", and credentials mode
  // is "omit"." [spec text]
  ScriptFetchOptions()
      : parser_state_(ParserDisposition::kNotParserInserted),
        credentials_mode_(WebURLRequest::kFetchCredentialsModeOmit) {}

  ScriptFetchOptions(const String& nonce,
                     ParserDisposition parser_state,
                     WebURLRequest::FetchCredentialsMode credentials_mode)
      : nonce_(nonce),
        parser_state_(parser_state),
        credentials_mode_(credentials_mode) {}
  ~ScriptFetchOptions() = default;

  const String& Nonce() const { return nonce_; }
  const ParserDisposition& ParserState() const { return parser_state_; }
  WebURLRequest::FetchCredentialsMode CredentialsMode() const {
    return credentials_mode_;
  }

 private:
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-fetch-options-nonce
  String nonce_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-fetch-options-integrity
  // TODO(kouhei): const IntegrityMetadata integrity_metadata_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-fetch-options-parser
  ParserDisposition parser_state_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-fetch-options-credentials
  WebURLRequest::FetchCredentialsMode credentials_mode_;
};

}  // namespace blink

#endif
