/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/network/ParsedContentType.h"

#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/StringView.h"

namespace blink {

using Mode = ParsedContentType::Mode;

namespace {

bool IsTokenCharacter(Mode mode, UChar c) {
  if (c >= 128)
    return false;
  if (c < 0x20)
    return false;

  switch (c) {
    case ' ':
    case ';':
    case '"':
      return false;
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ',':
    case ':':
    case '\\':
    case '/':
    case '[':
    case ']':
    case '?':
    case '=':
      return mode == Mode::kRelaxed;
    default:
      return true;
  }
}

bool Consume(char c, const String& input, unsigned& index) {
  DCHECK_NE(c, ' ');
  while (index < input.length() && input[index] == ' ')
    ++index;

  if (index < input.length() && input[index] == c) {
    ++index;
    return true;
  }
  return false;
}

bool ConsumeToken(Mode mode,
                  const String& input,
                  unsigned& index,
                  StringView& output) {
  DCHECK(output.IsNull());

  while (index < input.length() && input[index] == ' ')
    ++index;

  auto start = index;
  while (index < input.length() && IsTokenCharacter(mode, input[index]))
    ++index;

  if (start == index)
    return false;

  output = StringView(input, start, index - start);
  return true;
}

bool ConsumeQuotedString(const String& input, unsigned& index, String& output) {
  StringBuilder builder;
  DCHECK_EQ('"', input[index]);
  ++index;
  while (index < input.length()) {
    if (input[index] == '\\') {
      ++index;
      if (index == input.length())
        return false;
      builder.Append(input[index]);
      ++index;
      continue;
    }
    if (input[index] == '"') {
      output = builder.ToString();
      ++index;
      return true;
    }
    builder.Append(input[index]);
    ++index;
  }
  return false;
}

bool ConsumeTokenOrQuotedString(Mode mode,
                                const String& input,
                                unsigned& index,
                                String& output) {
  while (index < input.length() && input[index] == ' ')
    ++index;
  if (input.length() == index)
    return false;
  if (input[index] == '"') {
    return ConsumeQuotedString(input, index, output);
  }
  StringView view;
  auto result = ConsumeToken(mode, input, index, view);
  output = view.ToString();
  return result;
}

bool IsEnd(const String& input, unsigned index) {
  while (index < input.length()) {
    if (input[index] != ' ')
      return false;
    ++index;
  }
  return true;
}

}  // namespace

ParsedContentType::ParsedContentType(const String& content_type, Mode mode)
    : mode_(mode) {
  is_valid_ = Parse(content_type);
}

String ParsedContentType::Charset() const {
  return ParameterValueForName("charset");
}

String ParsedContentType::ParameterValueForName(const String& name) const {
  if (!name.ContainsOnlyASCII())
    return String();
  return parameters_.at(name.Lower());
}

size_t ParsedContentType::ParameterCount() const {
  return parameters_.size();
}

// From http://tools.ietf.org/html/rfc2045#section-5.1:
//
// content := "Content-Type" ":" type "/" subtype
//            *(";" parameter)
//            ; Matching of media type and subtype
//            ; is ALWAYS case-insensitive.
//
// type := discrete-type / composite-type
//
// discrete-type := "text" / "image" / "audio" / "video" /
//                  "application" / extension-token
//
// composite-type := "message" / "multipart" / extension-token
//
// extension-token := ietf-token / x-token
//
// ietf-token := <An extension token defined by a
//                standards-track RFC and registered
//                with IANA.>
//
// x-token := <The two characters "X-" or "x-" followed, with
//             no intervening white space, by any token>
//
// subtype := extension-token / iana-token
//
// iana-token := <A publicly-defined extension token. Tokens
//                of this form must be registered with IANA
//                as specified in RFC 2048.>
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

bool ParsedContentType::Parse(const String& content_type) {
  unsigned index = 0;

  StringView type, subtype;
  if (!ConsumeToken(Mode::kNormal, content_type, index, type)) {
    DVLOG(1) << "Failed to find `type' in '" << content_type << "'";
    return false;
  }
  if (!Consume('/', content_type, index)) {
    DVLOG(1) << "Failed to find '/' in '" << content_type << "'";
    return false;
  }
  if (!ConsumeToken(Mode::kNormal, content_type, index, subtype)) {
    DVLOG(1) << "Failed to find `type' in '" << content_type << "'";
    return false;
  }

  StringBuilder builder;
  builder.Append(type);
  builder.Append('/');
  builder.Append(subtype);
  mime_type_ = builder.ToString();

  KeyValuePairs map;
  while (!IsEnd(content_type, index)) {
    if (!Consume(';', content_type, index)) {
      DVLOG(1) << "Failed to find ';'";
      return false;
    }

    StringView key;
    String value;
    if (!ConsumeToken(Mode::kNormal, content_type, index, key)) {
      DVLOG(1) << "Invalid Content-Type parameter name. (at " << index << ")";
      return false;
    }
    if (!Consume('=', content_type, index)) {
      DVLOG(1) << "Failed to find '='";
      return false;
    }
    if (!ConsumeTokenOrQuotedString(mode_, content_type, index, value)) {
      DVLOG(1) << "Invalid Content-Type, invalid parameter value (at " << index
               << ", for '" << key.ToString() << "').";
      return false;
    }
    // As |key| is parsed as a token, it consists of ascii characters
    // and hence we don't need to care about non-ascii lowercasing.
    DCHECK(key.ToString().ContainsOnlyASCII());
    String key_string = key.ToString().Lower();
    if (mode_ == Mode::kStrict && map.Find(key_string) != map.end()) {
      DVLOG(1) << "Parameter " << key_string << " is defined multiple times.";
      return false;
    }
    map.Set(key_string, value);
  }
  parameters_ = std::move(map);
  return true;
}

}  // namespace blink
