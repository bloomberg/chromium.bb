// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserTokenStream_h
#define CSSParserTokenStream_h

#include "core/css/parser/CSSParserTokenBuffer.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSTokenizer.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class CSSParserScopedTokenBuffer;
class CSSParserObserverWrapper;

// A streaming interface to CSSTokenizer that tokenizes on demand.
// Abstractly, the stream ends at either EOF or the beginning/end of a block.
// To consume a block, a BlockGuard must be created first to ensure that
// we finish consuming a block even if there was an error.
//
// Methods prefixed with "Unchecked" can only be called after calls to Peek(),
// EnsureLookAhead(), or AtEnd() with no subsequent modifications to the stream
// such as a consume.
class CORE_EXPORT CSSParserTokenStream {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(CSSParserTokenStream);

 public:
  // Instantiate this to start reading from a block. When the guard is out of
  // scope, the rest of the block is consumed.
  class BlockGuard {
   public:
    explicit BlockGuard(CSSParserTokenStream& stream) : stream_(stream) {
      const CSSParserToken next = stream.ConsumeInternal();
      DCHECK_EQ(next.GetBlockType(), CSSParserToken::kBlockStart);
    }

    ~BlockGuard() {
      stream_.EnsureLookAhead();
      stream_.UncheckedSkipToEndOfBlock();
    }

   private:
    CSSParserTokenStream& stream_;
  };

  explicit CSSParserTokenStream(CSSTokenizer& tokenizer)
      : buffer_(512), tokenizer_(tokenizer), next_(kEOFToken) {}

  CSSParserTokenStream(CSSParserTokenStream&&) = default;

  inline void EnsureLookAhead() {
    if (!HasLookAhead()) {
      has_look_ahead_ = true;
      next_ = tokenizer_.TokenizeSingle();
    }
  }

  // Forcibly read a lookahead token.
  inline void LookAhead() {
    DCHECK(!HasLookAhead());
    next_ = tokenizer_.TokenizeSingle();
    has_look_ahead_ = true;
  }

  inline bool HasLookAhead() const { return has_look_ahead_; }

  inline const CSSParserToken& Peek() {
    EnsureLookAhead();
    return next_;
  }

  inline const CSSParserToken& UncheckedPeek() const {
    DCHECK(HasLookAhead());
    return next_;
  }

  inline const CSSParserToken& Consume() {
    EnsureLookAhead();
    return UncheckedConsume();
  }

  const CSSParserToken& UncheckedConsume() {
    DCHECK(HasLookAhead());
    DCHECK_NE(next_.GetBlockType(), CSSParserToken::kBlockStart);
    DCHECK_NE(next_.GetBlockType(), CSSParserToken::kBlockEnd);
    has_look_ahead_ = false;
    offset_ = tokenizer_.Offset();
    return next_;
  }

  inline bool AtEnd() {
    EnsureLookAhead();
    return UncheckedAtEnd();
  }

  inline bool UncheckedAtEnd() const {
    DCHECK(HasLookAhead());
    return next_.IsEOF() || next_.GetBlockType() == CSSParserToken::kBlockEnd;
  }

  // Get the index of the character in the original string to be consumed next.
  size_t Offset() const { return offset_; }

  // Get the index of the starting character of the look-ahead token.
  size_t LookAheadOffset() const {
    DCHECK(HasLookAhead());
    return tokenizer_.PreviousOffset();
  }

  void ConsumeWhitespace();
  CSSParserToken ConsumeIncludingWhitespace();
  void UncheckedConsumeComponentValue();
  void UncheckedConsumeComponentValue(CSSParserScopedTokenBuffer&);
  void UncheckedConsumeComponentValueWithOffsets(CSSParserObserverWrapper&,
                                                 CSSParserScopedTokenBuffer&);
  // Either consumes a comment token and returns true, or peeks at the next
  // token and return false.
  bool ConsumeCommentOrNothing();

 private:
  friend class CSSParserScopedTokenBuffer;

  const CSSParserToken& PeekInternal() {
    EnsureLookAhead();
    return UncheckedPeekInternal();
  }

  const CSSParserToken& UncheckedPeekInternal() const {
    DCHECK(HasLookAhead());
    return next_;
  }

  const CSSParserToken& ConsumeInternal() {
    EnsureLookAhead();
    return UncheckedConsumeInternal();
  }

  const CSSParserToken& UncheckedConsumeInternal() {
    DCHECK(HasLookAhead());
    has_look_ahead_ = false;
    offset_ = tokenizer_.Offset();
    return next_;
  }

  void UncheckedSkipToEndOfBlock();

  CSSParserTokenBuffer buffer_;
  CSSTokenizer& tokenizer_;
  CSSParserToken next_;
  size_t offset_ = 0;
  bool has_look_ahead_ = false;
};

}  // namespace blink

#endif  // CSSParserTokenStream_h
