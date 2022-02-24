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

#include "third_party/blink/renderer/core/css/rule_feature_set.h"

#include <algorithm>
#include <bitset>
#include "base/auto_reset.h"
#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/invalidation/invalidation_set.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

bool SupportsInvalidation(CSSSelector::MatchType match) {
  switch (match) {
    case CSSSelector::kTag:
    case CSSSelector::kId:
    case CSSSelector::kClass:
    case CSSSelector::kAttributeExact:
    case CSSSelector::kAttributeSet:
    case CSSSelector::kAttributeHyphen:
    case CSSSelector::kAttributeList:
    case CSSSelector::kAttributeContain:
    case CSSSelector::kAttributeBegin:
    case CSSSelector::kAttributeEnd:
      return true;
    case CSSSelector::kUnknown:
    case CSSSelector::kPagePseudoClass:
      // These should not appear in StyleRule selectors.
      NOTREACHED();
      return false;
    default:
      // New match type added. Figure out if it needs a subtree invalidation or
      // not.
      NOTREACHED();
      return false;
  }
}

bool SupportsInvalidation(CSSSelector::PseudoType type) {
  switch (type) {
    case CSSSelector::kPseudoEmpty:
    case CSSSelector::kPseudoFirstChild:
    case CSSSelector::kPseudoFirstOfType:
    case CSSSelector::kPseudoLastChild:
    case CSSSelector::kPseudoLastOfType:
    case CSSSelector::kPseudoOnlyChild:
    case CSSSelector::kPseudoOnlyOfType:
    case CSSSelector::kPseudoNthChild:
    case CSSSelector::kPseudoNthOfType:
    case CSSSelector::kPseudoNthLastChild:
    case CSSSelector::kPseudoNthLastOfType:
    case CSSSelector::kPseudoPart:
    case CSSSelector::kPseudoState:
    case CSSSelector::kPseudoLink:
    case CSSSelector::kPseudoVisited:
    case CSSSelector::kPseudoAny:
    case CSSSelector::kPseudoWebkitAnyLink:
    case CSSSelector::kPseudoAnyLink:
    case CSSSelector::kPseudoAutofill:
    case CSSSelector::kPseudoWebKitAutofill:
    case CSSSelector::kPseudoAutofillPreviewed:
    case CSSSelector::kPseudoAutofillSelected:
    case CSSSelector::kPseudoHover:
    case CSSSelector::kPseudoDrag:
    case CSSSelector::kPseudoFocus:
    case CSSSelector::kPseudoFocusVisible:
    case CSSSelector::kPseudoFocusWithin:
    case CSSSelector::kPseudoActive:
    case CSSSelector::kPseudoChecked:
    case CSSSelector::kPseudoEnabled:
    case CSSSelector::kPseudoFullPageMedia:
    case CSSSelector::kPseudoDefault:
    case CSSSelector::kPseudoDisabled:
    case CSSSelector::kPseudoOptional:
    case CSSSelector::kPseudoPlaceholderShown:
    case CSSSelector::kPseudoRequired:
    case CSSSelector::kPseudoReadOnly:
    case CSSSelector::kPseudoReadWrite:
    case CSSSelector::kPseudoValid:
    case CSSSelector::kPseudoInvalid:
    case CSSSelector::kPseudoIndeterminate:
    case CSSSelector::kPseudoTarget:
    case CSSSelector::kPseudoBefore:
    case CSSSelector::kPseudoAfter:
    case CSSSelector::kPseudoMarker:
    case CSSSelector::kPseudoModal:
    case CSSSelector::kPseudoBackdrop:
    case CSSSelector::kPseudoLang:
    case CSSSelector::kPseudoDir:
    case CSSSelector::kPseudoNot:
    case CSSSelector::kPseudoPlaceholder:
    case CSSSelector::kPseudoFileSelectorButton:
    case CSSSelector::kPseudoResizer:
    case CSSSelector::kPseudoRoot:
    case CSSSelector::kPseudoScope:
    case CSSSelector::kPseudoScrollbar:
    case CSSSelector::kPseudoScrollbarButton:
    case CSSSelector::kPseudoScrollbarCorner:
    case CSSSelector::kPseudoScrollbarThumb:
    case CSSSelector::kPseudoScrollbarTrack:
    case CSSSelector::kPseudoScrollbarTrackPiece:
    case CSSSelector::kPseudoWindowInactive:
    case CSSSelector::kPseudoSelection:
    case CSSSelector::kPseudoCornerPresent:
    case CSSSelector::kPseudoDecrement:
    case CSSSelector::kPseudoIncrement:
    case CSSSelector::kPseudoHorizontal:
    case CSSSelector::kPseudoVertical:
    case CSSSelector::kPseudoStart:
    case CSSSelector::kPseudoEnd:
    case CSSSelector::kPseudoDoubleButton:
    case CSSSelector::kPseudoSingleButton:
    case CSSSelector::kPseudoNoButton:
    case CSSSelector::kPseudoFullScreen:
    case CSSSelector::kPseudoFullScreenAncestor:
    case CSSSelector::kPseudoFullscreen:
    case CSSSelector::kPseudoPaused:
    case CSSSelector::kPseudoPictureInPicture:
    case CSSSelector::kPseudoPlaying:
    case CSSSelector::kPseudoInRange:
    case CSSSelector::kPseudoOutOfRange:
    case CSSSelector::kPseudoWebKitCustomElement:
    case CSSSelector::kPseudoBlinkInternalElement:
    case CSSSelector::kPseudoCue:
    case CSSSelector::kPseudoFutureCue:
    case CSSSelector::kPseudoPastCue:
    case CSSSelector::kPseudoDefined:
    case CSSSelector::kPseudoHost:
    case CSSSelector::kPseudoSpatialNavigationFocus:
    case CSSSelector::kPseudoSpatialNavigationInterest:
    case CSSSelector::kPseudoHasDatalist:
    case CSSSelector::kPseudoIsHtml:
    case CSSSelector::kPseudoListBox:
    case CSSSelector::kPseudoMultiSelectFocus:
    case CSSSelector::kPseudoHostHasAppearance:
    case CSSSelector::kPseudoPopupOpen:
    case CSSSelector::kPseudoSlotted:
    case CSSSelector::kPseudoVideoPersistent:
    case CSSSelector::kPseudoVideoPersistentAncestor:
    case CSSSelector::kPseudoXrOverlay:
    case CSSSelector::kPseudoIs:
    case CSSSelector::kPseudoWhere:
    case CSSSelector::kPseudoTargetText:
    case CSSSelector::kPseudoHighlight:
    case CSSSelector::kPseudoSpellingError:
    case CSSSelector::kPseudoGrammarError:
    case CSSSelector::kPseudoHas:
    case CSSSelector::kPseudoTransition:
    case CSSSelector::kPseudoTransitionContainer:
    case CSSSelector::kPseudoTransitionNewContent:
    case CSSSelector::kPseudoTransitionOldContent:
      return true;
    case CSSSelector::kPseudoUnknown:
    case CSSSelector::kPseudoLeftPage:
    case CSSSelector::kPseudoRightPage:
    case CSSSelector::kPseudoFirstPage:
      // These should not appear in StyleRule selectors.
      NOTREACHED();
      return false;
    default:
      // New pseudo type added. Figure out if it needs a subtree invalidation or
      // not.
      NOTREACHED();
      return false;
  }
}

bool SupportsInvalidationWithSelectorList(CSSSelector::PseudoType pseudo) {
  return pseudo == CSSSelector::kPseudoAny ||
         pseudo == CSSSelector::kPseudoCue ||
         pseudo == CSSSelector::kPseudoHost ||
         pseudo == CSSSelector::kPseudoHostContext ||
         pseudo == CSSSelector::kPseudoIs ||
         pseudo == CSSSelector::kPseudoNot ||
         pseudo == CSSSelector::kPseudoSlotted ||
         pseudo == CSSSelector::kPseudoWhere;
}

bool RequiresSubtreeInvalidation(const CSSSelector& selector) {
  if (selector.Match() != CSSSelector::kPseudoElement &&
      selector.Match() != CSSSelector::kPseudoClass) {
    DCHECK(SupportsInvalidation(selector.Match()));
    return false;
  }

  switch (selector.GetPseudoType()) {
    case CSSSelector::kPseudoFirstLine:
    case CSSSelector::kPseudoFirstLetter:
    // FIXME: Most pseudo classes/elements above can be supported and moved
    // to assertSupportedPseudo(). Move on a case-by-case basis. If they
    // require subtree invalidation, document why.
    case CSSSelector::kPseudoHostContext:
      // :host-context matches a shadow host, yet the simple selectors inside
      // :host-context matches an ancestor of the shadow host.
      return true;
    default:
      DCHECK(SupportsInvalidation(selector.GetPseudoType()));
      return false;
  }
}

