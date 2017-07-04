/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/css/resolver/SharedStyleFinder.h"

#include "core/HTMLNames.h"
#include "core/XMLNames.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleResolverStats.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/ElementShadow.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/Node.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/SpaceSplitString.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/V0InsertionPoint.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLOptGroupElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/style/ComputedStyle.h"
#include "core/svg/SVGElement.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

using namespace HTMLNames;

inline ComputedStyle* GetElementStyle(Element& element) {
  if (element.NeedsReattachLayoutTree()) {
    if (ComputedStyle* computed_style = element.GetNonAttachedStyle())
      return computed_style;
  }
  return element.MutableComputedStyle();
}

bool SharedStyleFinder::CanShareStyleWithControl(Element& candidate) const {
  if (!isHTMLInputElement(candidate) || !isHTMLInputElement(GetElement()))
    return false;

  HTMLInputElement& candidate_input = toHTMLInputElement(candidate);
  HTMLInputElement& this_input = toHTMLInputElement(GetElement());

  if (candidate_input.IsAutofilled() != this_input.IsAutofilled())
    return false;
  if (candidate_input.ShouldAppearChecked() != this_input.ShouldAppearChecked())
    return false;
  if (candidate_input.ShouldAppearIndeterminate() !=
      this_input.ShouldAppearIndeterminate())
    return false;
  if (candidate_input.IsRequired() != this_input.IsRequired())
    return false;

  if (candidate.IsDisabledFormControl() != GetElement().IsDisabledFormControl())
    return false;

  if (candidate.MatchesDefaultPseudoClass() !=
      GetElement().MatchesDefaultPseudoClass())
    return false;

  if (GetDocument().ContainsValidityStyleRules()) {
    bool will_validate = candidate.willValidate();

    if (will_validate != GetElement().willValidate())
      return false;

    if (will_validate &&
        (candidate.IsValidElement() != GetElement().IsValidElement()))
      return false;

    if (candidate.IsInRange() != GetElement().IsInRange())
      return false;

    if (candidate.IsOutOfRange() != GetElement().IsOutOfRange())
      return false;
  }

  if (candidate_input.IsPlaceholderVisible() !=
      this_input.IsPlaceholderVisible())
    return false;

  return true;
}

bool SharedStyleFinder::ClassNamesAffectedByRules(
    const SpaceSplitString& class_names) const {
  unsigned count = class_names.size();
  for (unsigned i = 0; i < count; ++i) {
    if (features_.HasSelectorForClass(class_names[i]))
      return true;
  }
  return false;
}

static inline const AtomicString& TypeAttributeValue(const Element& element) {
  // type is animatable in SVG so we need to go down the slow path here.
  return element.IsSVGElement() ? element.getAttribute(typeAttr)
                                : element.FastGetAttribute(typeAttr);
}

bool SharedStyleFinder::SharingCandidateHasIdenticalStyleAffectingAttributes(
    Element& candidate) const {
  if (GetElement().SharesSameElementData(candidate))
    return true;
  if (GetElement().FastGetAttribute(XMLNames::langAttr) !=
      candidate.FastGetAttribute(XMLNames::langAttr))
    return false;
  if (GetElement().FastGetAttribute(langAttr) !=
      candidate.FastGetAttribute(langAttr))
    return false;

  // These two checks must be here since RuleSet has a special case to allow
  // style sharing between elements with type and readonly attributes whereas
  // other attribute selectors prevent sharing.
  if (TypeAttributeValue(GetElement()) != TypeAttributeValue(candidate))
    return false;
  if (GetElement().FastGetAttribute(readonlyAttr) !=
      candidate.FastGetAttribute(readonlyAttr))
    return false;

  if (!element_affected_by_class_rules_) {
    if (candidate.HasClass() &&
        ClassNamesAffectedByRules(candidate.ClassNames()))
      return false;
  } else if (candidate.HasClass()) {
    // SVG elements require a (slow!) getAttribute comparision because "class"
    // is an animatable attribute for SVG.
    if (GetElement().IsSVGElement()) {
      if (GetElement().getAttribute(classAttr) !=
          candidate.getAttribute(classAttr))
        return false;
    } else if (GetElement().ClassNames() != candidate.ClassNames()) {
      return false;
    }
  } else {
    return false;
  }

  if (GetElement().PresentationAttributeStyle() !=
      candidate.PresentationAttributeStyle())
    return false;

  // FIXME: Consider removing this, it's unlikely we'll have so many progress
  // elements that sharing the style makes sense. Instead we should just not
  // support style sharing for them.
  if (isHTMLProgressElement(GetElement())) {
    if (GetElement().ShouldAppearIndeterminate() !=
        candidate.ShouldAppearIndeterminate())
      return false;
  }

  if (isHTMLOptGroupElement(GetElement()) ||
      isHTMLOptionElement(GetElement())) {
    if (GetElement().IsDisabledFormControl() !=
        candidate.IsDisabledFormControl())
      return false;
    if (isHTMLOptionElement(GetElement()) &&
        toHTMLOptionElement(GetElement()).Selected() !=
            toHTMLOptionElement(candidate).Selected())
      return false;
  }

  return true;
}

