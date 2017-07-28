/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
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
 *
 */

#ifndef StyleResolver_h
#define StyleResolver_h

#include "core/CoreExport.h"
#include "core/animation/Interpolation.h"
#include "core/animation/PropertyHandle.h"
#include "core/css/ElementRuleCollector.h"
#include "core/css/PseudoStyleRequest.h"
#include "core/css/SelectorChecker.h"
#include "core/css/SelectorFilter.h"
#include "core/css/resolver/CSSPropertyPriority.h"
#include "core/css/resolver/MatchedPropertiesCache.h"
#include "core/css/resolver/StyleBuilder.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/ListHashSet.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

class AnimatableValue;
class CSSRuleList;
class CSSValue;
class Document;
class Element;
class Interpolation;
class MatchResult;
class RuleSet;
class StylePropertySet;
class StyleRuleUsageTracker;
class CSSVariableResolver;

enum RuleMatchingBehavior { kMatchAllRules, kMatchAllRulesExcludingSMIL };

// This class selects a ComputedStyle for a given element based on a collection
// of stylesheets.
class CORE_EXPORT StyleResolver final
    : public GarbageCollectedFinalized<StyleResolver> {
  WTF_MAKE_NONCOPYABLE(StyleResolver);

 public:
  static StyleResolver* Create(Document& document) {
    return new StyleResolver(document);
  }
  ~StyleResolver();
  void Dispose();

  RefPtr<ComputedStyle> StyleForElement(
      Element*,
      const ComputedStyle* parent_style = nullptr,
      const ComputedStyle* layout_parent_style = nullptr,
      RuleMatchingBehavior = kMatchAllRules);

  static RefPtr<AnimatableValue> CreateAnimatableValueSnapshot(
      Element&,
      const ComputedStyle& base_style,
      const ComputedStyle* parent_style,
      CSSPropertyID,
      const CSSValue*);

  RefPtr<ComputedStyle> PseudoStyleForElement(
      Element*,
      const PseudoStyleRequest&,
      const ComputedStyle* parent_style,
      const ComputedStyle* layout_parent_style);

  RefPtr<ComputedStyle> StyleForPage(int page_index);
  RefPtr<ComputedStyle> StyleForText(Text*);

  static RefPtr<ComputedStyle> StyleForViewport(Document&);

  // TODO(esprehn): StyleResolver should probably not contain tree walking
  // state, instead we should pass a context object during recalcStyle.
  SelectorFilter& GetSelectorFilter() { return selector_filter_; }

  StyleRuleKeyframes* FindKeyframesRule(const Element*,
                                        const AtomicString& animation_name);

  // These methods will give back the set of rules that matched for a given
  // element (or a pseudo-element).
  enum CSSRuleFilter {
    kUAAndUserCSSRules = 1 << 1,
    kAuthorCSSRules = 1 << 2,
    kEmptyCSSRules = 1 << 3,
    kCrossOriginCSSRules = 1 << 4,
    kAllButEmptyCSSRules =
        kUAAndUserCSSRules | kAuthorCSSRules | kCrossOriginCSSRules,
    kAllCSSRules = kAllButEmptyCSSRules | kEmptyCSSRules,
  };
  CSSRuleList* CssRulesForElement(
      Element*,
      unsigned rules_to_include = kAllButEmptyCSSRules);
  CSSRuleList* PseudoCSSRulesForElement(
      Element*,
      PseudoId,
      unsigned rules_to_include = kAllButEmptyCSSRules);
  StyleRuleList* StyleRulesForElement(Element*, unsigned rules_to_include);

  void ComputeFont(ComputedStyle*, const StylePropertySet&);

  // FIXME: Rename to reflect the purpose, like didChangeFontSize or something.
  void InvalidateMatchedPropertiesCache();

  void SetResizedForViewportUnits();
  void ClearResizedForViewportUnits();

  // Exposed for ComputedStyle::IsStyleAvailable().
  static ComputedStyle* StyleNotYetAvailable() {
    return style_not_yet_available_;
  }

  PseudoElement* CreatePseudoElementIfNeeded(Element& parent, PseudoId);

  void SetRuleUsageTracker(StyleRuleUsageTracker*);
  void UpdateMediaType();

  static void ApplyAnimatedCustomProperty(StyleResolverState&,
                                          CSSVariableResolver&,
                                          const PropertyHandle&);

  DECLARE_TRACE();

 private:
  explicit StyleResolver(Document&);

  static RefPtr<ComputedStyle> InitialStyleForElement(Document&);

  // FIXME: This should probably go away, folded into FontBuilder.
  void UpdateFont(StyleResolverState&);

  void AddMatchedRulesToTracker(const ElementRuleCollector&);

  void LoadPendingResources(StyleResolverState&);
  void AdjustComputedStyle(StyleResolverState&, Element*);

  void CollectPseudoRulesForElement(const Element&,
                                    ElementRuleCollector&,
                                    PseudoId,
                                    unsigned rules_to_include);
  void MatchRuleSet(ElementRuleCollector&, RuleSet*);
  void MatchUARules(ElementRuleCollector&);
  void MatchScopedRules(const Element&, ElementRuleCollector&);
  void MatchAuthorRules(const Element&, ElementRuleCollector&);
  void MatchAuthorRulesV0(const Element&, ElementRuleCollector&);
  void MatchAllRules(StyleResolverState&,
                     ElementRuleCollector&,
                     bool include_smil_properties);
  void CollectTreeBoundaryCrossingRulesV0CascadeOrder(const Element&,
                                                      ElementRuleCollector&);

  struct CacheSuccess {
    STACK_ALLOCATED();
    bool is_inherited_cache_hit;
    bool is_non_inherited_cache_hit;
    unsigned cache_hash;
    Member<const CachedMatchedProperties> cached_matched_properties;

    CacheSuccess(bool is_inherited_cache_hit,
                 bool is_non_inherited_cache_hit,
                 unsigned cache_hash,
                 const CachedMatchedProperties* cached_matched_properties)
        : is_inherited_cache_hit(is_inherited_cache_hit),
          is_non_inherited_cache_hit(is_non_inherited_cache_hit),
          cache_hash(cache_hash),
          cached_matched_properties(cached_matched_properties) {}

    bool IsFullCacheHit() const {
      return is_inherited_cache_hit && is_non_inherited_cache_hit;
    }
    bool ShouldApplyInheritedOnly() const {
      return is_non_inherited_cache_hit && !is_inherited_cache_hit;
    }
    void SetFailed() {
      is_inherited_cache_hit = false;
      is_non_inherited_cache_hit = false;
    }
  };

  // These flags indicate whether an apply pass for a given CSSPropertyPriority
  // and isImportant is required.
  class NeedsApplyPass {
   public:
    bool Get(CSSPropertyPriority priority, bool is_important) const {
      return flags_[GetIndex(priority, is_important)];
    }
    void Set(CSSPropertyPriority priority, bool is_important) {
      flags_[GetIndex(priority, is_important)] = true;
    }

   private:
    static size_t GetIndex(CSSPropertyPriority priority, bool is_important) {
      DCHECK(priority >= 0 && priority < kPropertyPriorityCount);
      return priority * 2 + is_important;
    }
    bool flags_[kPropertyPriorityCount * 2] = {0};
  };

  enum ShouldUpdateNeedsApplyPass {
    kCheckNeedsApplyPass = false,
    kUpdateNeedsApplyPass = true,
  };

  void ApplyMatchedPropertiesAndCustomPropertyAnimations(
      StyleResolverState&,
      const MatchResult&,
      const Element* animating_element);
  CacheSuccess ApplyMatchedCache(StyleResolverState&, const MatchResult&);
  enum ApplyAnimations { kExcludeAnimations, kIncludeAnimations };
  void ApplyCustomProperties(StyleResolverState&,
                             const MatchResult&,
                             ApplyAnimations,
                             const CacheSuccess&,
                             NeedsApplyPass&);
  void ApplyMatchedAnimationProperties(StyleResolverState&,
                                       const MatchResult&,
                                       const CacheSuccess&,
                                       NeedsApplyPass&);
  void ApplyMatchedStandardProperties(StyleResolverState&,
                                      const MatchResult&,
                                      const CacheSuccess&,
                                      NeedsApplyPass&);
  void CalculateAnimationUpdate(StyleResolverState&,
                                const Element* animating_element);
  bool ApplyAnimatedStandardProperties(StyleResolverState&, const Element*);

  void ApplyCallbackSelectors(StyleResolverState&);

  template <CSSPropertyPriority priority, ShouldUpdateNeedsApplyPass>
  void ApplyMatchedProperties(StyleResolverState&,
                              const MatchedPropertiesRange&,
                              bool important,
                              bool inherited_only,
                              NeedsApplyPass&);
  template <CSSPropertyPriority priority, ShouldUpdateNeedsApplyPass>
  void ApplyProperties(StyleResolverState&,
                       const StylePropertySet* properties,
                       bool is_important,
                       bool inherited_only,
                       NeedsApplyPass&,
                       PropertyWhitelistType = kPropertyWhitelistNone);
  template <CSSPropertyPriority priority>
  void ApplyAnimatedStandardProperties(StyleResolverState&,
                                       const ActiveInterpolationsMap&);
  template <CSSPropertyPriority priority>
  void ApplyAllProperty(StyleResolverState&,
                        const CSSValue&,
                        bool inherited_only,
                        PropertyWhitelistType);
  template <CSSPropertyPriority priority, ShouldUpdateNeedsApplyPass>
  void ApplyPropertiesForApplyAtRule(StyleResolverState&,
                                     const CSSValue&,
                                     bool is_important,
                                     NeedsApplyPass&,
                                     PropertyWhitelistType);

  bool PseudoStyleForElementInternal(Element&,
                                     const PseudoStyleRequest&,
                                     const ComputedStyle* parent_style,
                                     StyleResolverState&);
  bool HasAuthorBackground(const StyleResolverState&);
  bool HasAuthorBorder(const StyleResolverState&);

  PseudoElement* CreatePseudoElement(Element* parent, PseudoId);

  Document& GetDocument() const { return *document_; }

  bool WasViewportResized() const { return was_viewport_resized_; }

  static ComputedStyle* style_not_yet_available_;

  MatchedPropertiesCache matched_properties_cache_;
  Member<Document> document_;
  SelectorFilter selector_filter_;

  Member<StyleRuleUsageTracker> tracker_;

  bool print_media_type_ = false;
  bool was_viewport_resized_ = false;
};

}  // namespace blink

#endif  // StyleResolver_h
