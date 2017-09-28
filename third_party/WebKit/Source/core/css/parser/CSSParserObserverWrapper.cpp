// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserObserverWrapper.h"

#include "core/css/parser/CSSParserTokenRange.h"

namespace blink {

unsigned CSSParserObserverWrapper::StartOffset(
    const CSSParserTokenRange& range) {
  const size_t index = range.begin() - first_parser_token_;
  DCHECK_LT(index, token_offsets_.size());
  return token_offsets_[range.begin() - first_parser_token_];
}

unsigned CSSParserObserverWrapper::EndOffset(const CSSParserTokenRange& range) {
  size_t index = range.end() - first_parser_token_;
  DCHECK_LT(index, token_offsets_.size());
  return token_offsets_[index];
}

}  // namespace blink