bool SharedStyleFinder::SharingCandidateCanShareHostStyles(
    Element& candidate) const {
  const ElementShadow* element_shadow = GetElement().Shadow();
  const ElementShadow* candidate_shadow = candidate.Shadow();

  if (!element_shadow && !candidate_shadow)
    return true;

  if (static_cast<bool>(element_shadow) != static_cast<bool>(candidate_shadow))
    return false;

  return element_shadow->HasSameStyles(*candidate_shadow);
}

bool SharedStyleFinder::SharingCandidateAssignedToSameSlot(
    Element& candidate) const {
  HTMLSlotElement* element_slot = GetElement().AssignedSlot();
  HTMLSlotElement* candidate_slot = candidate.AssignedSlot();
  if (!element_slot && !candidate_slot)
    return true;
  return element_slot == candidate_slot;
}

bool SharedStyleFinder::SharingCandidateDistributedToSameInsertionPoint(
    Element& candidate) const {
  HeapVector<Member<V0InsertionPoint>, 8> insertion_points,
      candidate_insertion_points;
  CollectDestinationInsertionPoints(GetElement(), insertion_points);
  CollectDestinationInsertionPoints(candidate, candidate_insertion_points);
  if (insertion_points.size() != candidate_insertion_points.size())
    return false;
  for (size_t i = 0; i < insertion_points.size(); ++i) {
    if (insertion_points[i] != candidate_insertion_points[i])
      return false;
  }
  return true;
}

DISABLE_CFI_PERF
bool SharedStyleFinder::CanShareStyleWithElement(Element& candidate) const {
  if (GetElement() == candidate)
    return false;
  Element* parent = candidate.ParentOrShadowHostElement();
  const ComputedStyle* style = GetElementStyle(candidate);
  if (!style)
    return false;
  if (!style->IsSharable())
    return false;
  if (!parent)
    return false;
  if (GetElementStyle(*GetElement().ParentOrShadowHostElement()) !=
      GetElementStyle(*parent))
    return false;
  if (candidate.TagQName() != GetElement().TagQName())
    return false;
  if (candidate.InlineStyle())
    return false;
  if (candidate.NeedsStyleRecalc() && !candidate.NeedsReattachLayoutTree())
    return false;
  if (candidate.IsSVGElement() &&
      ToSVGElement(candidate).AnimatedSMILStyleProperties())
    return false;
  if (candidate.IsLink() != GetElement().IsLink())
    return false;
  if (candidate.ShadowPseudoId() != GetElement().ShadowPseudoId())
    return false;
  if (!SharingCandidateHasIdenticalStyleAffectingAttributes(candidate))
    return false;
  if (candidate.AdditionalPresentationAttributeStyle() !=
      GetElement().AdditionalPresentationAttributeStyle())
    return false;
  if (candidate.HasID() &&
      features_.HasSelectorForId(candidate.IdForStyleResolution()))
    return false;
  if (!SharingCandidateCanShareHostStyles(candidate))
    return false;
  // For Shadow DOM V1
  if (!SharingCandidateAssignedToSameSlot(candidate))
    return false;
  // For Shadow DOM V0
  if (!SharingCandidateDistributedToSameInsertionPoint(candidate))
    return false;
  if (candidate.IsInTopLayer() != GetElement().IsInTopLayer())
    return false;

  bool is_control = candidate.IsFormControlElement();
  DCHECK_EQ(is_control, GetElement().IsFormControlElement());
  if (is_control && !CanShareStyleWithControl(candidate))
    return false;

  if (isHTMLOptionElement(candidate) && isHTMLOptionElement(GetElement()) &&
      (toHTMLOptionElement(candidate).Selected() !=
           toHTMLOptionElement(GetElement()).Selected() ||
       toHTMLOptionElement(candidate).SpatialNavigationFocused() !=
           toHTMLOptionElement(GetElement()).SpatialNavigationFocused()))
    return false;

  // FIXME: This line is surprisingly hot, we may wish to inline
  // hasDirectionAuto into StyleResolver.
  if (candidate.IsHTMLElement() && ToHTMLElement(candidate).HasDirectionAuto())
    return false;

  if (isHTMLImageElement(candidate) && isHTMLImageElement(GetElement()) &&
      toHTMLImageElement(candidate).IsCollapsed() !=
          toHTMLImageElement(GetElement()).IsCollapsed()) {
    return false;
  }

  if (candidate.IsLink() && context_.ElementLinkState() != style->InsideLink())
    return false;

  if (candidate.IsUnresolvedV0CustomElement() !=
      GetElement().IsUnresolvedV0CustomElement())
    return false;

  if (GetElement().ParentOrShadowHostElement() != parent) {
    if (!parent->IsStyledElement())
      return false;
    if (parent->InlineStyle())
      return false;
    if (parent->IsSVGElement() &&
        ToSVGElement(parent)->AnimatedSMILStyleProperties())
      return false;
    if (parent->HasID() &&
        features_.HasSelectorForId(parent->IdForStyleResolution()))
      return false;
    if (!parent->ChildrenSupportStyleSharing())
      return false;
  }

  ShadowRoot* root1 = GetElement().ContainingShadowRoot();
  ShadowRoot* root2 = candidate.ContainingShadowRoot();
  if (root1 && root2 && root1->GetType() != root2->GetType())
    return false;

  if (GetDocument().ContainsValidityStyleRules()) {
    if (candidate.IsValidElement() != GetElement().IsValidElement())
      return false;
  }

  return true;
}

