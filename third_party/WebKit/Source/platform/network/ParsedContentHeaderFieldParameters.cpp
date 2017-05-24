// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/ParsedContentHeaderFieldParameters.h"

#include "platform/network/HeaderFieldTokenizer.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/StringView.h"

namespace blink {

ParsedContentHeaderFieldParameters::ParsedContentHeaderFieldParameters()
    : is_valid_(false) {}

// static
ParsedContentHeaderFieldParameters
ParsedContentHeaderFieldParameters::CreateForTesting(const String& input,
                                                     Mode mode) {
  ParsedContentHeaderFieldParameters parameters;
  parameters.ParseParameters(HeaderFieldTokenizer(input), mode);
  return parameters;
}

String ParsedContentHeaderFieldParameters::ParameterValueForName(
    const String& name) const {
  if (!name.ContainsOnlyASCII())
    return String();
  return parameters_.at(name.LowerASCII());
}

size_t ParsedContentHeaderFieldParameters::ParameterCount() const {
  return parameters_.size();
}

// parameters := *(";" parameter)
//
// From http://tools.ietf.org/html/rfc2045#section-5.1:
//
// parameter := attribute "=" value
//
// attribute := token
//              ; Matching of attributes
//              ; is ALWAYS case-insensitive.
//
// value := token / quoted-string
//
// token := 1*<any (US-ASCII) CHAR except SPACE, CTLs,
//             or tspecials>
//
// tspecials :=  "(" / ")" / "<" / ">" / "@" /
//               "," / ";" / ":" / "\" / <">
//               "/" / "[" / "]" / "?" / "="
//               ; Must be in quoted-string,
//               ; to use within parameter values
void ParsedContentHeaderFieldParameters::ParseParameters(
    HeaderFieldTokenizer tokenizer,
    Mode mode) {
  DCHECK(!is_valid_);
  DCHECK(parameters_.IsEmpty());

  KeyValuePairs map;
  while (!tokenizer.IsConsumed()) {
    if (!tokenizer.Consume(';')) {
      DVLOG(1) << "Failed to find ';'";
      return;
    }

    StringView key;
    String value;
    if (!tokenizer.ConsumeToken(Mode::kNormal, key)) {
      DVLOG(1) << "Invalid content parameter name. (at " << tokenizer.Index()
               << ")";
      return;
    }
    if (!tokenizer.Consume('=')) {
      DVLOG(1) << "Failed to find '='";
      return;
    }
    if (!tokenizer.ConsumeTokenOrQuotedString(mode, value)) {
      DVLOG(1) << "Invalid content parameter value (at " << tokenizer.Index()
               << ", for '" << key.ToString() << "').";
      return;
    }
    // As |key| is parsed as a token, it consists of ascii characters
    // and hence we don't need to care about non-ascii lowercasing.
    DCHECK(key.ToString().ContainsOnlyASCII());
    String key_string = key.ToString().LowerASCII();
    if (mode == Mode::kStrict && map.find(key_string) != map.end()) {
      DVLOG(1) << "Parameter " << key_string << " is defined multiple times.";
      return;
    }
    map.Set(key_string, value);
  }
  parameters_ = std::move(map);
  is_valid_ = true;
}

}  // namespace blink
