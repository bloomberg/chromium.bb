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

#ifndef RuleSet_h
#define RuleSet_h

#include "core/CoreExport.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/RuleFeature.h"
#include "core/css/StyleRule.h"
#include "core/css/resolver/MediaQueryResult.h"
#include "platform/heap/HeapLinkedStack.h"
#include "platform/heap/HeapTerminatedArray.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/TerminatedArray.h"

namespace blink {

enum AddRuleFlags {
  kRuleHasNoSpecialState = 0,
  kRuleHasDocumentSecurityOrigin = 1,
};

enum PropertyWhitelistType {
  kPropertyWhitelistNone,
  kPropertyWhitelistCue,
  kPropertyWhitelistFirstLetter,
};

class CSSSelector;
class MediaQueryEvaluator;
class StyleSheetContents;

class MinimalRuleData {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  MinimalRuleData(StyleRule* rule, unsigned selector_index, AddRuleFlags flags)
      : rule_(rule), selector_index_(selector_index), flags_(flags) {}

  DECLARE_TRACE();

  Member<StyleRule> rule_;
  unsigned selector_index_;
  AddRuleFlags flags_;
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::MinimalRuleData);

namespace blink {

class CORE_EXPORT RuleData {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  RuleData(StyleRule*,
           unsigned selector_index,
           unsigned position,
           AddRuleFlags);

  unsigned GetPosition() const { return position_; }
  StyleRule* Rule() const { return rule_; }
  const CSSSelector& Selector() const {
    return rule_->SelectorList().SelectorAt(selector_index_);
  }
  unsigned SelectorIndex() const { return selector_index_; }

  bool IsLastInArray() const { return is_last_in_array_; }
  void SetLastInArray(bool flag) { is_last_in_array_ = flag; }

  bool ContainsUncommonAttributeSelector() const {
    return contains_uncommon_attribute_selector_;
  }
  unsigned Specificity() const { return specificity_; }
  unsigned LinkMatchType() const { return link_match_type_; }
  bool HasDocumentSecurityOrigin() const {
    return has_document_security_origin_;
  }
  PropertyWhitelistType PropertyWhitelist(
      bool is_matching_ua_rules = false) const {
    return is_matching_ua_rules
               ? kPropertyWhitelistNone
               : static_cast<PropertyWhitelistType>(property_whitelist_);
  }
  // Try to balance between memory usage (there can be lots of RuleData objects)
  // and good filtering performance.
  static const unsigned kMaximumIdentifierCount = 4;
  const unsigned* DescendantSelectorIdentifierHashes() const {
    return descendant_selector_identifier_hashes_;
  }

  DECLARE_TRACE();

 private:
  Member<StyleRule> rule_;
  // This number is picked fairly arbitrary. If lowered, be aware that there
  // might be sites and extensions using style rules with selector lists
  // exceeding the number of simple selectors to fit in this bitfield.
  // See https://crbug.com/312913 and https://crbug.com/704562
  unsigned selector_index_ : 13;
  // We store an array of RuleData objects in a primitive array.
  unsigned is_last_in_array_ : 1;
  // This number was picked fairly arbitrarily. We can probably lower it if we
  // need to. Some simple testing showed <100,000 RuleData's on large sites.
  unsigned position_ : 18;
  // 32 bits above
  unsigned specificity_ : 24;
  unsigned contains_uncommon_attribute_selector_ : 1;
  unsigned link_match_type_ : 2;  //  CSSSelector::LinkMatchMask
  unsigned has_document_security_origin_ : 1;
  unsigned property_whitelist_ : 2;
  // 30 bits above
  // Use plain array instead of a Vector to minimize memory overhead.
  unsigned descendant_selector_identifier_hashes_[kMaximumIdentifierCount];
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::RuleData);

namespace blink {

struct SameSizeAsRuleData {
  DISALLOW_NEW();
  Member<void*> a;
  unsigned b;
  unsigned c;
  unsigned d[4];
};

static_assert(sizeof(RuleData) == sizeof(SameSizeAsRuleData),
              "RuleData should stay small");

class CORE_EXPORT RuleSet : public GarbageCollectedFinalized<RuleSet> {
  WTF_MAKE_NONCOPYABLE(RuleSet);

 public:
  static RuleSet* Create() { return new RuleSet; }

  void AddRulesFromSheet(StyleSheetContents*,
                         const MediaQueryEvaluator&,
                         AddRuleFlags = kRuleHasNoSpecialState);
  void AddStyleRule(StyleRule*, AddRuleFlags);
  void AddRule(StyleRule*, unsigned selector_index, AddRuleFlags);

  const RuleFeatureSet& Features() const { return features_; }

