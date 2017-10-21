// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLazyParsingState_h
#define CSSLazyParsingState_h

#include "core/css/CSSSelectorList.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserMode.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CSSLazyPropertyParserImpl;

// This class helps lazy parsing by retaining necessary state. It should not
// outlive the StyleSheetContents that initiated the parse, as it retains a raw
// reference to the UseCounter associated with the style sheet.
class CSSLazyParsingState
    : public GarbageCollectedFinalized<CSSLazyParsingState> {
 public:
  CSSLazyParsingState(const CSSParserContext*,
                      const String& sheet_text,
                      StyleSheetContents*);

  // Called when all lazy property parsers are initialized. At this point we
  // know the total number of style rules that deferred parsing.
  void FinishInitialParsing();

  // Helper method used to bump total_style_rules_.
  CSSLazyPropertyParserImpl* CreateLazyParser(size_t offset);

  const CSSParserContext* Context();
  const String& SheetText() const { return sheet_text_; }
  void CountRuleParsed();
  bool ShouldLazilyParseProperties(const CSSSelectorList&) const;

  void Trace(blink::Visitor*);

  // Exposed for tests. This enum is used to back a histogram, so new values
  // must be appended to the end, before UsageLastValue.
  enum CSSRuleUsage {
    kUsageGe0 = 0,
    kUsageGt10 = 1,
    kUsageGt25 = 2,
    kUsageGt50 = 3,
    kUsageGt75 = 4,
    kUsageGt90 = 5,
    kUsageAll = 6,

    // This value must be last.
    kUsageLastValue = 7
  };

 private:
  void RecordUsageMetrics();

  Member<const CSSParserContext> context_;
  // Also referenced on the css resource.
  String sheet_text_;

  // Weak to ensure lazy state will never cause the contents to live longer than
  // it should (we DCHECK this fact).
  WeakMember<StyleSheetContents> owning_contents_;

  // Cache the document as a proxy for caching the UseCounter. Grabbing the
  // UseCounter per every property parse is a bit more expensive.
  WeakMember<Document> document_;

  // Used for calculating the % of rules that ended up being parsed.
  int parsed_style_rules_;
  int total_style_rules_;

  int style_rules_needed_for_next_milestone_;

  int usage_;

  // Whether or not use counting is enabled for parsing. This will usually be
  // true, except for when stylesheets with @imports are removed from the page.
  // See StyleRuleImport::setCSSStyleSheet.
  const bool should_use_count_;
};

}  // namespace blink

#endif  // CSSLazyParsingState_h