// Creates a copy of an InvalidationSet by combining an empty InvalidationSet
// (of the same type) with the specified InvalidationSet.
//
// See also InvalidationSet::Combine.
scoped_refptr<InvalidationSet> CopyInvalidationSet(
    const InvalidationSet& invalidation_set) {
  if (invalidation_set.IsSiblingInvalidationSet()) {
    scoped_refptr<InvalidationSet> copy =
        SiblingInvalidationSet::Create(nullptr);
    copy->Combine(invalidation_set);
    return copy;
  }
  if (invalidation_set.IsSelfInvalidationSet()) {
    scoped_refptr<InvalidationSet> copy = DescendantInvalidationSet::Create();
    copy->SetInvalidatesSelf();
    return copy;
  }
  scoped_refptr<InvalidationSet> copy = DescendantInvalidationSet::Create();
  copy->Combine(invalidation_set);
  return copy;
}

template <typename KeyType,
          typename MapType = HashMap<KeyType, scoped_refptr<InvalidationSet>>>
bool InvalidationSetMapsEqual(const MapType& a, const MapType& b) {
  if (a.size() != b.size())
    return false;
  for (const auto& entry : a) {
    auto it = b.find(entry.key);
    if (it == b.end())
      return false;
    if (!base::ValuesEquivalent(entry.value, it->value))
      return false;
  }
  return true;
}

void ExtractInvalidationSets(InvalidationSet* invalidation_set,
                             DescendantInvalidationSet*& descendants,
                             SiblingInvalidationSet*& siblings) {
  CHECK(invalidation_set->IsAlive());
  if (auto* descendant =
          DynamicTo<DescendantInvalidationSet>(invalidation_set)) {
    descendants = descendant;
    siblings = nullptr;
    return;
  }

  siblings = To<SiblingInvalidationSet>(invalidation_set);
  descendants = siblings->Descendants();
}

}  // anonymous namespace

InvalidationSet& RuleFeatureSet::EnsureMutableInvalidationSet(
    scoped_refptr<InvalidationSet>& invalidation_set,
    InvalidationType type,
    PositionType position) {
  if (invalidation_set && invalidation_set->IsSelfInvalidationSet()) {
    if (type == InvalidationType::kInvalidateDescendants &&
        position == kSubject)
      return *invalidation_set;
    // If we are retrieving the invalidation set for a simple selector in a non-
    // rightmost compound, it means we plan to add features to the set. If so,
    // create a DescendantInvalidationSet we are allowed to modify.
    //
    // Note that we also construct a DescendantInvalidationSet instead of using
    // the SelfInvalidationSet() when we create a SiblingInvalidationSet. We may
    // be able to let SiblingInvalidationSets reference the singleton set for
    // descendants as well. TODO(futhark@chromium.org)
    invalidation_set = CopyInvalidationSet(*invalidation_set);
    DCHECK(invalidation_set->HasOneRef());
  }
  if (!invalidation_set) {
    if (type == InvalidationType::kInvalidateDescendants) {
      if (position == kSubject)
        invalidation_set = InvalidationSet::SelfInvalidationSet();
      else
        invalidation_set = DescendantInvalidationSet::Create();
    } else {
      invalidation_set = SiblingInvalidationSet::Create(nullptr);
    }
    return *invalidation_set;
  }
  // If the currently stored invalidation_set is shared with other
  // RuleFeatureSets (for example), we must copy it before modifying it.
  if (!invalidation_set->HasOneRef()) {
    invalidation_set = CopyInvalidationSet(*invalidation_set);
    DCHECK(invalidation_set->HasOneRef());
  }
  if (invalidation_set->GetType() == type)
    return *invalidation_set;

  if (type == InvalidationType::kInvalidateDescendants)
    return To<SiblingInvalidationSet>(*invalidation_set).EnsureDescendants();

  scoped_refptr<InvalidationSet> descendants = invalidation_set;
  invalidation_set = SiblingInvalidationSet::Create(
      To<DescendantInvalidationSet>(descendants.get()));
  return *invalidation_set;
}

InvalidationSet& RuleFeatureSet::EnsureInvalidationSet(InvalidationSetMap& map,
                                                       const AtomicString& key,
                                                       InvalidationType type,
                                                       PositionType position) {
  scoped_refptr<InvalidationSet>& invalidation_set =
      map.insert(key, nullptr).stored_value->value;
  return EnsureMutableInvalidationSet(invalidation_set, type, position);
}

InvalidationSet& RuleFeatureSet::EnsureInvalidationSet(
    PseudoTypeInvalidationSetMap& map,
    CSSSelector::PseudoType key,
    InvalidationType type,
    PositionType position) {
  scoped_refptr<InvalidationSet>& invalidation_set =
      map.insert(key, nullptr).stored_value->value;
  return EnsureMutableInvalidationSet(invalidation_set, type, position);
}

void RuleFeatureSet::AddInvalidationSet(
    InvalidationSetMap& map,
    const AtomicString& key,
    scoped_refptr<InvalidationSet> invalidation_set) {
  DCHECK(invalidation_set);
  scoped_refptr<InvalidationSet>& slot =
      map.insert(key, nullptr).stored_value->value;
  if (!slot) {
    slot = std::move(invalidation_set);
  } else {
    EnsureMutableInvalidationSet(
        slot, invalidation_set->GetType(),
        invalidation_set->IsSelfInvalidationSet() ? kSubject : kAncestor)
        .Combine(*invalidation_set);
  }
}

void RuleFeatureSet::AddInvalidationSet(
    PseudoTypeInvalidationSetMap& map,
    CSSSelector::PseudoType key,
    scoped_refptr<InvalidationSet> invalidation_set) {
  DCHECK(invalidation_set);
  scoped_refptr<InvalidationSet>& slot =
      map.insert(key, nullptr).stored_value->value;
  if (!slot) {
    slot = std::move(invalidation_set);
  } else {
    EnsureMutableInvalidationSet(
        slot, invalidation_set->GetType(),
        invalidation_set->IsSelfInvalidationSet() ? kSubject : kAncestor)
        .Combine(*invalidation_set);
  }
}

RuleFeatureSet::RuleFeatureSet() : is_alive_(true) {}

RuleFeatureSet::~RuleFeatureSet() {
  CHECK(is_alive_);

  metadata_.Clear();
  class_invalidation_sets_.clear();
  attribute_invalidation_sets_.clear();
  id_invalidation_sets_.clear();
  pseudo_invalidation_sets_.clear();
  universal_sibling_invalidation_set_ = nullptr;
  nth_invalidation_set_ = nullptr;
  classes_in_has_argument_.clear();
  attributes_in_has_argument_.clear();
  ids_in_has_argument_.clear();
  tag_names_in_has_argument_.clear();
  universal_in_has_argument_ = false;
  pseudos_in_has_argument_.clear();

  is_alive_ = false;
}

bool RuleFeatureSet::operator==(const RuleFeatureSet& other) const {
  return metadata_ == other.metadata_ &&
         InvalidationSetMapsEqual<AtomicString>(
             class_invalidation_sets_, other.class_invalidation_sets_) &&
         InvalidationSetMapsEqual<AtomicString>(id_invalidation_sets_,
                                                other.id_invalidation_sets_) &&
         InvalidationSetMapsEqual<AtomicString>(
             attribute_invalidation_sets_,
             other.attribute_invalidation_sets_) &&
         InvalidationSetMapsEqual<CSSSelector::PseudoType>(
             pseudo_invalidation_sets_, other.pseudo_invalidation_sets_) &&
         base::ValuesEquivalent(universal_sibling_invalidation_set_,
                                other.universal_sibling_invalidation_set_) &&
         base::ValuesEquivalent(nth_invalidation_set_,
                                other.nth_invalidation_set_) &&
         base::ValuesEquivalent(universal_sibling_invalidation_set_,
                                other.universal_sibling_invalidation_set_) &&
         base::ValuesEquivalent(type_rule_invalidation_set_,
                                other.type_rule_invalidation_set_) &&
         viewport_dependent_media_query_results_ ==
             other.viewport_dependent_media_query_results_ &&
         device_dependent_media_query_results_ ==
             other.device_dependent_media_query_results_ &&
         classes_in_has_argument_ == other.classes_in_has_argument_ &&
         attributes_in_has_argument_ == other.attributes_in_has_argument_ &&
         ids_in_has_argument_ == other.ids_in_has_argument_ &&
         tag_names_in_has_argument_ == other.tag_names_in_has_argument_ &&
         universal_in_has_argument_ == other.universal_in_has_argument_ &&
         pseudos_in_has_argument_ == other.pseudos_in_has_argument_ &&
         is_alive_ == other.is_alive_;
}

