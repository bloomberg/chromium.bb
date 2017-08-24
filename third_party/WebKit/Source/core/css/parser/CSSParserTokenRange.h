// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserTokenRange_h
#define CSSParserTokenRange_h

#include "core/CoreExport.h"
#include "core/css/parser/CSSParserToken.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"

namespace blink {

CORE_EXPORT extern const CSSParserToken& g_static_eof_token;

// A CSSParserTokenRange is an iterator over a subrange of a vector of
// CSSParserTokens. Accessing outside of the range will return an endless stream
// of EOF tokens. This class refers to half-open intervals [first, last).
class CORE_EXPORT CSSParserTokenRange {
  DISALLOW_NEW();

 public:
  CSSParserTokenRange(const Vector<CSSParserToken>& buffer)
      : buffer_(&buffer), first_(0), last_(buffer.size()) {}

  // This should be called on a range with tokens returned by that range.
  CSSParserTokenRange MakeSubRange(const CSSParserToken* first,
                                   const CSSParserToken* last) const;

  bool AtEnd() const { return first_ == last_; }
  const CSSParserToken* end() const { return buffer_->begin() + last_; }

  const CSSParserToken& Peek(unsigned offset = 0) const {
    if (first_ + offset >= last_)
      return g_static_eof_token;
    return (*buffer_)[first_ + offset];
  }

  const CSSParserToken& Consume() {
    if (first_ == last_)
      return g_static_eof_token;
    return (*buffer_)[first_++];
  }

  const CSSParserToken& ConsumeIncludingWhitespace() {
    const CSSParserToken& result = Consume();
    ConsumeWhitespace();
    return result;
  }

  // The returned range doesn't include the brackets
  CSSParserTokenRange ConsumeBlock();

  void ConsumeComponentValue();

  void ConsumeWhitespace() {
    while (Peek().GetType() == kWhitespaceToken)
      ++first_;
  }

  String Serialize() const;

  const CSSParserToken* begin() const { return buffer_->begin() + first_; }

  static void InitStaticEOFToken();

 private:
  CSSParserTokenRange(const Vector<CSSParserToken>& buffer,
                      size_t first,
                      size_t last)
      : buffer_(&buffer), first_(first), last_(last) {
    DCHECK_LE(first_, buffer_->size());
    DCHECK_LE(last_, buffer_->size());
    DCHECK_LE(first_, last_);
  }

  // This is a non-null pointer to make this class copy-assignable
  const Vector<CSSParserToken>* buffer_;
  size_t first_;
  size_t last_;
};

}  // namespace blink

#endif  // CSSParserTokenRange_h
