// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSLazyPropertyParserImpl.h"

#include "core/css/parser/CSSLazyParsingState.h"
#include "core/css/parser/CSSParserImpl.h"

namespace blink {

CSSLazyPropertyParserImpl::CSSLazyPropertyParserImpl(CSSParserTokenRange block,
                                                     CSSLazyParsingState* state)
    : CSSLazyPropertyParser(), lazy_state_(state) {
  // Reserve capacity to minimize heap bloat.
  size_t length = block.end() - block.begin();
  tokens_.ReserveCapacity(length);
  tokens_.Append(block.begin(), length);
}

StylePropertySet* CSSLazyPropertyParserImpl::ParseProperties() {
  lazy_state_->CountRuleParsed();
  return CSSParserImpl::ParseDeclarationListForLazyStyle(
      tokens_, lazy_state_->Context());
}

}  // namespace blink