ALWAYS_INLINE InvalidationSet& RuleFeatureSet::EnsureClassInvalidationSet(
    const AtomicString& class_name,
    InvalidationType type,
    PositionType position) {
  CHECK(!class_name.IsEmpty());
  return EnsureInvalidationSet(class_invalidation_sets_, class_name, type,
                               position);
}

ALWAYS_INLINE InvalidationSet& RuleFeatureSet::EnsureAttributeInvalidationSet(
    const AtomicString& attribute_name,
    InvalidationType type,
    PositionType position) {
  CHECK(!attribute_name.IsEmpty());
  return EnsureInvalidationSet(attribute_invalidation_sets_, attribute_name,
                               type, position);
}

ALWAYS_INLINE InvalidationSet& RuleFeatureSet::EnsureIdInvalidationSet(
    const AtomicString& id,
    InvalidationType type,
    PositionType position) {
  CHECK(!id.IsEmpty());
  return EnsureInvalidationSet(id_invalidation_sets_, id, type, position);
}

ALWAYS_INLINE InvalidationSet& RuleFeatureSet::EnsurePseudoInvalidationSet(
    CSSSelector::PseudoType pseudo_type,
    InvalidationType type,
    PositionType position) {
  CHECK_NE(pseudo_type, CSSSelector::kPseudoUnknown);
  return EnsureInvalidationSet(pseudo_invalidation_sets_, pseudo_type, type,
                               position);
}

void RuleFeatureSet::UpdateFeaturesFromCombinator(
    const CSSSelector& last_in_compound,
    const CSSSelector* last_compound_in_adjacent_chain,
    InvalidationSetFeatures& last_compound_in_adjacent_chain_features,
    InvalidationSetFeatures*& sibling_features,
    InvalidationSetFeatures& descendant_features) {
  if (last_in_compound.IsAdjacentSelector()) {
    if (!sibling_features) {
      sibling_features = &last_compound_in_adjacent_chain_features;
      if (last_compound_in_adjacent_chain) {
        ExtractInvalidationSetFeaturesFromCompound(
            *last_compound_in_adjacent_chain,
            last_compound_in_adjacent_chain_features, kAncestor);
        if (!last_compound_in_adjacent_chain_features.HasFeatures()) {
          last_compound_in_adjacent_chain_features.invalidation_flags
              .SetWholeSubtreeInvalid(true);
        }
      }
    }
    if (sibling_features->max_direct_adjacent_selectors ==
        SiblingInvalidationSet::kDirectAdjacentMax) {
      return;
    }
    if (last_in_compound.Relation() == CSSSelector::kDirectAdjacent) {
      ++sibling_features->max_direct_adjacent_selectors;
    } else {
      sibling_features->max_direct_adjacent_selectors =
          SiblingInvalidationSet::kDirectAdjacentMax;
    }
    return;
  }

  if (sibling_features &&
      last_compound_in_adjacent_chain_features.max_direct_adjacent_selectors)
    last_compound_in_adjacent_chain_features = InvalidationSetFeatures();

  sibling_features = nullptr;

  if (last_in_compound.IsUAShadowSelector())
    descendant_features.invalidation_flags.SetTreeBoundaryCrossing(true);
  if (last_in_compound.Relation() == CSSSelector::kShadowSlot)
    descendant_features.invalidation_flags.SetInsertionPointCrossing(true);
}

void RuleFeatureSet::ExtractInvalidationSetFeaturesFromSimpleSelector(
    const CSSSelector& selector,
    InvalidationSetFeatures& features) {
  features.has_features_for_rule_set_invalidation |=
      selector.IsIdClassOrAttributeSelector();

  if (selector.Match() == CSSSelector::kTag &&
      selector.TagQName().LocalName() != CSSSelector::UniversalSelectorAtom()) {
    features.NarrowToTag(selector.TagQName().LocalName());
    return;
  }
  if (selector.Match() == CSSSelector::kId) {
    features.NarrowToId(selector.Value());
    return;
  }
  if (selector.Match() == CSSSelector::kClass) {
    features.NarrowToClass(selector.Value());
    return;
  }
  if (selector.IsAttributeSelector()) {
    features.NarrowToAttribute(selector.Attribute().LocalName());
    return;
  }
  switch (selector.GetPseudoType()) {
    case CSSSelector::kPseudoWebKitCustomElement:
    case CSSSelector::kPseudoBlinkInternalElement:
      features.invalidation_flags.SetInvalidateCustomPseudo(true);
      return;
    case CSSSelector::kPseudoSlotted:
      features.invalidation_flags.SetInvalidatesSlotted(true);
      return;
    case CSSSelector::kPseudoPart:
      features.invalidation_flags.SetInvalidatesParts(true);
      features.invalidation_flags.SetTreeBoundaryCrossing(true);
      return;
    default:
      return;
  }
}

InvalidationSet* RuleFeatureSet::InvalidationSetForSimpleSelector(
    const CSSSelector& selector,
    InvalidationType type,
    PositionType position) {
  if (selector.Match() == CSSSelector::kClass)
    return &EnsureClassInvalidationSet(selector.Value(), type, position);
  if (selector.IsAttributeSelector()) {
    return &EnsureAttributeInvalidationSet(selector.Attribute().LocalName(),
                                           type, position);
  }
  if (selector.Match() == CSSSelector::kId)
    return &EnsureIdInvalidationSet(selector.Value(), type, position);
  if (selector.Match() == CSSSelector::kPseudoClass) {
    switch (selector.GetPseudoType()) {
      case CSSSelector::kPseudoEmpty:
      case CSSSelector::kPseudoFirstChild:
      case CSSSelector::kPseudoLastChild:
      case CSSSelector::kPseudoOnlyChild:
      case CSSSelector::kPseudoLink:
      case CSSSelector::kPseudoVisited:
      case CSSSelector::kPseudoWebkitAnyLink:
      case CSSSelector::kPseudoAnyLink:
      case CSSSelector::kPseudoAutofill:
      case CSSSelector::kPseudoWebKitAutofill:
      case CSSSelector::kPseudoAutofillPreviewed:
      case CSSSelector::kPseudoAutofillSelected:
      case CSSSelector::kPseudoHover:
      case CSSSelector::kPseudoDrag:
      case CSSSelector::kPseudoFocus:
      case CSSSelector::kPseudoFocusVisible:
      case CSSSelector::kPseudoFocusWithin:
      case CSSSelector::kPseudoActive:
      case CSSSelector::kPseudoChecked:
      case CSSSelector::kPseudoEnabled:
      case CSSSelector::kPseudoDefault:
      case CSSSelector::kPseudoDisabled:
      case CSSSelector::kPseudoOptional:
      case CSSSelector::kPseudoPlaceholderShown:
      case CSSSelector::kPseudoRequired:
      case CSSSelector::kPseudoReadOnly:
      case CSSSelector::kPseudoReadWrite:
      case CSSSelector::kPseudoState:
      case CSSSelector::kPseudoValid:
      case CSSSelector::kPseudoInvalid:
      case CSSSelector::kPseudoIndeterminate:
      case CSSSelector::kPseudoTarget:
      case CSSSelector::kPseudoLang:
      case CSSSelector::kPseudoDir:
      case CSSSelector::kPseudoFullScreen:
      case CSSSelector::kPseudoFullScreenAncestor:
      case CSSSelector::kPseudoFullscreen:
      case CSSSelector::kPseudoPaused:
      case CSSSelector::kPseudoPictureInPicture:
      case CSSSelector::kPseudoPlaying:
      case CSSSelector::kPseudoInRange:
      case CSSSelector::kPseudoOutOfRange:
      case CSSSelector::kPseudoDefined:
      case CSSSelector::kPseudoPopupOpen:
      case CSSSelector::kPseudoVideoPersistent:
      case CSSSelector::kPseudoVideoPersistentAncestor:
      case CSSSelector::kPseudoXrOverlay:
      case CSSSelector::kPseudoSpatialNavigationInterest:
      case CSSSelector::kPseudoHasDatalist:
      case CSSSelector::kPseudoMultiSelectFocus:
      case CSSSelector::kPseudoModal:
        return &EnsurePseudoInvalidationSet(selector.GetPseudoType(), type,
                                            position);
      case CSSSelector::kPseudoFirstOfType:
      case CSSSelector::kPseudoLastOfType:
      case CSSSelector::kPseudoOnlyOfType:
      case CSSSelector::kPseudoNthChild:
      case CSSSelector::kPseudoNthOfType:
      case CSSSelector::kPseudoNthLastChild:
      case CSSSelector::kPseudoNthLastOfType:
        return &EnsureNthInvalidationSet();
      case CSSSelector::kPseudoHas:
        return position == kAncestor
                   ? &EnsurePseudoInvalidationSet(selector.GetPseudoType(),
                                                  type, position)
                   : nullptr;
      case CSSSelector::kPseudoPart:
      default:
        break;
    }
  }
  return nullptr;
}

