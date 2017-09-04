// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserTokenStream.h"

namespace blink {

void CSSParserTokenStream::ConsumeWhitespace() {
  while (Peek().GetType() == kWhitespaceToken)
    UncheckedConsume();
}

CSSParserToken CSSParserTokenStream::ConsumeIncludingWhitespace() {
  CSSParserToken result = Consume();
  ConsumeWhitespace();
  return result;
}

void CSSParserTokenStream::UncheckedConsumeComponentValue(
    unsigned nesting_level) {
  DCHECK(HasLookAhead());

  // Have to use internal consume/peek in here because they can read past
  // start/end of blocks
  do {
    const CSSParserToken& token = UncheckedConsumeInternal();
    if (token.GetBlockType() == CSSParserToken::kBlockStart)
      nesting_level++;
    else if (token.GetBlockType() == CSSParserToken::kBlockEnd)
      nesting_level--;
  } while (nesting_level && !PeekInternal().IsEOF());
}

}  // namespace blink
