// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserTokenRange.h"

#include "platform/wtf/StaticConstructors.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

DEFINE_GLOBAL(CSSParserToken, g_static_eof_token);

void CSSParserTokenRange::InitStaticEOFToken() {
  new ((void*)&g_static_eof_token) CSSParserToken(kEOFToken);
}

CSSParserTokenRange CSSParserTokenRange::MakeSubRange(
    const CSSParserToken* first,
    const CSSParserToken* last) const {
  // Convert first and last pointers into indices.
  size_t sub_range_first =
      (first == &g_static_eof_token) ? last_ : first - buffer_->begin();
  size_t sub_range_last =
      (last == &g_static_eof_token) ? last_ : last - buffer_->begin();
  return CSSParserTokenRange(*buffer_, sub_range_first, sub_range_last);
}

CSSParserTokenRange CSSParserTokenRange::ConsumeBlock() {
  DCHECK_EQ(Peek().GetBlockType(), CSSParserToken::kBlockStart);
  const CSSParserToken* start = &Peek() + 1;
  unsigned nesting_level = 0;
  do {
    const CSSParserToken& token = Consume();
    if (token.GetBlockType() == CSSParserToken::kBlockStart)
      nesting_level++;
    else if (token.GetBlockType() == CSSParserToken::kBlockEnd)
      nesting_level--;
  } while (nesting_level && first_ < last_);

  if (nesting_level)
    return MakeSubRange(start, begin());  // Ended at EOF
  return MakeSubRange(start, begin() - 1);
}

void CSSParserTokenRange::ConsumeComponentValue() {
  // FIXME: This is going to do multiple passes over large sections of a
  // stylesheet. We should consider optimising this by precomputing where each
  // block ends.
  unsigned nesting_level = 0;
  do {
    const CSSParserToken& token = Consume();
    if (token.GetBlockType() == CSSParserToken::kBlockStart)
      nesting_level++;
    else if (token.GetBlockType() == CSSParserToken::kBlockEnd)
      nesting_level--;
  } while (nesting_level && first_ < last_);
}

String CSSParserTokenRange::Serialize() const {
  // We're supposed to insert comments between certain pairs of token types
  // as per spec, but since this is currently only used for @supports CSSOM
  // we just get these cases wrong and avoid the additional complexity.
  StringBuilder builder;
  for (const CSSParserToken* it = begin(); it != end(); ++it)
    it->Serialize(builder);
  return builder.ToString();
}

}  // namespace blink