void RuleFeatureSet::UpdateInvalidationSets(const RuleData* rule_data) {
  InvalidationSetFeatures features;
  FeatureInvalidationType feature_invalidation_type =
      UpdateInvalidationSetsForComplex(rule_data->Selector(), features,
                                       kSubject, CSSSelector::kPseudoUnknown);
  if (feature_invalidation_type ==
      FeatureInvalidationType::kRequiresSubtreeInvalidation) {
    features.invalidation_flags.SetWholeSubtreeInvalid(true);
  }
  UpdateRuleSetInvalidation(features);
}

RuleFeatureSet::FeatureInvalidationType
RuleFeatureSet::UpdateInvalidationSetsForComplex(
    const CSSSelector& complex,
    InvalidationSetFeatures& features,
    PositionType position,
    CSSSelector::PseudoType pseudo_type) {
  // Given a rule, update the descendant invalidation sets for the features
  // found in its selector. The first step is to extract the features from the
  // rightmost compound selector (ExtractInvalidationSetFeaturesFromCompound).
  // Secondly, add those features to the invalidation sets for the features
  // found in the other compound selectors (addFeaturesToInvalidationSets). If
  // we find a feature in the right-most compound selector that requires a
  // subtree recalc, nextCompound will be the rightmost compound and we will
  // addFeaturesToInvalidationSets for that one as well.

  InvalidationSetFeatures* sibling_features = nullptr;

  const CSSSelector* last_in_compound =
      ExtractInvalidationSetFeaturesFromCompound(complex, features, position);

  bool was_whole_subtree_invalid =
      features.invalidation_flags.WholeSubtreeInvalid();

  if (features.invalidation_flags.WholeSubtreeInvalid())
    features.has_features_for_rule_set_invalidation = false;
  else if (!features.HasFeatures())
    features.invalidation_flags.SetWholeSubtreeInvalid(true);
  // Only check for has_nth_pseudo if this is the top-level complex selector.
  if (pseudo_type == CSSSelector::kPseudoUnknown && features.has_nth_pseudo) {
    // The rightmost compound contains an :nth-* selector.
    // Add the compound features to the NthSiblingInvalidationSet. That is, for
    // '#id:nth-child(even)', add #id to the invalidation set and make sure we
    // invalidate elements matching those features (SetInvalidateSelf()).
    NthSiblingInvalidationSet& nth_set = EnsureNthInvalidationSet();
    AddFeaturesToInvalidationSet(nth_set, features);
    nth_set.SetInvalidatesSelf();
  }

  const CSSSelector* next_compound =
      last_in_compound ? last_in_compound->TagHistory() : &complex;
  if (!next_compound)
    return kNormalInvalidation;
  if (last_in_compound) {
    UpdateFeaturesFromCombinator(*last_in_compound, nullptr, features,
                                 sibling_features, features);
  }

  AddFeaturesToInvalidationSets(*next_compound, sibling_features, features);

  // We need to differentiate between no features (HasFeatures()==false)
  // and RequiresSubtreeInvalidation at the callsite. Hence we reset the flag
  // before returning, otherwise the distinction would be lost.
  features.invalidation_flags.SetWholeSubtreeInvalid(was_whole_subtree_invalid);
  return last_in_compound ? kNormalInvalidation : kRequiresSubtreeInvalidation;
}

void RuleFeatureSet::UpdateRuleSetInvalidation(
    const InvalidationSetFeatures& features) {
  if (features.has_features_for_rule_set_invalidation)
    return;
  if (features.invalidation_flags.WholeSubtreeInvalid() ||
      (!features.invalidation_flags.InvalidateCustomPseudo() &&
       features.tag_names.IsEmpty())) {
    metadata_.needs_full_recalc_for_rule_set_invalidation = true;
    return;
  }

  EnsureTypeRuleInvalidationSet();

  if (features.invalidation_flags.InvalidateCustomPseudo()) {
    type_rule_invalidation_set_->SetCustomPseudoInvalid();
    type_rule_invalidation_set_->SetTreeBoundaryCrossing();
  }

  for (auto tag_name : features.tag_names)
    type_rule_invalidation_set_->AddTagName(tag_name);
}

void RuleFeatureSet::ExtractInvalidationSetFeaturesFromSelectorList(
    const CSSSelector& simple_selector,
    InvalidationSetFeatures& features,
    PositionType position) {
  AutoRestoreMaxDirectAdjacentSelectors restore_max(&features);

  const CSSSelectorList* selector_list = simple_selector.SelectorList();
  if (!selector_list)
    return;
  CSSSelector::PseudoType pseudo_type = simple_selector.GetPseudoType();

  // For the :has pseudo class, we should not extract invalidation set features
  // here because the :has invalidation direction is different with others.
  // (preceding-sibling/ancestors/preceding-sibling-of-ancestors)
  if (UNLIKELY(pseudo_type == CSSSelector::kPseudoHas))
    return;

  DCHECK(SupportsInvalidationWithSelectorList(pseudo_type));

  const CSSSelector* sub_selector = selector_list->First();

  bool all_sub_selectors_have_features = true;
  bool all_sub_selectors_have_features_for_ruleset_invalidation = true;
  InvalidationSetFeatures any_features;

  for (; sub_selector; sub_selector = CSSSelectorList::Next(*sub_selector)) {
    InvalidationSetFeatures complex_features;
    if (UpdateInvalidationSetsForComplex(*sub_selector, complex_features,
                                         position, pseudo_type) ==
        kRequiresSubtreeInvalidation) {
      features.invalidation_flags.SetWholeSubtreeInvalid(true);
      continue;
    }
    all_sub_selectors_have_features_for_ruleset_invalidation &=
        complex_features.has_features_for_rule_set_invalidation;
    if (complex_features.has_nth_pseudo)
      features.has_nth_pseudo = true;
    if (!all_sub_selectors_have_features)
      continue;
    if (complex_features.HasFeatures())
      any_features.Add(complex_features);
    else
      all_sub_selectors_have_features = false;
  }
  // Don't add any features if one of the sub-selectors of does not contain
  // any invalidation set features. E.g. :-webkit-any(*, span).
  //
  // For the :not() pseudo class, we should not use the inner features for
  // invalidation because we should invalidate elements _without_ that
  // feature. On the other hand, we should still have invalidation sets
  // for the features since we are able to detect when they change.
  // That is, ".a" should not have ".b" in its invalidation set for
  // ".a :not(.b)", but there should be an invalidation set for ".a" in
  // ":not(.a) .b".
  if (pseudo_type != CSSSelector::kPseudoNot) {
    if (all_sub_selectors_have_features)
      features.NarrowToFeatures(any_features);
    features.has_features_for_rule_set_invalidation |=
        all_sub_selectors_have_features_for_ruleset_invalidation;
  }
}

const CSSSelector* RuleFeatureSet::ExtractInvalidationSetFeaturesFromCompound(
    const CSSSelector& compound,
    InvalidationSetFeatures& features,
    PositionType position) {
  // Extract invalidation set features and return a pointer to the the last
  // simple selector of the compound, or nullptr if one of the selectors
  // requiresSubtreeInvalidation().

  const CSSSelector* simple_selector = &compound;
  for (;; simple_selector = simple_selector->TagHistory()) {
    // Fall back to use subtree invalidations, even for features in the
    // rightmost compound selector. Returning nullptr here will make
    // addFeaturesToInvalidationSets start marking invalidation sets for
    // subtree recalc for features in the rightmost compound selector.
    if (RequiresSubtreeInvalidation(*simple_selector)) {
      features.invalidation_flags.SetWholeSubtreeInvalid(true);
      return nullptr;
    }

    ExtractInvalidationSetFeaturesFromSimpleSelector(*simple_selector,
                                                     features);

    // Initialize the entry in the invalidation set map for self-
    // invalidation, if supported.
    if (InvalidationSet* invalidation_set = InvalidationSetForSimpleSelector(
            *simple_selector, InvalidationType::kInvalidateDescendants,
            position)) {
      if (invalidation_set == nth_invalidation_set_)
        features.has_nth_pseudo = true;
      else if (position == kSubject)
        invalidation_set->SetInvalidatesSelf();
    }

    ExtractInvalidationSetFeaturesFromSelectorList(*simple_selector, features,
                                                   position);

    if (features.invalidation_flags.InvalidatesParts())
      metadata_.invalidates_parts = true;

    if (simple_selector->GetPseudoType() == CSSSelector::kPseudoHas)
      CollectValuesInHasArgument(*simple_selector);

    if (!simple_selector->TagHistory() ||
        simple_selector->Relation() != CSSSelector::kSubSelector) {
      return simple_selector;
    }
  }
}

