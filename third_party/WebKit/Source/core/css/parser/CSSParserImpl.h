// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserImpl_h
#define CSSParserImpl_h

#include <memory>
#include "core/CSSPropertyNames.h"
#include "core/css/CSSProperty.h"
#include "core/css/CSSPropertySourceData.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CSSLazyParsingState;
class CSSParserContext;
class CSSParserObserver;
class CSSParserObserverWrapper;
class CSSParserScopedTokenBuffer;
class CSSParserTokenStream;
class StyleRule;
class StyleRuleBase;
class StyleRuleCharset;
class StyleRuleFontFace;
class StyleRuleImport;
class StyleRuleKeyframe;
class StyleRuleKeyframes;
class StyleRuleMedia;
class StyleRuleNamespace;
class StyleRulePage;
class StyleRuleSupports;
class StyleRuleViewport;
class StyleSheetContents;
class Element;

class CSSParserImpl {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(CSSParserImpl);

 public:
  CSSParserImpl(const CSSParserContext*, StyleSheetContents* = nullptr);

  enum AllowedRulesType {
    // As per css-syntax, css-cascade and css-namespaces, @charset rules
    // must come first, followed by @import then @namespace.
    // AllowImportRules actually means we allow @import and any rules thay
    // may follow it, i.e. @namespace rules and regular rules.
    // AllowCharsetRules and AllowNamespaceRules behave similarly.
    kAllowCharsetRules,
    kAllowImportRules,
    kAllowNamespaceRules,
    kRegularRules,
    kKeyframeRules,
    kApplyRules,  // For @apply inside style rules
    kNoRules,     // For parsing at-rules inside declaration lists
  };

  // Represents the start and end offsets of a CSSParserTokenRange.
  struct RangeOffset {
    size_t start, end;

    RangeOffset(size_t start, size_t end) : start(start), end(end) {
      DCHECK(start <= end);
    }

    // Used when we don't care what the offset is (typically when we don't have
    // an observer).
    static RangeOffset Ignore() { return {0, 0}; }
  };

  static MutableStylePropertySet::SetResult ParseValue(MutableStylePropertySet*,
                                                       CSSPropertyID,
                                                       const String&,
                                                       bool important,
                                                       const CSSParserContext*);
  static MutableStylePropertySet::SetResult ParseVariableValue(
      MutableStylePropertySet*,
      const AtomicString& property_name,
      const PropertyRegistry*,
      const String&,
      bool important,
      const CSSParserContext*,
      bool is_animation_tainted);
  static ImmutableStylePropertySet* ParseInlineStyleDeclaration(const String&,
                                                                Element*);
  static bool ParseDeclarationList(MutableStylePropertySet*,
                                   const String&,
                                   const CSSParserContext*);
  static StyleRuleBase* ParseRule(const String&,
                                  const CSSParserContext*,
                                  StyleSheetContents*,
                                  AllowedRulesType);
  static void ParseStyleSheet(const String&,
                              const CSSParserContext*,
                              StyleSheetContents*,
                              bool defer_property_parsing = false);
  static CSSSelectorList ParsePageSelector(CSSParserTokenRange,
                                           StyleSheetContents*);

  static ImmutableStylePropertySet* ParseCustomPropertySet(CSSParserTokenRange);
  // TODO(shend): Remove this when crbug.com/661854 is fixed. We need to use a
  // stream for parsing @apply blocks so we can correctly store custom
  // property values.
  void ConsumeDeclarationListForAtApply(CSSParserTokenRange);

  static std::unique_ptr<Vector<double>> ParseKeyframeKeyList(const String&);

  bool SupportsDeclaration(CSSParserTokenRange&);

  static void ParseDeclarationListForInspector(const String&,
                                               const CSSParserContext*,
                                               CSSParserObserver&);
  static void ParseStyleSheetForInspector(const String&,
                                          const CSSParserContext*,
                                          StyleSheetContents*,
                                          CSSParserObserver&);

