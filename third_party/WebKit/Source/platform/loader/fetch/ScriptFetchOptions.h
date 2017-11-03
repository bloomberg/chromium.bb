// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptFetchOptions_h
#define ScriptFetchOptions_h

#include "platform/PlatformExport.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/wtf/text/TextEncoding.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class KURL;
class SecurityOrigin;

// ScriptFetchOptions corresponds to the spec concept "script fetch options".
// https://html.spec.whatwg.org/multipage/webappapis.html#script-fetch-options
class PLATFORM_EXPORT ScriptFetchOptions final {
 public:
  // https://html.spec.whatwg.org/multipage/webappapis.html#default-classic-script-fetch-options
  // "The default classic script fetch options are a script fetch options whose
  // cryptographic nonce is the empty string, integrity metadata is the empty
  // string, parser metadata is "not-parser-inserted", and credentials mode
  // is "omit"." [spec text]
  ScriptFetchOptions()
      : parser_state_(ParserDisposition::kNotParserInserted),
        credentials_mode_(network::mojom::FetchCredentialsMode::kOmit) {}

  ScriptFetchOptions(const String& nonce,
                     const IntegrityMetadataSet& integrity_metadata,
                     const String& integrity_attribute,
                     ParserDisposition parser_state,
                     network::mojom::FetchCredentialsMode credentials_mode)
      : nonce_(nonce),
        integrity_metadata_(integrity_metadata),
        integrity_attribute_(integrity_attribute),
        parser_state_(parser_state),
        credentials_mode_(credentials_mode) {}
  ~ScriptFetchOptions() = default;

  const String& Nonce() const { return nonce_; }
  const IntegrityMetadataSet& GetIntegrityMetadata() const {
    return integrity_metadata_;
  }
  const String& GetIntegrityAttributeValue() const {
    return integrity_attribute_;
  }
  const ParserDisposition& ParserState() const { return parser_state_; }
  network::mojom::FetchCredentialsMode CredentialsMode() const {
    return credentials_mode_;
  }

  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-classic-script
  // Steps 1 and 3.
  FetchParameters CreateFetchParameters(const KURL&,
                                        SecurityOrigin*,
                                        const WTF::TextEncoding&,
                                        FetchParameters::DeferOption) const;

 private:
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-fetch-options-nonce
  const String nonce_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-fetch-options-integrity
  const IntegrityMetadataSet integrity_metadata_;
  const String integrity_attribute_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-fetch-options-parser
  const ParserDisposition parser_state_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-fetch-options-credentials
  const network::mojom::FetchCredentialsMode credentials_mode_;
};

}  // namespace blink

#endif