void RuleFeatureSet::CollectValuesInHasArgument(
    const CSSSelector& has_pseudo_class) {
  DCHECK_EQ(has_pseudo_class.GetPseudoType(), CSSSelector::kPseudoHas);
  const CSSSelectorList* selector_list = has_pseudo_class.SelectorList();
  DCHECK(selector_list);

  for (const CSSSelector* relative_selector = selector_list->First();
       relative_selector;
       relative_selector = CSSSelectorList::Next(*relative_selector)) {
    DCHECK(relative_selector);

    bool value_added = false;
    const CSSSelector* simple = relative_selector;
    while (simple->GetPseudoType() != CSSSelector::kPseudoRelativeLeftmost) {
      value_added |= AddValueOfSimpleSelectorInHasArgument(*simple);

      if (simple->Relation() != CSSSelector::kSubSelector) {
        if (!value_added)
          universal_in_has_argument_ = true;
        value_added = false;
      }

      simple = simple->TagHistory();
      DCHECK(simple);
    }
  }
}

bool RuleFeatureSet::AddValueOfSimpleSelectorInHasArgument(
    const CSSSelector& selector) {
  if (selector.Match() == CSSSelector::kClass) {
    classes_in_has_argument_.insert(selector.Value());
    return true;
  }
  if (selector.IsAttributeSelector()) {
    attributes_in_has_argument_.insert(selector.Attribute().LocalName());
    return true;
  }
  if (selector.Match() == CSSSelector::kId) {
    ids_in_has_argument_.insert(selector.Value());
    return true;
  }
  if (selector.Match() == CSSSelector::kTag &&
      selector.TagQName().LocalName() != CSSSelector::UniversalSelectorAtom()) {
    tag_names_in_has_argument_.insert(selector.TagQName().LocalName());
    return true;
  }
  if (selector.Match() == CSSSelector::kPseudoClass) {
    CSSSelector::PseudoType pseudo_type = selector.GetPseudoType();

    // Ignore :visited to prevent history leakage.
    if (pseudo_type != CSSSelector::kPseudoVisited)
      pseudos_in_has_argument_.insert(pseudo_type);
    return true;
  }
  return false;
}

// Add features extracted from the rightmost compound selector to descendant
// invalidation sets for features found in other compound selectors.
//
// We use descendant invalidation for descendants, sibling invalidation for
// siblings and their subtrees.
//
// As we encounter a descendant type of combinator, the features only need to be
// checked against descendants in the same subtree only. features.adjacent is
// set to false, and we start adding features to the descendant invalidation
// set.

void RuleFeatureSet::AddFeaturesToInvalidationSet(
    InvalidationSet& invalidation_set,
    const InvalidationSetFeatures& features) {
  if (features.invalidation_flags.TreeBoundaryCrossing())
    invalidation_set.SetTreeBoundaryCrossing();
  if (features.invalidation_flags.InsertionPointCrossing())
    invalidation_set.SetInsertionPointCrossing();
  if (features.invalidation_flags.InvalidatesSlotted())
    invalidation_set.SetInvalidatesSlotted();
  if (features.invalidation_flags.WholeSubtreeInvalid())
    invalidation_set.SetWholeSubtreeInvalid();
  if (features.invalidation_flags.InvalidatesParts())
    invalidation_set.SetInvalidatesParts();
  if (features.content_pseudo_crossing ||
      features.invalidation_flags.WholeSubtreeInvalid())
    return;

  for (const auto& id : features.ids)
    invalidation_set.AddId(id);
  for (const auto& tag_name : features.tag_names)
    invalidation_set.AddTagName(tag_name);
  for (const auto& emitted_tag_name : features.emitted_tag_names)
    invalidation_set.AddTagName(emitted_tag_name);
  for (const auto& class_name : features.classes)
    invalidation_set.AddClass(class_name);
  for (const auto& attribute : features.attributes)
    invalidation_set.AddAttribute(attribute);
  if (features.invalidation_flags.InvalidateCustomPseudo())
    invalidation_set.SetCustomPseudoInvalid();
}

void RuleFeatureSet::AddFeaturesToInvalidationSetsForSelectorList(
    const CSSSelector& simple_selector,
    InvalidationSetFeatures* sibling_features,
    InvalidationSetFeatures& descendant_features) {
  if (!simple_selector.SelectorList())
    return;

  DCHECK(SupportsInvalidationWithSelectorList(simple_selector.GetPseudoType()));

  bool had_features_for_rule_set_invalidation =
      descendant_features.has_features_for_rule_set_invalidation;
  bool selector_list_contains_universal =
      simple_selector.GetPseudoType() == CSSSelector::kPseudoNot ||
      simple_selector.GetPseudoType() == CSSSelector::kPseudoHostContext;

  for (const CSSSelector* sub_selector =
           simple_selector.SelectorList()->First();
       sub_selector; sub_selector = CSSSelectorList::Next(*sub_selector)) {
    AutoRestoreMaxDirectAdjacentSelectors restore_max(sibling_features);
    AutoRestoreTreeBoundaryCrossingFlag restore_tree_boundary(
        descendant_features);
    AutoRestoreInsertionPointCrossingFlag restore_insertion_point(
        descendant_features);

    if (simple_selector.IsHostPseudoClass())
      descendant_features.invalidation_flags.SetTreeBoundaryCrossing(true);

    descendant_features.has_features_for_rule_set_invalidation = false;

    AddFeaturesToInvalidationSets(*sub_selector, sibling_features,
                                  descendant_features);

    if (!descendant_features.has_features_for_rule_set_invalidation)
      selector_list_contains_universal = true;
  }

  descendant_features.has_features_for_rule_set_invalidation =
      had_features_for_rule_set_invalidation ||
      !selector_list_contains_universal;
}

void RuleFeatureSet::AddFeaturesToInvalidationSetsForSimpleSelector(
    const CSSSelector& simple_selector,
    InvalidationSetFeatures* sibling_features,
    InvalidationSetFeatures& descendant_features) {
  if (simple_selector.IsIdClassOrAttributeSelector())
    descendant_features.has_features_for_rule_set_invalidation = true;

  CSSSelector::PseudoType pseudo_type = simple_selector.GetPseudoType();

  if (UNLIKELY(pseudo_type == CSSSelector::kPseudoHas))
    CollectValuesInHasArgument(simple_selector);

  if (InvalidationSet* invalidation_set = InvalidationSetForSimpleSelector(
          simple_selector,
          sibling_features ? InvalidationType::kInvalidateSiblings
                           : InvalidationType::kInvalidateDescendants,
          kAncestor)) {
    if (!sibling_features) {
      if (invalidation_set == nth_invalidation_set_) {
        // TODO(futhark): We can extract the features from the current compound
        // to optimize this.
        invalidation_set->SetWholeSubtreeInvalid();
        AddFeaturesToInvalidationSet(
            To<SiblingInvalidationSet>(invalidation_set)
                ->EnsureSiblingDescendants(),
            descendant_features);
        return;
      } else {
        AddFeaturesToInvalidationSet(*invalidation_set, descendant_features);
        return;
      }
    }

    auto* sibling_invalidation_set =
        To<SiblingInvalidationSet>(invalidation_set);
    sibling_invalidation_set->UpdateMaxDirectAdjacentSelectors(
        sibling_features->max_direct_adjacent_selectors);
    AddFeaturesToInvalidationSet(*invalidation_set, *sibling_features);
    if (sibling_features == &descendant_features) {
      sibling_invalidation_set->SetInvalidatesSelf();
    } else {
      AddFeaturesToInvalidationSet(
          sibling_invalidation_set->EnsureSiblingDescendants(),
          descendant_features);
    }
    return;
  }

  // For the :has pseudo class, we should not extract invalidation set features
  // here because the :has invalidation direction is different with others.
  // (preceding-sibling/ancestors/preceding-sibling-of-ancestors)
  if (UNLIKELY(pseudo_type == CSSSelector::kPseudoHas))
    return;

  if (pseudo_type == CSSSelector::kPseudoPart)
    descendant_features.invalidation_flags.SetInvalidatesParts(true);

  AddFeaturesToInvalidationSetsForSelectorList(
      simple_selector, sibling_features, descendant_features);
}