bool SharedStyleFinder::DocumentContainsValidCandidate() const {
  for (Element& element :
       ElementTraversal::StartsAt(GetDocument().documentElement())) {
    if (element.SupportsStyleSharing() && CanShareStyleWithElement(element))
      return true;
  }
  return false;
}

inline Element* SharedStyleFinder::FindElementForStyleSharing() const {
  StyleSharingList& style_sharing_list = style_resolver_->GetStyleSharingList();
  for (StyleSharingList::iterator it = style_sharing_list.begin();
       it != style_sharing_list.end(); ++it) {
    Element& candidate = **it;
    if (!CanShareStyleWithElement(candidate))
      continue;
    if (it != style_sharing_list.begin()) {
      // Move the element to the front of the LRU
      style_sharing_list.erase(it);
      style_sharing_list.push_front(&candidate);
    }
    return &candidate;
  }
  style_resolver_->AddToStyleSharingList(GetElement());
  return nullptr;
}

bool SharedStyleFinder::MatchesRuleSet(RuleSet* rule_set) {
  if (!rule_set)
    return false;
  ElementRuleCollector collector(context_,
                                 style_resolver_->GetSelectorFilter());
  return collector.HasAnyMatchingRules(rule_set);
}

ComputedStyle* SharedStyleFinder::FindSharedStyle() {
  INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                shared_style_lookups, 1);

  if (!GetElement().SupportsStyleSharing())
    return nullptr;

  // Cache whether context.element() is affected by any known class selectors.
  element_affected_by_class_rules_ =
      GetElement().HasClass() &&
      ClassNamesAffectedByRules(GetElement().ClassNames());

  Element* share_element = FindElementForStyleSharing();

  if (!share_element) {
    if (GetDocument().GetStyleEngine().Stats() &&
        GetDocument().GetStyleEngine().Stats()->AllCountersEnabled() &&
        DocumentContainsValidCandidate())
      INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                    shared_style_missed, 1);
    return nullptr;
  }

  INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                shared_style_found, 1);

  if (MatchesRuleSet(sibling_rule_set_)) {
    INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                  shared_style_rejected_by_sibling_rules, 1);
    return nullptr;
  }

  if (MatchesRuleSet(uncommon_attribute_rule_set_)) {
    INCREMENT_STYLE_STATS_COUNTER(
        GetDocument().GetStyleEngine(),
        shared_style_rejected_by_uncommon_attribute_rules, 1);
    return nullptr;
  }

  // Tracking child index requires unique style for each node. This may get set
  // by the sibling rule match above.
  if (!GetElement()
           .ParentElementOrShadowRoot()
           ->ChildrenSupportStyleSharing()) {
    INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                  shared_style_rejected_by_parent, 1);
    return nullptr;
  }

  return GetElementStyle(*share_element);
}

}  // namespace blink
