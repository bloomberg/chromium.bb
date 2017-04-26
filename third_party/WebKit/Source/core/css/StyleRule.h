/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008, 2012, 2013 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef StyleRule_h
#define StyleRule_h

#include "core/CoreExport.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/MediaList.h"
#include "core/css/StylePropertySet.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class CSSRule;
class CSSStyleSheet;

class CORE_EXPORT StyleRuleBase
    : public GarbageCollectedFinalized<StyleRuleBase> {
 public:
  enum RuleType {
    kCharset,
    kStyle,
    kImport,
    kMedia,
    kFontFace,
    kPage,
    kKeyframes,
    kKeyframe,
    kNamespace,
    kSupports,
    kViewport,
  };

  RuleType GetType() const { return static_cast<RuleType>(type_); }

  bool IsCharsetRule() const { return GetType() == kCharset; }
  bool IsFontFaceRule() const { return GetType() == kFontFace; }
  bool IsKeyframesRule() const { return GetType() == kKeyframes; }
  bool IsKeyframeRule() const { return GetType() == kKeyframe; }
  bool IsNamespaceRule() const { return GetType() == kNamespace; }
  bool IsMediaRule() const { return GetType() == kMedia; }
  bool IsPageRule() const { return GetType() == kPage; }
  bool IsStyleRule() const { return GetType() == kStyle; }
  bool IsSupportsRule() const { return GetType() == kSupports; }
  bool IsViewportRule() const { return GetType() == kViewport; }
  bool IsImportRule() const { return GetType() == kImport; }

  StyleRuleBase* Copy() const;

  // FIXME: There shouldn't be any need for the null parent version.
  CSSRule* CreateCSSOMWrapper(CSSStyleSheet* parent_sheet = 0) const;
  CSSRule* CreateCSSOMWrapper(CSSRule* parent_rule) const;

  DECLARE_TRACE();
  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {}
  void FinalizeGarbageCollectedObject();

  // ~StyleRuleBase should be public, because non-public ~StyleRuleBase
  // causes C2248 error : 'blink::StyleRuleBase::~StyleRuleBase' : cannot
  // access protected member declared in class 'blink::StyleRuleBase' when
  // compiling 'source\wtf\refcounted.h' by using msvc.
  ~StyleRuleBase() {}

 protected:
  StyleRuleBase(RuleType type) : type_(type) {}
  StyleRuleBase(const StyleRuleBase& rule) : type_(rule.type_) {}

 private:
  CSSRule* CreateCSSOMWrapper(CSSStyleSheet* parent_sheet,
                              CSSRule* parent_rule) const;

  unsigned type_ : 5;
};

class CORE_EXPORT StyleRule : public StyleRuleBase {
 public:
  // Adopts the selector list
  static StyleRule* Create(CSSSelectorList selector_list,
                           StylePropertySet* properties) {
    return new StyleRule(std::move(selector_list), properties);
  }
  static StyleRule* CreateLazy(CSSSelectorList selector_list,
                               CSSLazyPropertyParser* lazy_property_parser) {
    return new StyleRule(std::move(selector_list), lazy_property_parser);
  }

  ~StyleRule();

  const CSSSelectorList& SelectorList() const { return selector_list_; }
  const StylePropertySet& Properties() const;
  MutableStylePropertySet& MutableProperties();

  void WrapperAdoptSelectorList(CSSSelectorList selectors) {
    selector_list_ = std::move(selectors);
  }

  StyleRule* Copy() const { return new StyleRule(*this); }

  static unsigned AverageSizeInBytes();

  // Helper methods to avoid parsing lazy properties when not needed.
  bool PropertiesHaveFailedOrCanceledSubresources() const;
  bool ShouldConsiderForMatchingRules(bool include_empty_rules) const;

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  friend class CSSLazyParsingTest;
  bool HasParsedProperties() const;

  StyleRule(CSSSelectorList, StylePropertySet*);
  StyleRule(CSSSelectorList, CSSLazyPropertyParser*);
  StyleRule(const StyleRule&);

  // Whether or not we should consider this for matching rules. Usually we try
  // to avoid considering empty property sets, as an optimization. This is
  // not possible for lazy properties, which always need to be considered. The
  // lazy parser does its best to avoid lazy parsing for properties that look
  // empty due to lack of tokens.
  enum ConsiderForMatching {
    kAlwaysConsider,
    kConsiderIfNonEmpty,
  };
  mutable ConsiderForMatching should_consider_for_matching_rules_;

  CSSSelectorList selector_list_;
  mutable Member<StylePropertySet> properties_;
  mutable Member<CSSLazyPropertyParser> lazy_property_parser_;
};

class StyleRuleFontFace : public StyleRuleBase {
 public:
  static StyleRuleFontFace* Create(StylePropertySet* properties) {
    return new StyleRuleFontFace(properties);
  }

  ~StyleRuleFontFace();

  const StylePropertySet& Properties() const { return *properties_; }
  MutableStylePropertySet& MutableProperties();

  StyleRuleFontFace* Copy() const { return new StyleRuleFontFace(*this); }

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  StyleRuleFontFace(StylePropertySet*);
  StyleRuleFontFace(const StyleRuleFontFace&);

  Member<StylePropertySet> properties_;  // Cannot be null.
};

class StyleRulePage : public StyleRuleBase {
 public:
  // Adopts the selector list
  static StyleRulePage* Create(CSSSelectorList selector_list,
                               StylePropertySet* properties) {
    return new StyleRulePage(std::move(selector_list), properties);
  }