const CSSSelector*
RuleFeatureSet::AddFeaturesToInvalidationSetsForCompoundSelector(
    const CSSSelector& compound,
    InvalidationSetFeatures* sibling_features,
    InvalidationSetFeatures& descendant_features) {
  bool compound_has_features_for_rule_set_invalidation = false;
  const CSSSelector* simple_selector = &compound;
  for (; simple_selector; simple_selector = simple_selector->TagHistory()) {
    base::AutoReset<bool> reset_has_features(
        &descendant_features.has_features_for_rule_set_invalidation, false);
    AddFeaturesToInvalidationSetsForSimpleSelector(
        *simple_selector, sibling_features, descendant_features);
    if (descendant_features.has_features_for_rule_set_invalidation)
      compound_has_features_for_rule_set_invalidation = true;
    if (simple_selector->Relation() != CSSSelector::kSubSelector)
      break;
    if (!simple_selector->TagHistory())
      break;
  }

  if (compound_has_features_for_rule_set_invalidation) {
    descendant_features.has_features_for_rule_set_invalidation = true;
  } else if (sibling_features) {
    AddFeaturesToUniversalSiblingInvalidationSet(*sibling_features,
                                                 descendant_features);
  }

  return simple_selector;
}

void RuleFeatureSet::AddFeaturesToInvalidationSets(
    const CSSSelector& selector,
    InvalidationSetFeatures* sibling_features,
    InvalidationSetFeatures& descendant_features) {
  // selector is the selector immediately to the left of the rightmost
  // combinator. descendantFeatures has the features of the rightmost compound
  // selector.

  InvalidationSetFeatures last_compound_in_sibling_chain_features;
  const CSSSelector* compound = &selector;
  while (compound) {
    const CSSSelector* last_in_compound =
        AddFeaturesToInvalidationSetsForCompoundSelector(
            *compound, sibling_features, descendant_features);
    DCHECK(last_in_compound);
    UpdateFeaturesFromCombinator(*last_in_compound, compound,
                                 last_compound_in_sibling_chain_features,
                                 sibling_features, descendant_features);
    compound = last_in_compound->TagHistory();
  }
}

RuleFeatureSet::SelectorPreMatch RuleFeatureSet::CollectFeaturesFromRuleData(
    const RuleData* rule_data) {
  CHECK(is_alive_);
  FeatureMetadata metadata;
  const unsigned max_direct_adjacent_selectors = 0;
  if (CollectFeaturesFromSelector(rule_data->Selector(), metadata,
                                  max_direct_adjacent_selectors) ==
      kSelectorNeverMatches) {
    return kSelectorNeverMatches;
  }
  metadata.uses_container_queries |=
      static_cast<bool>(rule_data->GetContainerQuery());

  metadata_.Add(metadata);

  UpdateInvalidationSets(rule_data);
  return kSelectorMayMatch;
}

RuleFeatureSet::SelectorPreMatch RuleFeatureSet::CollectFeaturesFromSelector(
    const CSSSelector& selector,
    RuleFeatureSet::FeatureMetadata& metadata,
    unsigned max_direct_adjacent_selectors) {
  CSSSelector::RelationType relation = CSSSelector::kDescendant;
  bool found_host_pseudo = false;

  for (const CSSSelector* current = &selector; current;
       current = current->TagHistory()) {
    switch (current->GetPseudoType()) {
      case CSSSelector::kPseudoHas:
        break;
      case CSSSelector::kPseudoFirstLine:
        metadata.uses_first_line_rules = true;
        break;
      case CSSSelector::kPseudoWindowInactive:
        metadata.uses_window_inactive_selector = true;
        break;
      case CSSSelector::kPseudoHost:
      case CSSSelector::kPseudoHostContext:
        if (!found_host_pseudo && relation == CSSSelector::kSubSelector)
          return kSelectorNeverMatches;
        if (!current->IsLastInTagHistory() &&
            current->TagHistory()->Match() != CSSSelector::kPseudoElement &&
            !current->TagHistory()->IsHostPseudoClass()) {
          return kSelectorNeverMatches;
        }
        found_host_pseudo = true;
        [[fallthrough]];
      default:
        if (const CSSSelectorList* selector_list = current->SelectorList()) {
          for (const CSSSelector* sub_selector = selector_list->First();
               sub_selector;
               sub_selector = CSSSelectorList::Next(*sub_selector)) {
            CollectFeaturesFromSelector(*sub_selector, metadata,
                                        max_direct_adjacent_selectors);
          }
        }
        break;
    }

    relation = current->Relation();

    if (found_host_pseudo && relation != CSSSelector::kSubSelector)
      return kSelectorNeverMatches;

    if (relation == CSSSelector::kDirectAdjacent) {
      max_direct_adjacent_selectors++;
    } else if (max_direct_adjacent_selectors &&
               ((relation != CSSSelector::kSubSelector) ||
                current->IsLastInTagHistory())) {
      if (max_direct_adjacent_selectors >
          metadata.max_direct_adjacent_selectors)
        metadata.max_direct_adjacent_selectors = max_direct_adjacent_selectors;
      max_direct_adjacent_selectors = 0;
    }
  }

  DCHECK(!max_direct_adjacent_selectors);
  return kSelectorMayMatch;
}

void RuleFeatureSet::FeatureMetadata::Add(const FeatureMetadata& other) {
  uses_first_line_rules |= other.uses_first_line_rules;
  uses_window_inactive_selector |= other.uses_window_inactive_selector;
  uses_container_queries |= other.uses_container_queries;
  max_direct_adjacent_selectors = std::max(max_direct_adjacent_selectors,
                                           other.max_direct_adjacent_selectors);
}

void RuleFeatureSet::FeatureMetadata::Clear() {
  uses_first_line_rules = false;
  uses_window_inactive_selector = false;
  uses_container_queries = false;
  needs_full_recalc_for_rule_set_invalidation = false;
  max_direct_adjacent_selectors = 0;
  invalidates_parts = false;
}

bool RuleFeatureSet::FeatureMetadata::operator==(
    const FeatureMetadata& other) const {
  return uses_first_line_rules == other.uses_first_line_rules &&
         uses_window_inactive_selector == other.uses_window_inactive_selector &&
         uses_container_queries == other.uses_container_queries &&
         needs_full_recalc_for_rule_set_invalidation ==
             other.needs_full_recalc_for_rule_set_invalidation &&
         max_direct_adjacent_selectors == other.max_direct_adjacent_selectors &&
         invalidates_parts == other.invalidates_parts;
}

void RuleFeatureSet::Add(const RuleFeatureSet& other) {
  CHECK(is_alive_);
  CHECK(other.is_alive_);
  CHECK_NE(&other, this);
  for (const auto& entry : other.class_invalidation_sets_)
    AddInvalidationSet(class_invalidation_sets_, entry.key, entry.value);
  for (const auto& entry : other.attribute_invalidation_sets_)
    AddInvalidationSet(attribute_invalidation_sets_, entry.key, entry.value);
  for (const auto& entry : other.id_invalidation_sets_)
    AddInvalidationSet(id_invalidation_sets_, entry.key, entry.value);
  for (const auto& entry : other.pseudo_invalidation_sets_) {
    auto key = static_cast<CSSSelector::PseudoType>(entry.key);
    AddInvalidationSet(pseudo_invalidation_sets_, key, entry.value);
  }
  if (other.universal_sibling_invalidation_set_) {
    EnsureUniversalSiblingInvalidationSet().Combine(
        *other.universal_sibling_invalidation_set_);
  }
  if (other.nth_invalidation_set_)
    EnsureNthInvalidationSet().Combine(*other.nth_invalidation_set_);
  if (other.metadata_.invalidates_parts)
    metadata_.invalidates_parts = true;

  metadata_.Add(other.metadata_);

  viewport_dependent_media_query_results_.AppendVector(
      other.viewport_dependent_media_query_results_);
  device_dependent_media_query_results_.AppendVector(
      other.device_dependent_media_query_results_);

  for (const auto& class_name : other.classes_in_has_argument_)
    classes_in_has_argument_.insert(class_name);
  for (const auto& attribute_name : other.attributes_in_has_argument_)
    attributes_in_has_argument_.insert(attribute_name);
  for (const auto& id : other.ids_in_has_argument_)
    ids_in_has_argument_.insert(id);
  for (const auto& tag_name : other.tag_names_in_has_argument_)
    tag_names_in_has_argument_.insert(tag_name);
  universal_in_has_argument_ |= other.universal_in_has_argument_;
  for (const auto& pseudo_type : other.pseudos_in_has_argument_)
    pseudos_in_has_argument_.insert(pseudo_type);
}

