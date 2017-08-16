// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserTokenStream_h
#define CSSParserTokenStream_h

#include "CSSTokenizer.h"
#include "core/css/parser/CSSParserTokenRange.h"

namespace blink {

// A streaming interface to CSSTokenizer that tokenizes on demand.
// Methods prefixed with "Unchecked" can only be called after Peek()
// returns a non-EOF token or after AtEnd() returns false, with no
// subsequent modifications to the stream such as a consume.
class CORE_EXPORT CSSParserTokenStream {
  DISALLOW_NEW();

 public:
  explicit CSSParserTokenStream(CSSTokenizer& tokenizer)
      : tokenizer_(tokenizer), next_index_(0) {
    DCHECK_EQ(tokenizer.tokens_.size(), 0U);
  }

  // TODO(shend): Remove this method. We should never convert from a range to a
  // stream. We can remove this once all the functions in CSSParserImpl.h accept
  // streams.
  void UpdatePositionFromRange(const CSSParserTokenRange& range) {
    next_index_ = range.begin() - tokenizer_.tokens_.begin();
  }

  const CSSParserToken& Peek() const {
    if (next_index_ == tokenizer_.tokens_.size()) {
      // Reached end of token buffer, but might not be end of input.
      if (tokenizer_.TokenizeSingle().IsEOF())
        return g_static_eof_token;
    }
    DCHECK_LT(next_index_, tokenizer_.tokens_.size());
    return UncheckedPeek();
  }

  const CSSParserToken& UncheckedPeek() const {
    DCHECK_LT(next_index_, tokenizer_.tokens_.size());
    return tokenizer_.tokens_[next_index_];
  }

  const CSSParserToken& Consume() {
    const CSSParserToken& token = Peek();
    if (!token.IsEOF())
      next_index_++;

    DCHECK_LE(next_index_, tokenizer_.tokens_.size());
    return token;
  }

  const CSSParserToken& UncheckedConsume() {
    DCHECK_LE(next_index_, tokenizer_.tokens_.size());
    return tokenizer_.tokens_[next_index_++];
  }

  bool AtEnd() const { return Peek().IsEOF(); }

  // Range represents all tokens from current position to EOF.
  // Eagerly consumes all the remaining input.
  // TODO(shend): Remove this method once we switch over to using streams
  // completely.
  CSSParserTokenRange MakeRangeToEOF() {
    const auto range = tokenizer_.TokenRange();
    return range.MakeSubRange(tokenizer_.tokens_.begin() + next_index_,
                              tokenizer_.tokens_.end());
  }

  void UncheckedConsumeComponentValue();

 private:
  CSSTokenizer& tokenizer_;
  size_t next_index_;  // Index of next token to be consumed.
};

}  // namespace blink

#endif  // CSSParserTokenStream_h
