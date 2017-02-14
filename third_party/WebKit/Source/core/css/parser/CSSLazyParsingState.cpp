// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSLazyParsingState.h"
#include "core/css/parser/CSSLazyPropertyParserImpl.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/dom/Document.h"
#include "core/frame/UseCounter.h"
#include "platform/Histogram.h"

namespace blink {

CSSLazyParsingState::CSSLazyParsingState(const CSSParserContext* context,
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
      m_usage(UsageGe0),
      m_shouldUseCount(m_context->isUseCounterRecordingEnabled()) {}

void CSSLazyParsingState::finishInitialParsing() {
  recordUsageMetrics();
}

CSSLazyPropertyParserImpl* CSSLazyParsingState::createLazyParser(
    const CSSParserTokenRange& block) {
  ++m_totalStyleRules;
  return new CSSLazyPropertyParserImpl(std::move(block), this);
}

const CSSParserContext* CSSLazyParsingState::context() {
  DCHECK(m_owningContents);
  if (!m_shouldUseCount) {
    DCHECK(!m_context->isUseCounterRecordingEnabled());
    return m_context;
  }

  // Try as best as possible to grab a valid Document if the old Document has
  // gone away so we can still use UseCounter.
  if (!m_document)
    m_document = m_owningContents->anyOwnerDocument();

  if (!m_context->isDocumentHandleEqual(m_document))
    m_context = CSSParserContext::create(m_context, m_document);
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
  DEFINE_STATIC_LOCAL(CustomCountHistogram, totalRulesHistogram,
                      ("Style.TotalLazyRules", 0, 100000, 50));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, totalRulesFullUsageHistogram,
                      ("Style.TotalLazyRules.FullUsage", 0, 100000, 50));
  switch (m_usage) {
    case UsageGe0:
      totalRulesHistogram.count(m_totalStyleRules);
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
      totalRulesFullUsageHistogram.count(m_totalStyleRules);
      m_styleRulesNeededForNextMilestone = m_totalStyleRules;
      break;
  }

  usageHistogram.count(m_usage);
}

DEFINE_TRACE(CSSLazyParsingState) {
  visitor->trace(m_owningContents);
  visitor->trace(m_document);
  visitor->trace(m_context);
}

}  // namespace blink
