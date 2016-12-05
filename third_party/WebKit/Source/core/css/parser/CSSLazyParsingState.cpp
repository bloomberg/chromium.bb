// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSLazyParsingState.h"
#include "core/css/parser/CSSLazyPropertyParserImpl.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/frame/UseCounter.h"
#include "platform/Histogram.h"

namespace blink {

CSSLazyParsingState::CSSLazyParsingState(const CSSParserContext& context,
                                         Vector<String> escapedStrings,
                                         const String& sheetText,
                                         StyleSheetContents* contents)
    : m_context(context),
      m_escapedStrings(std::move(escapedStrings)),
      m_sheetText(sheetText),
      m_owningContents(contents),
      m_parsedStyleRules(0),
      m_totalStyleRules(0),
      m_styleRulesNeededForNextMilestone(0),
      m_usage(UsageGe0) {
  recordUsageMetrics();
}

CSSLazyPropertyParserImpl* CSSLazyParsingState::createLazyParser(
    const CSSParserTokenRange& block) {
  ++m_totalStyleRules;
  return new CSSLazyPropertyParserImpl(std::move(block), this);
}

const CSSParserContext& CSSLazyParsingState::context() {
  DCHECK(m_owningContents);
  UseCounter* sheetCounter = UseCounter::getFrom(m_owningContents);
  if (sheetCounter != m_context.useCounter())
    m_context = CSSParserContext(m_context, sheetCounter);
  return m_context;
}

void CSSLazyParsingState::countRuleParsed() {
  ++m_parsedStyleRules;
  while (m_parsedStyleRules > m_styleRulesNeededForNextMilestone) {
    DCHECK_NE(UsageAll, m_usage);
    ++m_usage;
    recordUsageMetrics();
  }
}

bool CSSLazyParsingState::shouldLazilyParseProperties(
    const CSSSelectorList& selectors,
    const CSSParserTokenRange& block) const {
  // Simple heuristic for an empty block. Note that |block| here does not
  // include {} brackets. We avoid lazy parsing empty blocks so we can avoid
  // considering them when possible for matching. Lazy blocks must always be
  // considered. Three tokens is a reasonable minimum for a block:
  // ident ':' <value>.
  if (block.end() - block.begin() <= 2)
    return false;

  //  Disallow lazy parsing for blocks which have before/after in their selector
  //  list. This ensures we don't cause a collectFeatures() when we trigger
  //  parsing for attr() functions which would trigger expensive invalidation
  //  propagation.
  for (const auto* s = selectors.first(); s; s = CSSSelectorList::next(*s)) {
    for (const CSSSelector* current = s; current;
         current = current->tagHistory()) {
      const CSSSelector::PseudoType type(current->getPseudoType());
      if (type == CSSSelector::PseudoBefore || type == CSSSelector::PseudoAfter)
        return false;
      if (current->relation() != CSSSelector::SubSelector)
        break;
    }
  }
  return true;
}

void CSSLazyParsingState::recordUsageMetrics() {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, usageHistogram,
                      ("Style.LazyUsage.Percent", UsageLastValue));
  switch (m_usage) {
    case UsageGe0:
      m_styleRulesNeededForNextMilestone = m_totalStyleRules * .1;
      break;
    case UsageGt10:
      m_styleRulesNeededForNextMilestone = m_totalStyleRules * .25;
      break;
    case UsageGt25:
      m_styleRulesNeededForNextMilestone = m_totalStyleRules * .5;
      break;
    case UsageGt50:
      m_styleRulesNeededForNextMilestone = m_totalStyleRules * .75;
      break;
    case UsageGt75:
      m_styleRulesNeededForNextMilestone = m_totalStyleRules * .9;
      break;
    case UsageGt90:
      m_styleRulesNeededForNextMilestone = m_totalStyleRules - 1;
      break;
    case UsageAll:
      m_styleRulesNeededForNextMilestone = m_totalStyleRules;
      break;
  }

  usageHistogram.count(m_usage);
}

}  // namespace blink
