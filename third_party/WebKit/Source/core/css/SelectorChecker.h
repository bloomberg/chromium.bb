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

#ifndef SelectorChecker_h
#define SelectorChecker_h

#include "base/macros.h"
#include "core/css/CSSSelector.h"
#include "core/dom/Element.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class CSSSelector;
class ContainerNode;
class Element;
class LayoutScrollbar;
class ComputedStyle;

class SelectorChecker {
  STACK_ALLOCATED();

 public:
  enum VisitedMatchType { kVisitedMatchDisabled, kVisitedMatchEnabled };

  enum Mode {
    // Used when matching selectors inside style recalc. This mode will set
    // restyle flags across the tree during matching which impact how style
    // sharing and invalidation work later.
    kResolvingStyle,

    // Used when collecting which rules match into a StyleRuleList, the engine
    // internal represention.
    //
    // TODO(esprehn): This doesn't change the behavior of the SelectorChecker
    // we should merge it with a generic CollectingRules mode.
    kCollectingStyleRules,

    // Used when collecting which rules match into a CSSRuleList, the CSSOM api
    // represention.
    //
    // TODO(esprehn): This doesn't change the behavior of the SelectorChecker
    // we should merge it with a generic CollectingRules mode.
    kCollectingCSSRules,

    // Used when matching rules for querySelector and <content select>. This
    // disables the special handling for positional selectors during parsing
    // and also enables static profile only selectors like >>>.
    kQueryingRules,
  };

  struct Init {
    STACK_ALLOCATED();

   public:
    Mode mode = kResolvingStyle;
    bool is_ua_rule = false;
    ComputedStyle* element_style = nullptr;
    Member<LayoutScrollbar> scrollbar = nullptr;
    ScrollbarPart scrollbar_part = kNoPart;
  };

  explicit SelectorChecker(const Init& init)
      : mode_(init.mode),
        is_ua_rule_(init.is_ua_rule),
        element_style_(init.element_style),
        scrollbar_(init.scrollbar),
        scrollbar_part_(init.scrollbar_part) {}

  struct SelectorCheckingContext {
    STACK_ALLOCATED();

   public:
    // Initial selector constructor
    SelectorCheckingContext(Element* element,
                            VisitedMatchType visited_match_type)
        : selector(nullptr),
          element(element),
          previous_element(nullptr),
          scope(nullptr),
          visited_match_type(visited_match_type),
          pseudo_id(kPseudoIdNone),
          is_sub_selector(false),
          in_rightmost_compound(true),
          has_scrollbar_pseudo(false),
          has_selection_pseudo(false),
          treat_shadow_host_as_normal_scope(false) {}

    const CSSSelector* selector;
    Member<Element> element;
    Member<Element> previous_element;
    Member<const ContainerNode> scope;
    VisitedMatchType visited_match_type;
    PseudoId pseudo_id;
    bool is_sub_selector;
    bool in_rightmost_compound;
    bool has_scrollbar_pseudo;
    bool has_selection_pseudo;
    bool treat_shadow_host_as_normal_scope;
  };

  struct MatchResult {
    STACK_ALLOCATED();
    MatchResult() : dynamic_pseudo(kPseudoIdNone), specificity(0) {}

    PseudoId dynamic_pseudo;
    unsigned specificity;
  };

  bool Match(const SelectorCheckingContext& context,
             MatchResult& result) const {
    DCHECK(context.selector);
    return MatchSelector(context, result) == kSelectorMatches;
  }

  bool Match(const SelectorCheckingContext& context) const {
    MatchResult ignore_result;
    return Match(context, ignore_result);
  }

  static bool MatchesFocusPseudoClass(const Element&);

 private:
  bool CheckOne(const SelectorCheckingContext&, MatchResult&) const;

  enum MatchStatus {
    kSelectorMatches,
    kSelectorFailsLocally,
    kSelectorFailsAllSiblings,
    kSelectorFailsCompletely
  };

  MatchStatus MatchSelector(const SelectorCheckingContext&, MatchResult&) const;
  MatchStatus MatchForSubSelector(const SelectorCheckingContext&,
                                  MatchResult&) const;
  MatchStatus MatchForRelation(const SelectorCheckingContext&,
                               MatchResult&) const;
  MatchStatus MatchForPseudoContent(const SelectorCheckingContext&,
                                    const Element&,
                                    MatchResult&) const;
  MatchStatus MatchForPseudoShadow(const SelectorCheckingContext&,
                                   const ContainerNode*,
                                   MatchResult&) const;
  bool CheckPseudoClass(const SelectorCheckingContext&, MatchResult&) const;
  bool CheckPseudoElement(const SelectorCheckingContext&, MatchResult&) const;
  bool CheckScrollbarPseudoClass(const SelectorCheckingContext&,
                                 MatchResult&) const;
  bool CheckPseudoHost(const SelectorCheckingContext&, MatchResult&) const;
  bool CheckPseudoNot(const SelectorCheckingContext&, MatchResult&) const;

  Mode mode_;
  bool is_ua_rule_;
  ComputedStyle* element_style_;
  Member<LayoutScrollbar> scrollbar_;
  ScrollbarPart scrollbar_part_;
  DISALLOW_COPY_AND_ASSIGN(SelectorChecker);
};

}  // namespace blink

#endif
