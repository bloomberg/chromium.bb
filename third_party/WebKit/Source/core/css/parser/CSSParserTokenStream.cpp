// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserTokenStream.h"

namespace blink {

void CSSParserTokenStream::UncheckedConsumeComponentValue() {
  unsigned nesting_level = 0;
  do {
    const CSSParserToken& token = UncheckedConsume();
    if (token.GetBlockType() == CSSParserToken::kBlockStart)
      nesting_level++;
    else if (token.GetBlockType() == CSSParserToken::kBlockEnd)
      nesting_level--;
  } while (nesting_level && !AtEnd());
}

}  // namespace blink