  ~StyleRulePage();

  const CSSSelector* Selector() const { return selector_list_.First(); }
  const StylePropertySet& Properties() const { return *properties_; }
  MutableStylePropertySet& MutableProperties();

  void WrapperAdoptSelectorList(CSSSelectorList selectors) {
    selector_list_ = std::move(selectors);
  }

  StyleRulePage* Copy() const { return new StyleRulePage(*this); }

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  StyleRulePage(CSSSelectorList, StylePropertySet*);
  StyleRulePage(const StyleRulePage&);

  Member<StylePropertySet> properties_;  // Cannot be null.
  CSSSelectorList selector_list_;
};

class CORE_EXPORT StyleRuleGroup : public StyleRuleBase {
 public:
  const HeapVector<Member<StyleRuleBase>>& ChildRules() const {
    return child_rules_;
  }

  void WrapperInsertRule(unsigned, StyleRuleBase*);
  void WrapperRemoveRule(unsigned);

  DECLARE_TRACE_AFTER_DISPATCH();

 protected:
  StyleRuleGroup(RuleType, HeapVector<Member<StyleRuleBase>>& adopt_rule);
  StyleRuleGroup(const StyleRuleGroup&);

 private:
  HeapVector<Member<StyleRuleBase>> child_rules_;
};

class CORE_EXPORT StyleRuleCondition : public StyleRuleGroup {
 public:
  String ConditionText() const { return condition_text_; }

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    StyleRuleGroup::TraceAfterDispatch(visitor);
  }

 protected:
  StyleRuleCondition(RuleType, HeapVector<Member<StyleRuleBase>>& adopt_rule);
  StyleRuleCondition(RuleType,
                     const String& condition_text,
                     HeapVector<Member<StyleRuleBase>>& adopt_rule);
  StyleRuleCondition(const StyleRuleCondition&);
  String condition_text_;
};

class CORE_EXPORT StyleRuleMedia : public StyleRuleCondition {
 public:
  static StyleRuleMedia* Create(
      RefPtr<MediaQuerySet> media,
      HeapVector<Member<StyleRuleBase>>& adopt_rules) {
    return new StyleRuleMedia(media, adopt_rules);
  }

  MediaQuerySet* MediaQueries() const { return media_queries_.Get(); }

  StyleRuleMedia* Copy() const { return new StyleRuleMedia(*this); }

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  StyleRuleMedia(RefPtr<MediaQuerySet>,
                 HeapVector<Member<StyleRuleBase>>& adopt_rules);
  StyleRuleMedia(const StyleRuleMedia&);

  RefPtr<MediaQuerySet> media_queries_;
};

class StyleRuleSupports : public StyleRuleCondition {
 public:
  static StyleRuleSupports* Create(
      const String& condition_text,
      bool condition_is_supported,
      HeapVector<Member<StyleRuleBase>>& adopt_rules) {
    return new StyleRuleSupports(condition_text, condition_is_supported,
                                 adopt_rules);
  }

  bool ConditionIsSupported() const { return condition_is_supported_; }
  StyleRuleSupports* Copy() const { return new StyleRuleSupports(*this); }

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    StyleRuleCondition::TraceAfterDispatch(visitor);
  }

 private:
  StyleRuleSupports(const String& condition_text,
                    bool condition_is_supported,
                    HeapVector<Member<StyleRuleBase>>& adopt_rules);
  StyleRuleSupports(const StyleRuleSupports&);

  String condition_text_;
  bool condition_is_supported_;
};

class StyleRuleViewport : public StyleRuleBase {
 public:
  static StyleRuleViewport* Create(StylePropertySet* properties) {
    return new StyleRuleViewport(properties);
  }

  ~StyleRuleViewport();

  const StylePropertySet& Properties() const { return *properties_; }
  MutableStylePropertySet& MutableProperties();

  StyleRuleViewport* Copy() const { return new StyleRuleViewport(*this); }

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  StyleRuleViewport(StylePropertySet*);
  StyleRuleViewport(const StyleRuleViewport&);

  Member<StylePropertySet> properties_;  // Cannot be null
};

// This should only be used within the CSS Parser
class StyleRuleCharset : public StyleRuleBase {
 public:
  static StyleRuleCharset* Create() { return new StyleRuleCharset(); }
  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    StyleRuleBase::TraceAfterDispatch(visitor);
  }

 private:
  StyleRuleCharset() : StyleRuleBase(kCharset) {}
};

#define DEFINE_STYLE_RULE_TYPE_CASTS(Type)                \
  DEFINE_TYPE_CASTS(StyleRule##Type, StyleRuleBase, rule, \
                    rule->Is##Type##Rule(), rule.Is##Type##Rule())

DEFINE_TYPE_CASTS(StyleRule,
                  StyleRuleBase,
                  rule,
                  rule->IsStyleRule(),
                  rule.IsStyleRule());
DEFINE_STYLE_RULE_TYPE_CASTS(FontFace);
DEFINE_STYLE_RULE_TYPE_CASTS(Page);
DEFINE_STYLE_RULE_TYPE_CASTS(Media);
DEFINE_STYLE_RULE_TYPE_CASTS(Supports);
DEFINE_STYLE_RULE_TYPE_CASTS(Viewport);
DEFINE_STYLE_RULE_TYPE_CASTS(Charset);

}  // namespace blink

#endif  // StyleRule_h
