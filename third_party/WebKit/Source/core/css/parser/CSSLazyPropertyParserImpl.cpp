// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSLazyPropertyParserImpl.h"

#include "core/css/StyleEngine.h"
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
  StylePropertySet* property_set =
      CSSParserImpl::ParseDeclarationListForLazyStyle(tokens_,
                                                      lazy_state_->Context());
  if (has_before_or_after_ && lazy_state_->HasRuleSet() &&
      property_set->FindPropertyIndex(CSSPropertyContent) != -1) {
    lazy_state_->GetRuleSet().UpdateInvalidationSetsForContentAttribute(
        property_set);
    lazy_state_->GetStyleEngine().MarkGlobalRuleSetDirty();
  }
  return property_set;
}

}  // namespace blink
