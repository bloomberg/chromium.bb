// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserObserverWrapper_h
#define CSSParserObserverWrapper_h

#include "core/css/parser/CSSParserObserver.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserToken;
class CSSParserTokenRange;

class CSSParserObserverWrapper {
  STACK_ALLOCATED();

 public:
  explicit CSSParserObserverWrapper(CSSParserObserver& observer)
      : observer_(observer) {}

  unsigned StartOffset(const CSSParserTokenRange&);
  unsigned PreviousTokenStartOffset(const CSSParserTokenRange&);
  unsigned EndOffset(const CSSParserTokenRange&);  // Includes trailing comments

  void SkipCommentsBefore(const CSSParserTokenRange&,
                          bool leave_directly_before);
  void YieldCommentsBefore(const CSSParserTokenRange&);

  CSSParserObserver& Observer() { return observer_; }
  void AddComment(unsigned start_offset,
                  unsigned end_offset,
                  unsigned tokens_before) {
    CommentPosition position = {start_offset, end_offset, tokens_before};
    comment_offsets_.push_back(position);
  }
  void AddToken(unsigned start_offset) {
    token_offsets_.push_back(start_offset);
  }
  void FinalizeConstruction(CSSParserToken* first_parser_token) {
    first_parser_token_ = first_parser_token;
    comment_iterator_ = comment_offsets_.begin();
  }

 private:
  CSSParserObserver& observer_;
  Vector<unsigned> token_offsets_;
  CSSParserToken* first_parser_token_;

  struct CommentPosition {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    unsigned start_offset;
    unsigned end_offset;
    unsigned tokens_before;
  };

  Vector<CommentPosition> comment_offsets_;
  Vector<CommentPosition>::iterator comment_iterator_;
};

}  // namespace blink

#endif  // CSSParserObserverWrapper_h
