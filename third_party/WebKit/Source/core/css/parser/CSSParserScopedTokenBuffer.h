// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserScopedTokenBuffer_h
#define CSSParserScopedTokenBuffer_h

#include "core/css/parser/CSSParserTokenStream.h"

namespace blink {

// Represents a vector of CSSParserTokens, used to create CSSParserTokenRanges.
// This class owns the tokens that it stores, destroying them when it goes out
// of scope.
//
// Only one CSSParserScopedTokenBuffer can be active at any point in time per
// stream.
// TODO(shend): Remove this class completely when selector parsing is done using
// streams.
class CSSParserScopedTokenBuffer {
  WTF_MAKE_NONCOPYABLE(CSSParserScopedTokenBuffer);

 public:
  CSSParserScopedTokenBuffer(CSSParserTokenStream& stream)
      : buffer_(stream.buffer_) {
    DCHECK_EQ(buffer_.size(), 0U);
  }

  CSSParserScopedTokenBuffer(CSSParserScopedTokenBuffer&& other)
      : buffer_(other.buffer_), num_tokens_(other.num_tokens_) {
    other.num_tokens_ = 0;
  }

  ~CSSParserScopedTokenBuffer() { Release(); }

  // This invalidates any ranges created from this buffer.
  void Append(const CSSParserToken& token) {
    DCHECK_EQ(buffer_.size(), num_tokens_);
    buffer_.push_back(token);
    ++num_tokens_;
  }

  CSSParserTokenRange Range() const { return buffer_.Range(); }

  void Release() {
    DCHECK_EQ(buffer_.size(), num_tokens_);
    buffer_.clear();
    num_tokens_ = 0;
  }

 private:
  CSSParserTokenBuffer& buffer_;
  size_t num_tokens_ = 0;  // For DCHECKs
};

}  // namespace blink

#endif  // CSSParserScopedTokenBuffer_h
