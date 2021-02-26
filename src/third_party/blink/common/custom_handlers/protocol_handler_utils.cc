// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace blink {

bool IsValidCustomHandlerScheme(const base::StringPiece scheme,
                                bool& has_custom_scheme_prefix) {
  has_custom_scheme_prefix = false;

  static constexpr const char kWebPrefix[] = "web+";
  static constexpr const size_t kWebPrefixLength = base::size(kWebPrefix) - 1;
  if (base::StartsWith(scheme, kWebPrefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    has_custom_scheme_prefix = true;
    // HTML5 requires that schemes with the |web+| prefix contain one or more
    // ASCII alphas after that prefix.
    auto scheme_name = scheme.substr(kWebPrefixLength);
    if (scheme_name.empty())
      return false;
    for (auto& character : scheme_name) {
      if (!base::IsAsciiAlpha(character))
        return false;
    }
    return true;
  }

  static constexpr const char* const kProtocolSafelist[] = {
      "bitcoin",  "cabal",       "dat",    "did",    "doi",   "dweb",
      "ethereum", "geo",         "hyper",  "im",     "ipfs",  "ipns",
      "irc",      "ircs",        "magnet", "mailto", "mms",   "news",
      "nntp",     "openpgp4fpr", "sip",    "sms",    "smsto", "ssb",
      "ssh",      "tel",         "urn",    "webcal", "wtai",  "xmpp"};
  return base::Contains(kProtocolSafelist, base::ToLowerASCII(scheme));
}

}  // namespace blink
