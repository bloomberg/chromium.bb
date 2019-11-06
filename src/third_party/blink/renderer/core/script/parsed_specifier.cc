// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/parsed_specifier.h"

namespace blink {

// <specdef href="https://html.spec.whatwg.org/#resolve-a-module-specifier">
ParsedSpecifier ParsedSpecifier::Create(const String& specifier,
                                        const KURL& base_url) {
  // <spec step="1">Apply the URL parser to specifier. If the result is not
  // failure, return the result.</spec>
  KURL url(NullURL(), specifier);
  if (url.IsValid())
    return ParsedSpecifier(url);

  // <spec step="2">If specifier does not start with the character U+002F
  // SOLIDUS (/), the two-character sequence U+002E FULL STOP, U+002F SOLIDUS
  // (./), or the three-character sequence U+002E FULL STOP, U+002E FULL STOP,
  // U+002F SOLIDUS (../), return failure.</spec>
  if (!specifier.StartsWith("/") && !specifier.StartsWith("./") &&
      !specifier.StartsWith("../")) {
    // Do not consider an empty specifier as a valid bare specifier.
    if (specifier.IsEmpty())
      return ParsedSpecifier();

    return ParsedSpecifier(specifier);
  }

  // <spec step="3">Return the result of applying the URL parser to specifier
  // with base URL as the base URL.</spec>
  DCHECK(base_url.IsValid());
  KURL absolute_url(base_url, specifier);
  if (absolute_url.IsValid())
    return ParsedSpecifier(absolute_url);

  return ParsedSpecifier();
}

String ParsedSpecifier::GetImportMapKeyString() const {
  switch (GetType()) {
    case Type::kInvalid:
      return String();
    case Type::kBare:
      return bare_specifier_;
    case Type::kURL:
      return url_.GetString();
  }
}

KURL ParsedSpecifier::GetUrl() const {
  switch (GetType()) {
    case Type::kInvalid:
    case Type::kBare:
      return NullURL();
    case Type::kURL:
      return url_;
  }
}

}  // namespace blink
