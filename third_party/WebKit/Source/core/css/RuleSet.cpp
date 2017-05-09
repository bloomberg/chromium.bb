/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "core/css/RuleSet.h"

#include "core/HTMLNames.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/SelectorFilter.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/StyleSheetContents.h"
#include "core/html/track/TextTrackCue.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/heap/HeapTerminatedArrayBuilder.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"

#include "platform/wtf/TerminatedArrayBuilder.h"

namespace blink {

using namespace HTMLNames;

// -----------------------------------------------------------------

static bool ContainsUncommonAttributeSelector(const CSSSelector&);

static inline bool SelectorListContainsUncommonAttributeSelector(
    const CSSSelector* selector) {
  const CSSSelectorList* selector_list = selector->SelectorList();
  if (!selector_list)
    return false;
  for (const CSSSelector* selector = selector_list->First(); selector;
       selector = CSSSelectorList::Next(*selector)) {
    if (ContainsUncommonAttributeSelector(*selector))
      return true;
  }
  return false;
}

static inline bool IsCommonAttributeSelectorAttribute(
    const QualifiedName& attribute) {
  // These are explicitly tested for equality in canShareStyleWithElement.
  return attribute == typeAttr || attribute == readonlyAttr;
}

static bool ContainsUncommonAttributeSelector(const CSSSelector& selector) {
  const CSSSelector* current = &selector;
  for (; current; current = current->TagHistory()) {
    // Allow certain common attributes (used in the default style) in the
    // selectors that match the current element.
    if (current->IsAttributeSelector() &&
        !IsCommonAttributeSelectorAttribute(current->Attribute()))
      return true;
    if (SelectorListContainsUncommonAttributeSelector(current))
      return true;
    if (current->RelationIsAffectedByPseudoContent() ||
        current->GetPseudoType() == CSSSelector::kPseudoSlotted)
      return false;
    if (current->Relation() != CSSSelector::kSubSelector) {
      current = current->TagHistory();
      break;
    }
  }

  for (; current; current = current->TagHistory()) {
    if (current->IsAttributeSelector())
      return true;
    if (SelectorListContainsUncommonAttributeSelector(current))
      return true;
  }
  return false;
}

static inline PropertyWhitelistType DeterminePropertyWhitelistType(
    const AddRuleFlags add_rule_flags,
    const CSSSelector& selector) {
  for (const CSSSelector* component = &selector; component;
       component = component->TagHistory()) {
    if (component->GetPseudoType() == CSSSelector::kPseudoCue ||
        (component->Match() == CSSSelector::kPseudoElement &&
         component->Value() == TextTrackCue::CueShadowPseudoId()))
      return kPropertyWhitelistCue;
    if (component->GetPseudoType() == CSSSelector::kPseudoFirstLetter)
      return kPropertyWhitelistFirstLetter;
  }
  return kPropertyWhitelistNone;
}

RuleData::RuleData(StyleRule* rule,
                   unsigned selector_index,
                   unsigned position,
                   AddRuleFlags add_rule_flags)
    : rule_(rule),
      selector_index_(selector_index),
      is_last_in_array_(false),
      position_(position),
      specificity_(Selector().Specificity()),
      contains_uncommon_attribute_selector_(
          blink::ContainsUncommonAttributeSelector(Selector())),
      link_match_type_(Selector().ComputeLinkMatchType()),
      has_document_security_origin_(add_rule_flags &
                                    kRuleHasDocumentSecurityOrigin),
      property_whitelist_(
          DeterminePropertyWhitelistType(add_rule_flags, Selector())),
      descendant_selector_identifier_hashes_() {
  SelectorFilter::CollectIdentifierHashes(
      Selector(), descendant_selector_identifier_hashes_,
      kMaximumIdentifierCount);
}

void RuleSet::AddToRuleSet(const AtomicString& key,
                           PendingRuleMap& map,
                           const RuleData& rule_data) {
  Member<HeapLinkedStack<RuleData>>& rules =
      map.insert(key, nullptr).stored_value->value;
  if (!rules)
    rules = new HeapLinkedStack<RuleData>;
  rules->Push(rule_data);
}

static void ExtractValuesforSelector(const CSSSelector* selector,
                                     AtomicString& id,
                                     AtomicString& class_name,
                                     AtomicString& custom_pseudo_element_name,
                                     AtomicString& tag_name) {
  switch (selector->Match()) {
    case CSSSelector::kId:
      id = selector->Value();
      break;
    case CSSSelector::kClass:
      class_name = selector->Value();
      break;
    case CSSSelector::kTag:
      if (selector->TagQName().LocalName() != g_star_atom)
        tag_name = selector->TagQName().LocalName();
      break;
    default:
      break;
  }
  if (selector->GetPseudoType() == CSSSelector::kPseudoWebKitCustomElement ||
      selector->GetPseudoType() == CSSSelector::kPseudoBlinkInternalElement)
    custom_pseudo_element_name = selector->Value();
}

bool RuleSet::FindBestRuleSetAndAdd(const CSSSelector& component,
                                    RuleData& rule_data) {
  AtomicString id;
  AtomicString class_name;
  AtomicString custom_pseudo_element_name;
  AtomicString tag_name;

#ifndef NDEBUG
  all_rules_.push_back(rule_data);
#endif

  const CSSSelector* it = &component;
  for (; it && it->Relation() == CSSSelector::kSubSelector;
       it = it->TagHistory())
    ExtractValuesforSelector(it, id, class_name, custom_pseudo_element_name,
                             tag_name);
  if (it)
    ExtractValuesforSelector(it, id, class_name, custom_pseudo_element_name,
                             tag_name);

  // Prefer rule sets in order of most likely to apply infrequently.
  if (!id.IsEmpty()) {
    AddToRuleSet(id, EnsurePendingRules()->id_rules, rule_data);
    return true;
  }
  if (!class_name.IsEmpty()) {
    AddToRuleSet(class_name, EnsurePendingRules()->class_rules, rule_data);
    return true;
  }
  if (!custom_pseudo_element_name.IsEmpty()) {
    // Custom pseudos come before ids and classes in the order of tagHistory,
    // and have a relation of ShadowPseudo between them. Therefore we should
    // never be a situation where extractValuesforSelector finsd id and
    // className in addition to custom pseudo.
    DCHECK(id.IsEmpty());
    DCHECK(class_name.IsEmpty());
    AddToRuleSet(custom_pseudo_element_name,
                 EnsurePendingRules()->shadow_pseudo_element_rules, rule_data);
    return true;
  }

  switch (component.GetPseudoType()) {
    case CSSSelector::kPseudoCue:
      cue_pseudo_rules_.push_back(rule_data);
      return true;
    case CSSSelector::kPseudoLink:
    case CSSSelector::kPseudoVisited:
    case CSSSelector::kPseudoAnyLink:
      link_pseudo_class_rules_.push_back(rule_data);
      return true;
    case CSSSelector::kPseudoFocus:
      focus_pseudo_class_rules_.push_back(rule_data);
      return true;
    case CSSSelector::kPseudoPlaceholder:
      placeholder_pseudo_rules_.push_back(rule_data);
      return true;
    default:
      break;
  }

  if (!tag_name.IsEmpty()) {
    AddToRuleSet(tag_name, EnsurePendingRules()->tag_rules, rule_data);
    return true;
  }

  if (component.IsHostPseudoClass()) {
    shadow_host_rules_.push_back(rule_data);
    return true;
  }

  return false;
}

void RuleSet::AddRule(StyleRule* rule,
                      unsigned selector_index,
                      AddRuleFlags add_rule_flags) {
  RuleData rule_data(rule, selector_index, rule_count_++, add_rule_flags);
  if (features_.CollectFeaturesFromRuleData(rule_data) ==
      RuleFeatureSet::kSelectorNeverMatches)
    return;

  if (!FindBestRuleSetAndAdd(rule_data.Selector(), rule_data)) {
    // If we didn't find a specialized map to stick it in, file under universal
    // rules.
    universal_rules_.push_back(rule_data);
  }
}

void RuleSet::AddPageRule(StyleRulePage* rule) {
  EnsurePendingRules();  // So that page_rules_.ShrinkToFit() gets called.
  page_rules_.push_back(rule);
}

void RuleSet::AddFontFaceRule(StyleRuleFontFace* rule) {
  EnsurePendingRules();  // So that font_face_rules_.ShrinkToFit() gets called.
  font_face_rules_.push_back(rule);
}

void RuleSet::AddKeyframesRule(StyleRuleKeyframes* rule) {
  EnsurePendingRules();  // So that keyframes_rules_.ShrinkToFit() gets called.
  keyframes_rules_.push_back(rule);
}

void RuleSet::AddChildRules(const HeapVector<Member<StyleRuleBase>>& rules,
                            const MediaQueryEvaluator& medium,
                            AddRuleFlags add_rule_flags) {
  for (unsigned i = 0; i < rules.size(); ++i) {
    StyleRuleBase* rule = rules[i].Get();

    if (rule->IsStyleRule()) {
      StyleRule* style_rule = ToStyleRule(rule);

      const CSSSelectorList& selector_list = style_rule->SelectorList();
      for (const CSSSelector* selector = selector_list.First(); selector;
           selector = selector_list.Next(*selector)) {
        size_t selector_index = selector_list.SelectorIndex(*selector);
        if (selector->HasDeepCombinatorOrShadowPseudo()) {
          deep_combinator_or_shadow_pseudo_rules_.push_back(
              MinimalRuleData(style_rule, selector_index, add_rule_flags));
        } else if (selector->HasContentPseudo()) {
          content_pseudo_element_rules_.push_back(
              MinimalRuleData(style_rule, selector_index, add_rule_flags));
        } else if (selector->HasSlottedPseudo()) {
          slotted_pseudo_element_rules_.push_back(
              MinimalRuleData(style_rule, selector_index, add_rule_flags));
        } else {
          AddRule(style_rule, selector_index, add_rule_flags);
        }
      }
    } else if (rule->IsPageRule()) {
      AddPageRule(ToStyleRulePage(rule));
    } else if (rule->IsMediaRule()) {
      StyleRuleMedia* media_rule = ToStyleRuleMedia(rule);
      if (!media_rule->MediaQueries() ||
          medium.Eval(*media_rule->MediaQueries(),
                      &features_.ViewportDependentMediaQueryResults(),
                      &features_.DeviceDependentMediaQueryResults()))
        AddChildRules(media_rule->ChildRules(), medium, add_rule_flags);
    } else if (rule->IsFontFaceRule()) {
      AddFontFaceRule(ToStyleRuleFontFace(rule));
    } else if (rule->IsKeyframesRule()) {
      AddKeyframesRule(ToStyleRuleKeyframes(rule));
    } else if (rule->IsSupportsRule() &&
               ToStyleRuleSupports(rule)->ConditionIsSupported()) {
      AddChildRules(ToStyleRuleSupports(rule)->ChildRules(), medium,
                    add_rule_flags);
    }
  }
}

void RuleSet::AddRulesFromSheet(StyleSheetContents* sheet,
                                const MediaQueryEvaluator& medium,
                                AddRuleFlags add_rule_flags) {
  TRACE_EVENT0("blink", "RuleSet::addRulesFromSheet");

  DCHECK(sheet);

  const HeapVector<Member<StyleRuleImport>>& import_rules =
      sheet->ImportRules();
  for (unsigned i = 0; i < import_rules.size(); ++i) {
    StyleRuleImport* import_rule = import_rules[i].Get();
    if (import_rule->GetStyleSheet() &&
        (!import_rule->MediaQueries() ||
         medium.Eval(*import_rule->MediaQueries(),
                     &features_.ViewportDependentMediaQueryResults(),
                     &features_.DeviceDependentMediaQueryResults())))
      AddRulesFromSheet(import_rule->GetStyleSheet(), medium, add_rule_flags);
  }

  AddChildRules(sheet->ChildRules(), medium, add_rule_flags);
}

void RuleSet::AddStyleRule(StyleRule* rule, AddRuleFlags add_rule_flags) {
  for (size_t selector_index = 0; selector_index != kNotFound;
       selector_index =
           rule->SelectorList().IndexOfNextSelectorAfter(selector_index))
    AddRule(rule, selector_index, add_rule_flags);
}

void RuleSet::CompactPendingRules(PendingRuleMap& pending_map,
                                  CompactRuleMap& compact_map) {
  for (auto& item : pending_map) {
    HeapLinkedStack<RuleData>* pending_rules = item.value.Release();
    CompactRuleMap::ValueType* compact_rules =
        compact_map.insert(item.key, nullptr).stored_value;

    HeapTerminatedArrayBuilder<RuleData> builder(
        compact_rules->value.Release());
    builder.Grow(pending_rules->size());
    while (!pending_rules->IsEmpty()) {
      builder.Append(pending_rules->Peek());
      pending_rules->Pop();
    }

    compact_rules->value = builder.Release();
  }
}

void RuleSet::CompactRules() {
  DCHECK(pending_rules_);
  PendingRuleMaps* pending_rules = pending_rules_.Release();
  CompactPendingRules(pending_rules->id_rules, id_rules_);
  CompactPendingRules(pending_rules->class_rules, class_rules_);
  CompactPendingRules(pending_rules->tag_rules, tag_rules_);
  CompactPendingRules(pending_rules->shadow_pseudo_element_rules,
                      shadow_pseudo_element_rules_);
  link_pseudo_class_rules_.ShrinkToFit();
  cue_pseudo_rules_.ShrinkToFit();
  focus_pseudo_class_rules_.ShrinkToFit();
  placeholder_pseudo_rules_.ShrinkToFit();
  universal_rules_.ShrinkToFit();
  shadow_host_rules_.ShrinkToFit();
  page_rules_.ShrinkToFit();
  font_face_rules_.ShrinkToFit();
  keyframes_rules_.ShrinkToFit();
  deep_combinator_or_shadow_pseudo_rules_.ShrinkToFit();
  content_pseudo_element_rules_.ShrinkToFit();
  slotted_pseudo_element_rules_.ShrinkToFit();
}

DEFINE_TRACE(MinimalRuleData) {
  visitor->Trace(rule_);
}

DEFINE_TRACE(RuleData) {
  visitor->Trace(rule_);
}

DEFINE_TRACE(RuleSet::PendingRuleMaps) {
  visitor->Trace(id_rules);
  visitor->Trace(class_rules);
  visitor->Trace(tag_rules);
  visitor->Trace(shadow_pseudo_element_rules);
}

DEFINE_TRACE(RuleSet) {
  visitor->Trace(id_rules_);
  visitor->Trace(class_rules_);
  visitor->Trace(tag_rules_);
  visitor->Trace(shadow_pseudo_element_rules_);
  visitor->Trace(link_pseudo_class_rules_);
  visitor->Trace(cue_pseudo_rules_);
  visitor->Trace(focus_pseudo_class_rules_);
  visitor->Trace(placeholder_pseudo_rules_);
  visitor->Trace(universal_rules_);
  visitor->Trace(shadow_host_rules_);
  visitor->Trace(features_);
  visitor->Trace(page_rules_);
  visitor->Trace(font_face_rules_);
  visitor->Trace(keyframes_rules_);
  visitor->Trace(deep_combinator_or_shadow_pseudo_rules_);
  visitor->Trace(content_pseudo_element_rules_);
  visitor->Trace(slotted_pseudo_element_rules_);
  visitor->Trace(pending_rules_);
#ifndef NDEBUG
  visitor->Trace(all_rules_);
#endif
}

#ifndef NDEBUG
void RuleSet::Show() const {
  for (const auto& rule : all_rules_)
    rule.Selector().Show();
}
#endif

}  // namespace blink
