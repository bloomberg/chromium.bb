// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSLazyPropertyParserImpl.h"

#include "core/css/parser/CSSLazyParsingState.h"
#include "core/css/parser/CSSParserImpl.h"

namespace blink {

CSSLazyPropertyParserImpl::CSSLazyPropertyParserImpl(size_t offset,
                                                     CSSLazyParsingState* state)
    : CSSLazyPropertyParser(), offset_(offset), lazy_state_(state) {}

StylePropertySet* CSSLazyPropertyParserImpl::ParseProperties() {
  lazy_state_->CountRuleParsed();
  return CSSParserImpl::ParseDeclarationListForLazyStyle(
      lazy_state_->SheetText(), offset_, lazy_state_->Context());
}

}  // namespace blink
