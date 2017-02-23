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

#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringView.h"

namespace blink {

using Mode = ParsedContentType::Mode;

namespace {

bool isTokenCharacter(Mode mode, UChar c) {
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
      return mode == Mode::Relaxed;
    default:
      return true;
  }
}

bool consume(char c, const String& input, unsigned& index) {
  DCHECK_NE(c, ' ');
  while (index < input.length() && input[index] == ' ')
    ++index;

  if (index < input.length() && input[index] == c) {
    ++index;
    return true;
  }
  return false;
}

bool consumeToken(Mode mode,
                  const String& input,
                  unsigned& index,
                  StringView& output) {
  DCHECK(output.isNull());

  while (index < input.length() && input[index] == ' ')
    ++index;

  auto start = index;
  while (index < input.length() && isTokenCharacter(mode, input[index]))
    ++index;

  if (start == index)
    return false;

  output = StringView(input, start, index - start);
  return true;
}

bool consumeQuotedString(const String& input, unsigned& index, String& output) {
  StringBuilder builder;
  DCHECK_EQ('"', input[index]);
  ++index;
  while (index < input.length()) {
    if (input[index] == '\\') {
      ++index;
      if (index == input.length())
        return false;
      builder.append(input[index]);
      ++index;
      continue;
    }
    if (input[index] == '"') {
      output = builder.toString();
      ++index;
      return true;
    }
    builder.append(input[index]);
    ++index;
  }
  return false;
}

bool consumeTokenOrQuotedString(Mode mode,
                                const String& input,
                                unsigned& index,
                                String& output) {
  while (index < input.length() && input[index] == ' ')
    ++index;
  if (input.length() == index)
    return false;
  if (input[index] == '"') {
    return consumeQuotedString(input, index, output);
  }
  StringView view;
  auto result = consumeToken(mode, input, index, view);
  output = view.toString();
  return result;
}

bool isEnd(const String& input, unsigned index) {
  while (index < input.length()) {
    if (input[index] != ' ')
      return false;
    ++index;
  }
  return true;
}

}  // namespace

ParsedContentType::ParsedContentType(const String& contentType, Mode mode)
    : m_mode(mode) {
  m_isValid = parse(contentType);
}

String ParsedContentType::charset() const {
  return parameterValueForName("charset");
}

String ParsedContentType::parameterValueForName(const String& name) const {
  if (!name.containsOnlyASCII())
    return String();
  return m_parameters.at(name.lower());
}

size_t ParsedContentType::parameterCount() const {
  return m_parameters.size();
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

bool ParsedContentType::parse(const String& contentType) {
  unsigned index = 0;

  StringView type, subtype;
  if (!consumeToken(Mode::Normal, contentType, index, type)) {
    DVLOG(1) << "Failed to find `type' in '" << contentType << "'";
    return false;
  }
  if (!consume('/', contentType, index)) {
    DVLOG(1) << "Failed to find '/' in '" << contentType << "'";
    return false;
  }
  if (!consumeToken(Mode::Normal, contentType, index, subtype)) {
    DVLOG(1) << "Failed to find `type' in '" << contentType << "'";
    return false;
  }

  StringBuilder builder;
  builder.append(type);
  builder.append('/');
  builder.append(subtype);
  m_mimeType = builder.toString();

  KeyValuePairs map;
  while (!isEnd(contentType, index)) {
    if (!consume(';', contentType, index)) {
      DVLOG(1) << "Failed to find ';'";
      return false;
    }

    StringView key;
    String value;
    if (!consumeToken(Mode::Normal, contentType, index, key)) {
      DVLOG(1) << "Invalid Content-Type parameter name. (at " << index << ")";
      return false;
    }
    if (!consume('=', contentType, index)) {
      DVLOG(1) << "Failed to find '='";
      return false;
    }
    if (!consumeTokenOrQuotedString(m_mode, contentType, index, value)) {
      DVLOG(1) << "Invalid Content-Type, invalid parameter value (at " << index
               << ", for '" << key.toString() << "').";
      return false;
    }
    String keyString = key.toString();
    // As |key| is parsed as a token, it consists of ascii characters
    // and hence we don't need to care about non-ascii lowercasing.
    DCHECK(keyString.containsOnlyASCII());
    map.set(keyString.lower(), value);
  }
  m_parameters = std::move(map);
  return true;
}

}  // namespace blink
