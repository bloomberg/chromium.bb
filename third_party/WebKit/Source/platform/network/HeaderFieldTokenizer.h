// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HeaderFieldTokenizer_h
#define HeaderFieldTokenizer_h

#include "platform/PlatformExport.h"
#include "platform/network/ParsedContentHeaderFieldParameters.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

// Parses header fields into tokens, quoted strings and separators.
// Commonly used by ParsedContent* classes.
class PLATFORM_EXPORT HeaderFieldTokenizer final {
  STACK_ALLOCATED();

 public:
  using Mode = ParsedContentHeaderFieldParameters::Mode;

  explicit HeaderFieldTokenizer(const String& header_field);
  HeaderFieldTokenizer(HeaderFieldTokenizer&&);

  // Try to parse a separator character, a token or either a token or a quoted
  // string from the |header_field| input. Return |true| on success. Return
  // |false| if the separator character, the token or the quoted string is
  // missing or invalid.
  bool Consume(char);
  bool ConsumeToken(Mode, StringView& output);
  bool ConsumeTokenOrQuotedString(Mode, String& output);

  unsigned Index() const { return index_; }
  bool IsConsumed() const { return index_ >= input_.length(); }

 private:
  bool ConsumeQuotedString(String& output);
  void SkipSpaces();

  unsigned index_;
  const String input_;
};

}  // namespace blink

#endif
