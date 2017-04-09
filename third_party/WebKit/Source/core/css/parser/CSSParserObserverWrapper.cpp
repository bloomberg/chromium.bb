// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserObserverWrapper.h"

#include "core/css/parser/CSSParserTokenRange.h"

namespace blink {

unsigned CSSParserObserverWrapper::StartOffset(
    const CSSParserTokenRange& range) {
  return token_offsets_[range.begin() - first_parser_token_];
}

unsigned CSSParserObserverWrapper::PreviousTokenStartOffset(
    const CSSParserTokenRange& range) {
  if (range.begin() == first_parser_token_)
    return 0;
  return token_offsets_[range.begin() - first_parser_token_ - 1];
}

unsigned CSSParserObserverWrapper::EndOffset(const CSSParserTokenRange& range) {
  return token_offsets_[range.end() - first_parser_token_];
}

void CSSParserObserverWrapper::SkipCommentsBefore(
    const CSSParserTokenRange& range,
    bool leave_directly_before) {
  unsigned start_index = range.begin() - first_parser_token_;
  if (!leave_directly_before)
    start_index++;
  while (comment_iterator_ < comment_offsets_.end() &&
         comment_iterator_->tokens_before < start_index)
    comment_iterator_++;
}

void CSSParserObserverWrapper::YieldCommentsBefore(
    const CSSParserTokenRange& range) {
  unsigned start_index = range.begin() - first_parser_token_;
  while (comment_iterator_ < comment_offsets_.end() &&
         comment_iterator_->tokens_before <= start_index) {
    observer_.ObserveComment(comment_iterator_->start_offset,
                             comment_iterator_->end_offset);
    comment_iterator_++;
  }
}

}  // namespace blink
