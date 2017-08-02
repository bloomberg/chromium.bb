// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/ContentSecurityPolicyParsers.h"

#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/text/StringUTF8Adaptor.h"
#include "public/platform/WebContentSecurityPolicy.h"

namespace blink {

bool IsCSPDirectiveNameCharacter(UChar c) {
  return IsASCIIAlphanumeric(c) || c == '-';
}

bool IsCSPDirectiveValueCharacter(UChar c) {
  return IsASCIISpace(c) || (c >= 0x21 && c <= 0x7e);  // Whitespace + VCHAR
}

// Only checks for general Base64(url) encoded chars, not '=' chars since '=' is
// positional and may only appear at the end of a Base64 encoded string.
bool IsBase64EncodedCharacter(UChar c) {
  return IsASCIIAlphanumeric(c) || c == '+' || c == '/' || c == '-' || c == '_';
}

bool IsNonceCharacter(UChar c) {
  return IsBase64EncodedCharacter(c) || c == '=';
}

bool IsSourceCharacter(UChar c) {
  return !IsASCIISpace(c);
}

bool IsPathComponentCharacter(UChar c) {
  return c != '?' && c != '#';
}

bool IsHostCharacter(UChar c) {
  return IsASCIIAlphanumeric(c) || c == '-';
}

bool IsSchemeContinuationCharacter(UChar c) {
  return IsASCIIAlphanumeric(c) || c == '+' || c == '-' || c == '.';
}

bool IsNotASCIISpace(UChar c) {
  return !IsASCIISpace(c);
}

bool IsNotColonOrSlash(UChar c) {
  return c != ':' && c != '/';
}

bool IsMediaTypeCharacter(UChar c) {
  return !IsASCIISpace(c) && c != '/';
}

STATIC_ASSERT_ENUM(kWebContentSecurityPolicyTypeReport,
                   kContentSecurityPolicyHeaderTypeReport);
STATIC_ASSERT_ENUM(kWebContentSecurityPolicyTypeEnforce,
                   kContentSecurityPolicyHeaderTypeEnforce);

STATIC_ASSERT_ENUM(kWebContentSecurityPolicySourceHTTP,
                   kContentSecurityPolicyHeaderSourceHTTP);
STATIC_ASSERT_ENUM(kWebContentSecurityPolicySourceMeta,
                   kContentSecurityPolicyHeaderSourceMeta);
}  // namespace blink
