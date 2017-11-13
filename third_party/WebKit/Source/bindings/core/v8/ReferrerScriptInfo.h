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
#include "services/network/public/interfaces/fetch_api.mojom-blink.h"
#include "v8/include/v8.h"

namespace blink {

// ReferrerScriptInfo carries a copy of "referencing script's" info referenced
// in HTML Spec: "HostImportModuleDynamically" algorithm.
// https://github.com/tc39/proposal-dynamic-import/blob/master/HTML%20Integration.md#hostimportmoduledynamicallyreferencingscriptormodule-specifier-promisecapability
class CORE_EXPORT ReferrerScriptInfo {
 public:
  ReferrerScriptInfo() {}
  ReferrerScriptInfo(network::mojom::FetchCredentialsMode credentials_mode,
                     const String& nonce,
                     ParserDisposition parser_state)
      : credentials_mode_(credentials_mode),
        nonce_(nonce),
        parser_state_(parser_state) {}
  static ReferrerScriptInfo FromScriptFetchOptions(
      const ScriptFetchOptions& options) {
    return ReferrerScriptInfo(options.CredentialsMode(), options.Nonce(),
                              options.ParserState());
  }

  static ReferrerScriptInfo FromV8HostDefinedOptions(
      v8::Local<v8::Context>,
      v8::Local<v8::PrimitiveArray>);
  v8::Local<v8::PrimitiveArray> ToV8HostDefinedOptions(v8::Isolate*) const;

  network::mojom::FetchCredentialsMode CredentialsMode() const {
    return credentials_mode_;
  }
  const String& Nonce() const { return nonce_; }
  ParserDisposition ParserState() const { return parser_state_; }

  bool IsDefaultValue() const {
    return credentials_mode_ == network::mojom::FetchCredentialsMode::kOmit &&
           nonce_.IsEmpty() && parser_state_ == kNotParserInserted;
  }

 private:
  // Spec: "referencing script's credentials mode"
  // The default value is "omit", from Step 5 of [HIMD].
  // [HIMD]
  // https://github.com/tc39/proposal-dynamic-import/blob/master/HTML%20Integration.md#hostimportmoduledynamicallyreferencingscriptormodule-specifier-promisecapability
  network::mojom::FetchCredentialsMode credentials_mode_ =
      network::mojom::FetchCredentialsMode::kOmit;
  // Spec: "referencing script's cryptographic nonce"
  String nonce_;
  // Spec: "referencing script's parser state"
  ParserDisposition parser_state_ = kNotParserInserted;
};

}  // namespace blink

#endif  // ReferrerScriptInfo_h
