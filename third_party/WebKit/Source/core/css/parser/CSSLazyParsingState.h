// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLazyParsingState_h
#define CSSLazyParsingState_h

#include "core/css/CSSSelectorList.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserMode.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSLazyPropertyParserImpl;
class CSSParserTokenRange;

// This class helps lazy parsing by retaining necessary state. It should not
// outlive the StyleSheetContents that initiated the parse, as it retains a raw
// reference to the UseCounter associated with the style sheet.
class CSSLazyParsingState
    : public GarbageCollectedFinalized<CSSLazyParsingState> {
 public:
  CSSLazyParsingState(const CSSParserContext*,
                      Vector<String> escapedStrings,
                      const String& sheetText,
                      StyleSheetContents*);

  // Called when all lazy property parsers are initialized. At this point we
  // know the total number of style rules that deferred parsing.
  void finishInitialParsing();

  // Helper method used to bump m_totalStyleRules.
  CSSLazyPropertyParserImpl* createLazyParser(const CSSParserTokenRange& block);

  const CSSParserContext* context();

  void countRuleParsed();

  bool shouldLazilyParseProperties(const CSSSelectorList&,
                                   const CSSParserTokenRange& block) const;

  DECLARE_TRACE();

  // Exposed for tests. This enum is used to back a histogram, so new values
  // must be appended to the end, before UsageLastValue.
  enum CSSRuleUsage {
    UsageGe0 = 0,
    UsageGt10 = 1,
    UsageGt25 = 2,
    UsageGt50 = 3,
    UsageGt75 = 4,
    UsageGt90 = 5,
    UsageAll = 6,

    // This value must be last.
    UsageLastValue = 7
  };

 private:
  void recordUsageMetrics();

  Member<const CSSParserContext> m_context;
  Vector<String> m_escapedStrings;
  // Also referenced on the css resource.
  String m_sheetText;

  // Weak to ensure lazy state will never cause the contents to live longer than
  // it should (we DCHECK this fact).
  WeakMember<StyleSheetContents> m_owningContents;

  // Cache the document as a proxy for caching the UseCounter. Grabbing the
  // UseCounter per every property parse is a bit more expensive.
  WeakMember<Document> m_document;

  // Used for calculating the % of rules that ended up being parsed.
  int m_parsedStyleRules;
  int m_totalStyleRules;

  int m_styleRulesNeededForNextMilestone;

  int m_usage;

  // Whether or not use counting is enabled for parsing. This will usually be
  // true, except for when stylesheets with @imports are removed from the page.
  // See StyleRuleImport::setCSSStyleSheet.
  const bool m_shouldUseCount;
};

}  // namespace blink

#endif  // CSSLazyParsingState_h
