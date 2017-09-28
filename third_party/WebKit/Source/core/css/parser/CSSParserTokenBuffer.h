// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserTokenBuffer_h
#define CSSParserTokenBuffer_h

#include "core/css/parser/CSSParserTokenRange.h"

namespace blink {

// Represents a container of CSSParserTokens which is designed to work
// efficiently with CSSTokenRangeBuffers. Most notably, clearing the container
// doesn't deallocate the tokens to avoid memory churn.
// TODO(shend): Move this class to be nested inside CSSParserTokenStream once
// CSSParserScopedTokenBuffer is gone.
class CSSParserTokenBuffer {
 public:
  CSSParserTokenBuffer(size_t reserve = 0) : size_(0) {
    tokens_.ReserveInitialCapacity(reserve);
  }

  void push_back(const CSSParserToken& token) {
    if (size_ == tokens_.size())
      tokens_.push_back(token);
    else
      tokens_[size_] = token;
    size_++;
  }

  void clear() { size_ = 0; }

  size_t size() const { return size_; }

  CSSParserTokenRange Range() const {
    return CSSParserTokenRange(tokens_).MakeSubRange(tokens_.begin(),
                                                     tokens_.begin() + size_);
  }

 private:
  Vector<CSSParserToken, 32> tokens_;
  size_t size_;
};

}  // namespace blink

#endif  // CSSParserTokenBuffer_h
