// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReferrerScriptInfo_h
#define ReferrerScriptInfo_h

#include "core/CoreExport.h"
#include "platform/loader/fetch/AccessControlStatus.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ScriptFetchOptions.h"
#include "platform/wtf/text/TextPosition.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebURLRequest.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "v8/include/v8.h"

namespace blink {

// ReferrerScriptInfo carries a copy of "referencing script's" info referenced
// in HTML Spec: "HostImportModuleDynamically" algorithm.
// https://html.spec.whatwg.org/multipage/webappapis.html#hostimportmoduledynamically(referencingscriptormodule,-specifier,-promisecapability)
class CORE_EXPORT ReferrerScriptInfo {
 public:
  ReferrerScriptInfo() {}
  ReferrerScriptInfo(const KURL& base_url,
                     network::mojom::FetchCredentialsMode credentials_mode,
                     const String& nonce,
                     ParserDisposition parser_state)
      : base_url_(base_url),
        credentials_mode_(credentials_mode),
        nonce_(nonce),
        parser_state_(parser_state) {}
  ReferrerScriptInfo(const KURL& base_url, const ScriptFetchOptions& options)
      : ReferrerScriptInfo(base_url,
                           options.CredentialsMode(),
                           options.Nonce(),
                           options.ParserState()) {}

  static ReferrerScriptInfo FromV8HostDefinedOptions(
      v8::Local<v8::Context>,
      v8::Local<v8::PrimitiveArray>);
  v8::Local<v8::PrimitiveArray> ToV8HostDefinedOptions(v8::Isolate*) const;

  const KURL& BaseURL() const { return base_url_; }
  network::mojom::FetchCredentialsMode CredentialsMode() const {
    return credentials_mode_;
  }
  const String& Nonce() const { return nonce_; }
  ParserDisposition ParserState() const { return parser_state_; }

  bool IsDefaultValue() const {
    return base_url_.IsNull() &&
           credentials_mode_ == network::mojom::FetchCredentialsMode::kOmit &&
           nonce_.IsEmpty() && parser_state_ == kNotParserInserted;
  }

 private:
  // Spec: "referencing script's base URL"
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-base-url
  //
  // If base_url_.IsNull(), refer to ScriptOrigin::ResourceName() instead.
  // Note: This improves the chance of getting into the fast path in
  //       ToV8HostDefinedOptions().
  const KURL base_url_;

  // Spec: "referencing script's credentials mode"
  // The default value is "omit" per:
  // https://html.spec.whatwg.org/multipage/webappapis.html#default-classic-script-fetch-options
  const network::mojom::FetchCredentialsMode credentials_mode_ =
      network::mojom::FetchCredentialsMode::kOmit;

  // Spec: "referencing script's cryptographic nonce"
  const String nonce_;

  // Spec: "referencing script's parser state"
  // The default value is "not-parser-inserted" per:
  // https://html.spec.whatwg.org/multipage/webappapis.html#default-classic-script-fetch-options
  const ParserDisposition parser_state_ = kNotParserInserted;
};

}  // namespace blink

#endif  // ReferrerScriptInfo_h