void RuleFeatureSet::Clear() {
  CHECK(is_alive_);
  metadata_.Clear();
  class_invalidation_sets_.clear();
  attribute_invalidation_sets_.clear();
  id_invalidation_sets_.clear();
  pseudo_invalidation_sets_.clear();
  universal_sibling_invalidation_set_ = nullptr;
  nth_invalidation_set_ = nullptr;
  type_rule_invalidation_set_ = nullptr;
  viewport_dependent_media_query_results_.clear();
  device_dependent_media_query_results_.clear();
  media_query_unit_flags_ = 0;
  classes_in_has_argument_.clear();
  attributes_in_has_argument_.clear();
  ids_in_has_argument_.clear();
  tag_names_in_has_argument_.clear();
  universal_in_has_argument_ = false;
  pseudos_in_has_argument_.clear();
}

bool RuleFeatureSet::HasDynamicViewportDependentMediaQueries() const {
  return media_query_unit_flags_ &
         MediaQueryExpValue::UnitFlags::kDynamicViewport;
}

void RuleFeatureSet::CollectInvalidationSetsForClass(
    InvalidationLists& invalidation_lists,
    Element& element,
    const AtomicString& class_name) const {
  InvalidationSetMap::const_iterator it =
      class_invalidation_sets_.find(class_name);
  if (it == class_invalidation_sets_.end())
    return;

  DescendantInvalidationSet* descendants;
  SiblingInvalidationSet* siblings;
  ExtractInvalidationSets(it->value.get(), descendants, siblings);

  if (descendants) {
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *descendants, ClassChange,
                                      class_name);
    invalidation_lists.descendants.push_back(descendants);
  }

  if (siblings) {
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblings, ClassChange,
                                      class_name);
    invalidation_lists.siblings.push_back(siblings);
  }
}

void RuleFeatureSet::CollectSiblingInvalidationSetForClass(
    InvalidationLists& invalidation_lists,
    Element& element,
    const AtomicString& class_name,
    unsigned min_direct_adjacent) const {
  InvalidationSetMap::const_iterator it =
      class_invalidation_sets_.find(class_name);
  if (it == class_invalidation_sets_.end())
    return;

  auto* sibling_set = DynamicTo<SiblingInvalidationSet>(it->value.get());
  if (!sibling_set)
    return;

  if (sibling_set->MaxDirectAdjacentSelectors() < min_direct_adjacent)
    return;

  TRACE_SCHEDULE_STYLE_INVALIDATION(element, *sibling_set, ClassChange,
                                    class_name);
  invalidation_lists.siblings.push_back(sibling_set);
}

void RuleFeatureSet::CollectInvalidationSetsForId(
    InvalidationLists& invalidation_lists,
    Element& element,
    const AtomicString& id) const {
  InvalidationSetMap::const_iterator it = id_invalidation_sets_.find(id);
  if (it == id_invalidation_sets_.end())
    return;

  DescendantInvalidationSet* descendants;
  SiblingInvalidationSet* siblings;
  ExtractInvalidationSets(it->value.get(), descendants, siblings);

  if (descendants) {
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *descendants, IdChange, id);
    invalidation_lists.descendants.push_back(descendants);
  }

  if (siblings) {
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblings, IdChange, id);
    invalidation_lists.siblings.push_back(siblings);
  }
}

void RuleFeatureSet::CollectSiblingInvalidationSetForId(
    InvalidationLists& invalidation_lists,
    Element& element,
    const AtomicString& id,
    unsigned min_direct_adjacent) const {
  InvalidationSetMap::const_iterator it = id_invalidation_sets_.find(id);
  if (it == id_invalidation_sets_.end())
    return;

  auto* sibling_set = DynamicTo<SiblingInvalidationSet>(it->value.get());
  if (!sibling_set)
    return;

  if (sibling_set->MaxDirectAdjacentSelectors() < min_direct_adjacent)
    return;

  TRACE_SCHEDULE_STYLE_INVALIDATION(element, *sibling_set, IdChange, id);
  invalidation_lists.siblings.push_back(sibling_set);
}

void RuleFeatureSet::CollectInvalidationSetsForAttribute(
    InvalidationLists& invalidation_lists,
    Element& element,
    const QualifiedName& attribute_name) const {
  InvalidationSetMap::const_iterator it =
      attribute_invalidation_sets_.find(attribute_name.LocalName());
  if (it == attribute_invalidation_sets_.end())
    return;

  DescendantInvalidationSet* descendants;
  SiblingInvalidationSet* siblings;
  ExtractInvalidationSets(it->value.get(), descendants, siblings);

  if (descendants) {
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *descendants, AttributeChange,
                                      attribute_name);
    invalidation_lists.descendants.push_back(descendants);
  }

  if (siblings) {
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblings, AttributeChange,
                                      attribute_name);
    invalidation_lists.siblings.push_back(siblings);
  }
}

void RuleFeatureSet::CollectSiblingInvalidationSetForAttribute(
    InvalidationLists& invalidation_lists,
    Element& element,
    const QualifiedName& attribute_name,
    unsigned min_direct_adjacent) const {
  InvalidationSetMap::const_iterator it =
      attribute_invalidation_sets_.find(attribute_name.LocalName());
  if (it == attribute_invalidation_sets_.end())
    return;

  auto* sibling_set = DynamicTo<SiblingInvalidationSet>(it->value.get());
  if (!sibling_set)
    return;

  if (sibling_set->MaxDirectAdjacentSelectors() < min_direct_adjacent)
    return;

  TRACE_SCHEDULE_STYLE_INVALIDATION(element, *sibling_set, AttributeChange,
                                    attribute_name);
  invalidation_lists.siblings.push_back(sibling_set);
}

void RuleFeatureSet::CollectInvalidationSetsForPseudoClass(
    InvalidationLists& invalidation_lists,
    Element& element,
    CSSSelector::PseudoType pseudo) const {
  PseudoTypeInvalidationSetMap::const_iterator it =
      pseudo_invalidation_sets_.find(pseudo);
  if (it == pseudo_invalidation_sets_.end())
    return;

  DescendantInvalidationSet* descendants;
  SiblingInvalidationSet* siblings;
  ExtractInvalidationSets(it->value.get(), descendants, siblings);

  if (descendants) {
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *descendants, PseudoChange,
                                      pseudo);
    invalidation_lists.descendants.push_back(descendants);
  }

  if (siblings) {
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblings, PseudoChange, pseudo);
    invalidation_lists.siblings.push_back(siblings);
  }
}

void RuleFeatureSet::CollectUniversalSiblingInvalidationSet(
    InvalidationLists& invalidation_lists,
    unsigned min_direct_adjacent) const {
  if (universal_sibling_invalidation_set_ &&
      universal_sibling_invalidation_set_->MaxDirectAdjacentSelectors() >=
          min_direct_adjacent)
    invalidation_lists.siblings.push_back(universal_sibling_invalidation_set_);
}

SiblingInvalidationSet&
RuleFeatureSet::EnsureUniversalSiblingInvalidationSet() {
  if (!universal_sibling_invalidation_set_) {
    universal_sibling_invalidation_set_ =
        SiblingInvalidationSet::Create(nullptr);
  }
  return *universal_sibling_invalidation_set_;
}

void RuleFeatureSet::CollectNthInvalidationSet(
    InvalidationLists& invalidation_lists) const {
  if (nth_invalidation_set_)
    invalidation_lists.siblings.push_back(nth_invalidation_set_);
}

NthSiblingInvalidationSet& RuleFeatureSet::EnsureNthInvalidationSet() {
  if (!nth_invalidation_set_)
    nth_invalidation_set_ = NthSiblingInvalidationSet::Create();
  return *nth_invalidation_set_;
}

void RuleFeatureSet::CollectPartInvalidationSet(
    InvalidationLists& invalidation_lists) const {
  if (metadata_.invalidates_parts) {
    invalidation_lists.descendants.push_back(
        InvalidationSet::PartInvalidationSet());
  }
}

void RuleFeatureSet::CollectTypeRuleInvalidationSet(
    InvalidationLists& invalidation_lists,
    ContainerNode& root_node) const {
  if (type_rule_invalidation_set_) {
    invalidation_lists.descendants.push_back(type_rule_invalidation_set_);
    TRACE_SCHEDULE_STYLE_INVALIDATION(root_node, *type_rule_invalidation_set_,
                                      RuleSetInvalidation);
  }
}

DescendantInvalidationSet& RuleFeatureSet::EnsureTypeRuleInvalidationSet() {
  if (!type_rule_invalidation_set_)
    type_rule_invalidation_set_ = DescendantInvalidationSet::Create();
  return *type_rule_invalidation_set_;
}

