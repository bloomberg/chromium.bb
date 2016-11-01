// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSLazyParsingState.h"
#include "core/frame/UseCounter.h"

namespace blink {

CSSLazyParsingState::CSSLazyParsingState(const CSSParserContext& context,
                                         Vector<String> escapedStrings,
                                         const String& sheetText,
                                         StyleSheetContents* contents)
    : m_context(context),
      m_escapedStrings(std::move(escapedStrings)),
      m_sheetText(sheetText),
      m_owningContents(contents) {}

const CSSParserContext& CSSLazyParsingState::context() {
  DCHECK(m_owningContents);
  UseCounter* sheetCounter = UseCounter::getFrom(m_owningContents);
  if (sheetCounter != m_context.useCounter())
    m_context = CSSParserContext(m_context, sheetCounter);
  return m_context;
}

//  Disallow lazy parsing for blocks which have
//  - before/after in their selector list. This ensures we don't cause a
//  collectFeatures() when we trigger parsing for attr() functions which would
//  trigger expensive invalidation propagation.
//
//  Note: another optimization might be to disallow lazy parsing for rules which
//  will end up being empty. This is hard to know without parsing but we may be
//  able to catch the {}, { } cases. This would be to hit
//  StyleRule::shouldConsiderForMatchingRules more.
bool CSSLazyParsingState::shouldLazilyParseProperties(
    const CSSSelectorList& selectors) {
  for (const auto* s = selectors.first(); s; s = CSSSelectorList::next(*s)) {
    const CSSSelector::PseudoType type(s->getPseudoType());
    if (type == CSSSelector::PseudoBefore || type == CSSSelector::PseudoAfter)
      return false;
  }
  return true;
}

}  // namespace blink
