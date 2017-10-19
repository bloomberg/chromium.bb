/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef ScopedStyleResolver_h
#define ScopedStyleResolver_h

#include "core/css/ActiveStyleSheets.h"
#include "core/css/ElementRuleCollector.h"
#include "core/css/RuleSet.h"
#include "core/dom/TreeScope.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"

namespace blink {

class PageRuleCollector;
class StyleSheetContents;

// This class selects a ComputedStyle for a given element based on a collection
// of stylesheets.
class ScopedStyleResolver final
    : public GarbageCollectedFinalized<ScopedStyleResolver> {
  WTF_MAKE_NONCOPYABLE(ScopedStyleResolver);

 public:
  static ScopedStyleResolver* Create(TreeScope& scope) {
    return new ScopedStyleResolver(scope);
  }

  const TreeScope& GetTreeScope() const { return *scope_; }
  ScopedStyleResolver* Parent() const;

  StyleRuleKeyframes* KeyframeStylesForAnimation(
      const StringImpl* animation_name);

  void AppendActiveStyleSheets(unsigned index, const ActiveStyleSheetVector&);
  void CollectMatchingAuthorRules(ElementRuleCollector&,
                                  CascadeOrder = kIgnoreCascadeOrder);
  void CollectMatchingShadowHostRules(ElementRuleCollector&,
                                      CascadeOrder = kIgnoreCascadeOrder);
  void CollectMatchingSlottedRules(ElementRuleCollector&,
                                   CascadeOrder = kIgnoreCascadeOrder);
  void CollectMatchingTreeBoundaryCrossingRules(
      ElementRuleCollector&,
      CascadeOrder = kIgnoreCascadeOrder);
  void MatchPageRules(PageRuleCollector&);
  void CollectFeaturesTo(RuleFeatureSet&,
                         HeapHashSet<Member<const StyleSheetContents>>&
                             visited_shared_style_sheet_contents) const;
  void ResetAuthorStyle();
  bool HasDeepOrShadowSelector() const { return has_deep_or_shadow_selector_; }
  void SetHasUnresolvedKeyframesRule() {
    has_unresolved_keyframes_rule_ = true;
  }
  bool NeedsAppendAllSheets() const { return needs_append_all_sheets_; }
  void SetNeedsAppendAllSheets() { needs_append_all_sheets_ = true; }
  static void KeyframesRulesAdded(const TreeScope&);
  static ContainerNode& InvalidationRootForTreeScope(const TreeScope&);
  CORE_EXPORT static bool HaveSameStyles(const ScopedStyleResolver*,
                                         const ScopedStyleResolver*);

  void Trace(blink::Visitor*);

 private:
  explicit ScopedStyleResolver(TreeScope& scope) : scope_(scope) {}

  void AddTreeBoundaryCrossingRules(const RuleSet&,
                                    CSSStyleSheet*,
                                    unsigned sheet_index);
  void AddSlottedRules(const RuleSet&, CSSStyleSheet*, unsigned sheet_index);
  void AddKeyframeRules(const RuleSet&);
  void AddFontFaceRules(const RuleSet&);
  void AddKeyframeStyle(StyleRuleKeyframes*);

  Member<TreeScope> scope_;

  HeapVector<Member<CSSStyleSheet>> author_style_sheets_;
  MediaQueryResultList viewport_dependent_media_query_results_;
  MediaQueryResultList device_dependent_media_query_results_;

  using KeyframesRuleMap =
      HeapHashMap<const StringImpl*, Member<StyleRuleKeyframes>>;
  KeyframesRuleMap keyframes_rule_map_;

  class RuleSubSet final : public GarbageCollected<RuleSubSet> {
   public:
    static RuleSubSet* Create(CSSStyleSheet* sheet,
                              unsigned index,
                              RuleSet* rules) {
      return new RuleSubSet(sheet, index, rules);
    }

    Member<CSSStyleSheet> parent_style_sheet_;
    unsigned parent_index_;
    Member<RuleSet> rule_set_;

    void Trace(blink::Visitor*);

   private:
    RuleSubSet(CSSStyleSheet* sheet, unsigned index, RuleSet* rules)
        : parent_style_sheet_(sheet), parent_index_(index), rule_set_(rules) {}
  };
  using CSSStyleSheetRuleSubSet = HeapVector<Member<RuleSubSet>>;

  Member<CSSStyleSheetRuleSubSet> tree_boundary_crossing_rule_set_;
  Member<CSSStyleSheetRuleSubSet> slotted_rule_set_;

  bool has_deep_or_shadow_selector_ = false;
  bool has_unresolved_keyframes_rule_ = false;
  bool needs_append_all_sheets_ = false;
};

}  // namespace blink

#endif  // ScopedStyleResolver_h