void RuleFeatureSet::AddFeaturesToUniversalSiblingInvalidationSet(
    const InvalidationSetFeatures& sibling_features,
    const InvalidationSetFeatures& descendant_features) {
  SiblingInvalidationSet& universal_set =
      EnsureUniversalSiblingInvalidationSet();
  AddFeaturesToInvalidationSet(universal_set, sibling_features);
  universal_set.UpdateMaxDirectAdjacentSelectors(
      sibling_features.max_direct_adjacent_selectors);

  if (&sibling_features == &descendant_features) {
    universal_set.SetInvalidatesSelf();
  } else {
    AddFeaturesToInvalidationSet(universal_set.EnsureSiblingDescendants(),
                                 descendant_features);
  }
}

bool RuleFeatureSet::NeedsHasInvalidationForClass(
    const AtomicString& class_name) const {
  return classes_in_has_argument_.Contains(class_name);
}

bool RuleFeatureSet::NeedsHasInvalidationForAttribute(
    const QualifiedName& attribute_name) const {
  return attributes_in_has_argument_.Contains(attribute_name.LocalName());
}

bool RuleFeatureSet::NeedsHasInvalidationForId(const AtomicString& id) const {
  return ids_in_has_argument_.Contains(id);
}

bool RuleFeatureSet::NeedsHasInvalidationForTagName(
    const AtomicString& tag_name) const {
  return universal_in_has_argument_ ||
         tag_names_in_has_argument_.Contains(tag_name);
}

bool RuleFeatureSet::NeedsHasInvalidationForElement(Element& element) const {
  if (element.HasID()) {
    if (NeedsHasInvalidationForId(element.IdForStyleResolution()))
      return true;
  }

  if (element.HasClass()) {
    const SpaceSplitString& class_names = element.ClassNames();
    for (wtf_size_t i = 0; i < class_names.size(); i++) {
      if (NeedsHasInvalidationForClass(class_names[i]))
        return true;
    }
  }

  for (const Attribute& attribute : element.Attributes()) {
    if (NeedsHasInvalidationForAttribute(attribute.GetName()))
      return true;
  }

  return NeedsHasInvalidationForTagName(element.LocalNameForSelectorMatching());
}

bool RuleFeatureSet::NeedsHasInvalidationForPseudoClass(
    CSSSelector::PseudoType pseudo_type) const {
  return pseudos_in_has_argument_.Contains(pseudo_type);
}

bool RuleFeatureSet::NeedsHasInvalidationForPseudoStateChange() const {
  return !pseudos_in_has_argument_.IsEmpty();
}

void RuleFeatureSet::InvalidationSetFeatures::Add(
    const InvalidationSetFeatures& other) {
  classes.AppendVector(other.classes);
  attributes.AppendVector(other.attributes);
  ids.AppendVector(other.ids);
  // Tag names that have been added to an invalidation set for an ID, a class,
  // or an attribute are called "emitted" tag names. Emitted tag names need to
  // go in a separate vector in order to correctly track which tag names to
  // add to the type rule invalidation set.
  //
  // Example: :is(.a, div) :is(span, .b, ol, .c li)
  //
  // For the above selector, we need span and ol in the type invalidation set,
  // but not li, since that tag name was added to the invalidation set for .c.
  // Hence, when processing the rightmost :is(), we end up with li in the
  // emitted_tag_names vector, and span and ol in the regular tag_names vector.
  if (other.has_features_for_rule_set_invalidation)
    emitted_tag_names.AppendVector(other.tag_names);
  else
    tag_names.AppendVector(other.tag_names);
  emitted_tag_names.AppendVector(other.emitted_tag_names);
  max_direct_adjacent_selectors = std::max(max_direct_adjacent_selectors,
                                           other.max_direct_adjacent_selectors);
  invalidation_flags.Merge(other.invalidation_flags);
  content_pseudo_crossing |= other.content_pseudo_crossing;
  has_nth_pseudo |= other.has_nth_pseudo;
}

void RuleFeatureSet::InvalidationSetFeatures::NarrowToFeatures(
    const InvalidationSetFeatures& other) {
  unsigned size = Size();
  unsigned other_size = other.Size();
  if (size == 0 || (1 <= other_size && other_size < size)) {
    ClearFeatures();
    Add(other);
  }
}

bool RuleFeatureSet::InvalidationSetFeatures::HasFeatures() const {
  return !classes.IsEmpty() || !attributes.IsEmpty() || !ids.IsEmpty() ||
         !tag_names.IsEmpty() || !emitted_tag_names.IsEmpty() ||
         invalidation_flags.InvalidateCustomPseudo() ||
         invalidation_flags.InvalidatesParts();
}

bool RuleFeatureSet::InvalidationSetFeatures::HasIdClassOrAttribute() const {
  return !classes.IsEmpty() || !attributes.IsEmpty() || !ids.IsEmpty();
}

String RuleFeatureSet::ToString() const {
  StringBuilder builder;

  enum TypeFlags {
    kId = 1 << 0,
    kClass = 1 << 1,
    kAttribute = 1 << 2,
    kPseudo = 1 << 3,
    kDescendant = 1 << 4,
    kSibling = 1 << 5,
    kType = 1 << 6,
    kUniversal = 1 << 7,
    kNth = 1 << 8,
  };

  struct Entry {
    String name;
    const InvalidationSet* set;
    unsigned flags;
  };

  Vector<Entry> entries;

  auto add_invalidation_sets =
      [&entries](const String& base, InvalidationSet* set, unsigned flags,
                 const char* prefix = "", const char* suffix = "") {
        if (!set)
          return;
        DescendantInvalidationSet* descendants;
        SiblingInvalidationSet* siblings;
        ExtractInvalidationSets(set, descendants, siblings);

        if (descendants)
          entries.push_back(Entry{base, descendants, flags | kDescendant});
        if (siblings)
          entries.push_back(Entry{base, siblings, flags | kSibling});
        if (siblings && siblings->SiblingDescendants()) {
          entries.push_back(Entry{base, siblings->SiblingDescendants(),
                                  flags | kSibling | kDescendant});
        }
      };

  auto format_name = [](const String& base, unsigned flags) {
    StringBuilder builder;
    // Prefix:

    builder.Append((flags & kId) ? "#" : "");
    builder.Append((flags & kClass) ? "." : "");
    builder.Append((flags & kAttribute) ? "[" : "");

    builder.Append(base);

    // Suffix:
    builder.Append((flags & kAttribute) ? "]" : "");

    builder.Append("[");
    if (flags & kSibling)
      builder.Append("+");
    if (flags & kDescendant)
      builder.Append(">");
    builder.Append("]");

    return builder.ReleaseString();
  };

  auto format_max_direct_adjancent = [](unsigned max) -> String {
    if (max == SiblingInvalidationSet::kDirectAdjacentMax)
      return "~";
    if (max)
      return String::Number(max);
    return g_empty_atom;
  };

  for (auto& i : id_invalidation_sets_)
    add_invalidation_sets(i.key, i.value.get(), kId, "#");
  for (auto& i : class_invalidation_sets_)
    add_invalidation_sets(i.key, i.value.get(), kClass, ".");
  for (auto& i : attribute_invalidation_sets_)
    add_invalidation_sets(i.key, i.value.get(), kAttribute, "[", "]");
  for (auto& i : pseudo_invalidation_sets_) {
    String name = CSSSelector::FormatPseudoTypeForDebugging(
        static_cast<CSSSelector::PseudoType>(i.key));
    add_invalidation_sets(name, i.value.get(), kPseudo, ":", "");
  }

  add_invalidation_sets("type", type_rule_invalidation_set_.get(), kType);
  add_invalidation_sets("*", universal_sibling_invalidation_set_.get(),
                        kUniversal);
  add_invalidation_sets("nth", nth_invalidation_set_.get(), kNth);

  std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
    if (a.flags != b.flags)
      return a.flags < b.flags;
    return WTF::CodeUnitCompareLessThan(a.name, b.name);
  });

  for (const Entry& entry : entries) {
    builder.Append(format_name(entry.name, entry.flags));
    builder.Append(entry.set->ToString());
    builder.Append(" ");
  }

  StringBuilder metadata;
  metadata.Append(metadata_.uses_first_line_rules ? "F" : "");
  metadata.Append(metadata_.uses_window_inactive_selector ? "W" : "");
  metadata.Append(metadata_.uses_container_queries ? "C" : "");
  metadata.Append(metadata_.needs_full_recalc_for_rule_set_invalidation ? "R"
                                                                        : "");
  metadata.Append(metadata_.invalidates_parts ? "P" : "");
  metadata.Append(
      format_max_direct_adjancent(metadata_.max_direct_adjacent_selectors));

  if (!metadata.IsEmpty()) {
    builder.Append("META:");
    builder.Append(metadata.ReleaseString());
  }

  return builder.ReleaseString();
}

std::ostream& operator<<(std::ostream& ostream, const RuleFeatureSet& set) {
  return ostream << set.ToString().Utf8();
}

}  // namespace blink
