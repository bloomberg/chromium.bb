// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserTokenStream_h
#define CSSParserTokenStream_h

#include "CSSTokenizer.h"

namespace blink {

// A streaming interface to CSSTokenizer that tokenizes on demand.
class CSSParserTokenStream {
  DISALLOW_NEW();

 public:
  explicit CSSParserTokenStream(CSSTokenizer& tokenizer)
      : tokenizer_(tokenizer) {
    DCHECK_EQ(tokenizer.tokens_.size(), 0U);
  }

  CSSParserTokenRange MakeRangeToEOF() { return tokenizer_.TokenRange(); }

 private:
  CSSTokenizer& tokenizer_;
};

}  // namespace blink

#endif  // CSSParserTokenStream_h
