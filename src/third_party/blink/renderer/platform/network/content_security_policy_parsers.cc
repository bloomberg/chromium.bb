// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"

#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

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

}  // namespace blink
