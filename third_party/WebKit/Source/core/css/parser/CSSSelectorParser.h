// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSelectorParser_h
#define CSSSelectorParser_h

#include "core/CoreExport.h"
#include "core/css/parser/CSSParserSelector.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include <memory>

namespace blink {

class CSSParserContext;
class CSSParserTokenStream;
class CSSParserObserverWrapper;
class CSSSelectorList;
class StyleSheetContents;

// FIXME: We should consider building CSSSelectors directly instead of using
// the intermediate CSSParserSelector.
class CORE_EXPORT CSSSelectorParser {
  STACK_ALLOCATED();

 public:
  static CSSSelectorList ParseSelector(CSSParserTokenRange,
                                       const CSSParserContext*,
                                       StyleSheetContents*);
  static CSSSelectorList ConsumeSelector(CSSParserTokenStream&,
                                         const CSSParserContext*,
                                         StyleSheetContents*,
                                         CSSParserObserverWrapper*);

  static bool ConsumeANPlusB(CSSParserTokenRange&, std::pair<int, int>&);

 private:
  CSSSelectorParser(const CSSParserContext*, StyleSheetContents*);

  // These will all consume trailing comments if successful

  CSSSelectorList ConsumeComplexSelectorList(CSSParserTokenRange&);
  CSSSelectorList ConsumeComplexSelectorList(CSSParserTokenStream&,
                                             CSSParserObserverWrapper*);
  CSSSelectorList ConsumeCompoundSelectorList(CSSParserTokenRange&);

  std::unique_ptr<CSSParserSelector> ConsumeComplexSelector(
      CSSParserTokenRange&);
  std::unique_ptr<CSSParserSelector> ConsumeCompoundSelector(
      CSSParserTokenRange&);
  // This doesn't include element names, since they're handled specially
  std::unique_ptr<CSSParserSelector> ConsumeSimpleSelector(
      CSSParserTokenRange&);

  bool ConsumeName(CSSParserTokenRange&,
                   AtomicString& name,
                   AtomicString& namespace_prefix);

  // These will return nullptr when the selector is invalid
  std::unique_ptr<CSSParserSelector> ConsumeId(CSSParserTokenRange&);
  std::unique_ptr<CSSParserSelector> ConsumeClass(CSSParserTokenRange&);
  std::unique_ptr<CSSParserSelector> ConsumePseudo(CSSParserTokenRange&);
  std::unique_ptr<CSSParserSelector> ConsumeAttribute(CSSParserTokenRange&);

  CSSSelector::RelationType ConsumeCombinator(CSSParserTokenRange&);
  CSSSelector::MatchType ConsumeAttributeMatch(CSSParserTokenRange&);
  CSSSelector::AttributeMatchType ConsumeAttributeFlags(CSSParserTokenRange&);

  const AtomicString& DefaultNamespace() const;
  const AtomicString& DetermineNamespace(const AtomicString& prefix);
  void PrependTypeSelectorIfNeeded(const AtomicString& namespace_prefix,
                                   const AtomicString& element_name,
                                   CSSParserSelector*);
  static std::unique_ptr<CSSParserSelector> AddSimpleSelectorToCompound(
      std::unique_ptr<CSSParserSelector> compound_selector,
      std::unique_ptr<CSSParserSelector> simple_selector);
  static std::unique_ptr<CSSParserSelector>
  SplitCompoundAtImplicitShadowCrossingCombinator(
      std::unique_ptr<CSSParserSelector> compound_selector);
  void RecordUsageAndDeprecations(const CSSSelectorList&);

  Member<const CSSParserContext> context_;
  Member<StyleSheetContents> style_sheet_;  // FIXME: Should be const

  bool failed_parsing_ = false;
  bool disallow_pseudo_elements_ = false;

  class DisallowPseudoElementsScope {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(DisallowPseudoElementsScope);

   public:
    DisallowPseudoElementsScope(CSSSelectorParser* parser)
        : parser_(parser), was_disallowed_(parser_->disallow_pseudo_elements_) {
      parser_->disallow_pseudo_elements_ = true;
    }

    ~DisallowPseudoElementsScope() {
      parser_->disallow_pseudo_elements_ = was_disallowed_;
    }

   private:
    CSSSelectorParser* parser_;
    bool was_disallowed_;
  };
};

}  // namespace blink

#endif  // CSSSelectorParser_h
