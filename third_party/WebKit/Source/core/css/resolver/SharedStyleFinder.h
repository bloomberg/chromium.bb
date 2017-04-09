/*
 * Copyright (C) 2013 Google, Inc.
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
 */

#ifndef SharedStyleFinder_h
#define SharedStyleFinder_h

#include "core/css/resolver/ElementResolveContext.h"
#include "core/dom/Element.h"

namespace blink {

class Element;
class ComputedStyle;
class RuleFeatureSet;
class RuleSet;
class SpaceSplitString;
class StyleResolver;

class CORE_EXPORT SharedStyleFinder {
  STACK_ALLOCATED();

 public:
  // RuleSets are passed non-const as the act of matching against them can cause
  // them to be compacted. :(
  SharedStyleFinder(const ElementResolveContext& context,
                    const RuleFeatureSet& features,
                    RuleSet* sibling_rule_set,
                    RuleSet* uncommon_attribute_rule_set,
                    StyleResolver& style_resolver)
      : element_affected_by_class_rules_(false),
        features_(features),
        sibling_rule_set_(sibling_rule_set),
        uncommon_attribute_rule_set_(uncommon_attribute_rule_set),
        style_resolver_(&style_resolver),
        context_(context) {}

  ComputedStyle* FindSharedStyle();

 private:
  Element* FindElementForStyleSharing() const;

  // Only used when we're collecting stats on styles.
  bool DocumentContainsValidCandidate() const;

  bool ClassNamesAffectedByRules(const SpaceSplitString&) const;

  bool CanShareStyleWithElement(Element& candidate) const;
  bool CanShareStyleWithControl(Element& candidate) const;
  bool SharingCandidateHasIdenticalStyleAffectingAttributes(
      Element& candidate) const;
  bool SharingCandidateCanShareHostStyles(Element& candidate) const;
  bool SharingCandidateAssignedToSameSlot(Element& candidate) const;
  bool SharingCandidateDistributedToSameInsertionPoint(
      Element& candidate) const;
  bool MatchesRuleSet(RuleSet*);

  Element& GetElement() const { return *context_.GetElement(); }
  Document& GetDocument() const { return GetElement().GetDocument(); }

  bool element_affected_by_class_rules_;
  const RuleFeatureSet& features_;
  Member<RuleSet> sibling_rule_set_;
  Member<RuleSet> uncommon_attribute_rule_set_;
  Member<StyleResolver> style_resolver_;
  const ElementResolveContext& context_;

  friend class SharedStyleFinderTest;
};

}  // namespace blink

#endif  // SharedStyleFinder_h