  static StylePropertySet* ParseDeclarationListForLazyStyle(
      const String&,
      size_t offset,
      const CSSParserContext*);

 private:
  enum RuleListType { kTopLevelRuleList, kRegularRuleList, kKeyframesRuleList };

  // Returns whether the first encountered rule was valid
  template <typename T>
  bool ConsumeRuleList(CSSParserTokenStream&, RuleListType, T callback);

  // These functions update the range/stream they're given
  StyleRuleBase* ConsumeAtRule(CSSParserTokenStream&, AllowedRulesType);
  StyleRuleBase* ConsumeQualifiedRule(CSSParserTokenStream&, AllowedRulesType);

  static StyleRuleCharset* ConsumeCharsetRule(CSSParserTokenRange prelude);
  StyleRuleImport* ConsumeImportRule(AtomicString prelude_uri,
                                     CSSParserTokenRange prelude,
                                     const RangeOffset& prelude_offset);
  StyleRuleNamespace* ConsumeNamespaceRule(CSSParserTokenRange prelude);
  StyleRuleMedia* ConsumeMediaRule(CSSParserScopedTokenBuffer prelude_buffer,
                                   const RangeOffset& prelude_offset,
                                   CSSParserTokenStream& block);
  StyleRuleSupports* ConsumeSupportsRule(
      CSSParserScopedTokenBuffer prelude_buffer,
      const RangeOffset& prelude_offset,
      CSSParserTokenStream& block);
  StyleRuleViewport* ConsumeViewportRule(
      CSSParserScopedTokenBuffer prelude_buffer,
      const RangeOffset& prelude_offset,
      CSSParserTokenStream& block);
  StyleRuleFontFace* ConsumeFontFaceRule(
      CSSParserScopedTokenBuffer prelude_buffer,
      const RangeOffset& prelude_offset,
      CSSParserTokenStream& block);
  StyleRuleKeyframes* ConsumeKeyframesRule(
      bool webkit_prefixed,
      CSSParserScopedTokenBuffer prelude_buffer,
      const RangeOffset& prelude_offset,
      CSSParserTokenStream& block);
  StyleRulePage* ConsumePageRule(CSSParserScopedTokenBuffer prelude_buffer,
                                 const RangeOffset& prelude_offset,
                                 CSSParserTokenStream& block);
  // Updates parsed_properties_
  void ConsumeApplyRule(CSSParserTokenRange prelude);

  StyleRuleKeyframe* ConsumeKeyframeStyleRule(
      CSSParserScopedTokenBuffer prelude_buffer,
      const RangeOffset& prelude_offset,
      CSSParserTokenStream& block);
  StyleRule* ConsumeStyleRule(CSSParserTokenStream&);

  void ConsumeDeclarationList(CSSParserTokenStream&, StyleRule::RuleType);
  void ConsumeDeclaration(CSSParserTokenRange,
                          const RangeOffset& decl_offset,
                          StyleRule::RuleType);
  void ConsumeDeclarationValue(CSSParserTokenRange,
                               CSSPropertyID,
                               bool important,
                               StyleRule::RuleType);
  void ConsumeVariableValue(CSSParserTokenRange,
                            const AtomicString& property_name,
                            bool important,
                            bool is_animation_tainted);

  static std::unique_ptr<Vector<double>> ConsumeKeyframeKeyList(
      CSSParserTokenRange);

  // FIXME: Can we build StylePropertySets directly?
  // FIXME: Investigate using a smaller inline buffer
  HeapVector<CSSProperty, 256> parsed_properties_;

  Member<const CSSParserContext> context_;
  Member<StyleSheetContents> style_sheet_;

  // For the inspector
  CSSParserObserverWrapper* observer_wrapper_;

  Member<CSSLazyParsingState> lazy_state_;
};

}  // namespace blink

#endif  // CSSParserImpl_h
