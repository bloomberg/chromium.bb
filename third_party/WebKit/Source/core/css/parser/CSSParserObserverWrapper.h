// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserObserverWrapper_h
#define CSSParserObserverWrapper_h

#include "core/css/parser/CSSParserObserver.h"
#include "core/css/parser/CSSParserTokenStream.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserToken;
class CSSParserTokenRange;

// TODO(shend): Remove this class once we can use stream parsing directly
// with CSSParserObserver.
class CSSParserObserverWrapper {
  STACK_ALLOCATED();

 public:
  explicit CSSParserObserverWrapper(CSSParserObserver& observer)
      : observer_(observer) {}

  unsigned StartOffset(const CSSParserTokenRange&);
  unsigned EndOffset(const CSSParserTokenRange&);

  CSSParserObserver& Observer() { return observer_; }

  void AddToken(unsigned start_offset) {
    token_offsets_.push_back(start_offset);
  }

  void StartConstruction() {
    token_offsets_.clear();
    first_parser_token_ = nullptr;
  }
  void FinalizeConstruction(const CSSParserToken* first_parser_token) {
    first_parser_token_ = first_parser_token;
  }

 private:
  CSSParserObserver& observer_;
  Vector<unsigned> token_offsets_;
  const CSSParserToken* first_parser_token_;
};

}  // namespace blink

#endif  // CSSParserObserverWrapper_h