  const HeapTerminatedArray<RuleData>* IdRules(const AtomicString& key) const {
    DCHECK(!pending_rules_);
    return id_rules_.at(key);
  }
  const HeapTerminatedArray<RuleData>* ClassRules(
      const AtomicString& key) const {
    DCHECK(!pending_rules_);
    return class_rules_.at(key);
  }
  const HeapTerminatedArray<RuleData>* TagRules(const AtomicString& key) const {
    DCHECK(!pending_rules_);
    return tag_rules_.at(key);
  }
  const HeapTerminatedArray<RuleData>* ShadowPseudoElementRules(
      const AtomicString& key) const {
    DCHECK(!pending_rules_);
    return shadow_pseudo_element_rules_.at(key);
  }
  const HeapVector<RuleData>* LinkPseudoClassRules() const {
    DCHECK(!pending_rules_);
    return &link_pseudo_class_rules_;
  }
  const HeapVector<RuleData>* CuePseudoRules() const {
    DCHECK(!pending_rules_);
    return &cue_pseudo_rules_;
  }
  const HeapVector<RuleData>* FocusPseudoClassRules() const {
    DCHECK(!pending_rules_);
    return &focus_pseudo_class_rules_;
  }
  const HeapVector<RuleData>* UniversalRules() const {
    DCHECK(!pending_rules_);
    return &universal_rules_;
  }
  const HeapVector<RuleData>* ShadowHostRules() const {
    DCHECK(!pending_rules_);
    return &shadow_host_rules_;
  }
  const HeapVector<Member<StyleRulePage>>& PageRules() const {
    DCHECK(!pending_rules_);
    return page_rules_;
  }
  const HeapVector<Member<StyleRuleFontFace>>& FontFaceRules() const {
    return font_face_rules_;
  }
  const HeapVector<Member<StyleRuleKeyframes>>& KeyframesRules() const {
    return keyframes_rules_;
  }
  const HeapVector<MinimalRuleData>& DeepCombinatorOrShadowPseudoRules() const {
    return deep_combinator_or_shadow_pseudo_rules_;
  }
  const HeapVector<MinimalRuleData>& ContentPseudoElementRules() const {
    return content_pseudo_element_rules_;
  }
  const HeapVector<MinimalRuleData>& SlottedPseudoElementRules() const {
    return slotted_pseudo_element_rules_;
  }

  unsigned RuleCount() const { return rule_count_; }

  void CompactRulesIfNeeded() {
    if (!pending_rules_)
      return;
    CompactRules();
  }

  bool HasSlottedRules() const {
    return !slotted_pseudo_element_rules_.IsEmpty();
  }

  bool HasV0BoundaryCrossingRules() const {
    return !deep_combinator_or_shadow_pseudo_rules_.IsEmpty() ||
           !content_pseudo_element_rules_.IsEmpty();
  }

  bool NeedsFullRecalcForRuleSetInvalidation() const {
    return features_.NeedsFullRecalcForRuleSetInvalidation() ||
           HasV0BoundaryCrossingRules();
  }

#ifndef NDEBUG
  void Show() const;
#endif

  DECLARE_TRACE();

 private:
  using PendingRuleMap =
      HeapHashMap<AtomicString, Member<HeapLinkedStack<RuleData>>>;
  using CompactRuleMap =
      HeapHashMap<AtomicString, Member<HeapTerminatedArray<RuleData>>>;

  RuleSet() : rule_count_(0) {}

  void AddToRuleSet(const AtomicString& key, PendingRuleMap&, const RuleData&);
  void AddPageRule(StyleRulePage*);
  void AddViewportRule(StyleRuleViewport*);
  void AddFontFaceRule(StyleRuleFontFace*);
  void AddKeyframesRule(StyleRuleKeyframes*);

  void AddChildRules(const HeapVector<Member<StyleRuleBase>>&,
                     const MediaQueryEvaluator& medium,
                     AddRuleFlags);
  bool FindBestRuleSetAndAdd(const CSSSelector&, RuleData&);

  void CompactRules();
  static void CompactPendingRules(PendingRuleMap&, CompactRuleMap&);

  class PendingRuleMaps : public GarbageCollected<PendingRuleMaps> {
   public:
    static PendingRuleMaps* Create() { return new PendingRuleMaps; }

    PendingRuleMap id_rules;
    PendingRuleMap class_rules;
    PendingRuleMap tag_rules;
    PendingRuleMap shadow_pseudo_element_rules;

    DECLARE_TRACE();

   private:
    PendingRuleMaps() {}
  };

  PendingRuleMaps* EnsurePendingRules() {
    if (!pending_rules_)
      pending_rules_ = PendingRuleMaps::Create();
    return pending_rules_.Get();
  }

  CompactRuleMap id_rules_;
  CompactRuleMap class_rules_;
  CompactRuleMap tag_rules_;
  CompactRuleMap shadow_pseudo_element_rules_;
  HeapVector<RuleData> link_pseudo_class_rules_;
  HeapVector<RuleData> cue_pseudo_rules_;
  HeapVector<RuleData> focus_pseudo_class_rules_;
  HeapVector<RuleData> universal_rules_;
  HeapVector<RuleData> shadow_host_rules_;
  RuleFeatureSet features_;
  HeapVector<Member<StyleRulePage>> page_rules_;
  HeapVector<Member<StyleRuleFontFace>> font_face_rules_;
  HeapVector<Member<StyleRuleKeyframes>> keyframes_rules_;
  HeapVector<MinimalRuleData> deep_combinator_or_shadow_pseudo_rules_;
  HeapVector<MinimalRuleData> content_pseudo_element_rules_;
  HeapVector<MinimalRuleData> slotted_pseudo_element_rules_;

  unsigned rule_count_;
  Member<PendingRuleMaps> pending_rules_;

#ifndef NDEBUG
  HeapVector<RuleData> all_rules_;
#endif
};

}  // namespace blink

#endif  // RuleSet_h
