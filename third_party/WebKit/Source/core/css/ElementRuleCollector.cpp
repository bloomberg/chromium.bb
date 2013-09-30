/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#include "config.h"
#include "core/css/ElementRuleCollector.h"

#include "core/css/CSSRuleList.h"
#include "core/css/CSSSelector.h"
#include "core/css/SelectorCheckerFastPath.h"
#include "core/css/SiblingTraversalStrategies.h"
#include "core/css/StylePropertySet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/rendering/RenderRegion.h"

namespace WebCore {

ElementRuleCollector::ElementRuleCollector(const ElementResolveContext& context,
    const SelectorFilter& filter, RenderStyle* style)
    : m_context(context)
    , m_selectorFilter(filter)
    , m_style(style)
    , m_regionForStyling(0)
    , m_pseudoStyleRequest(NOPSEUDO)
    , m_mode(SelectorChecker::ResolvingStyle)
    , m_behaviorAtBoundary(SelectorChecker::DoesNotCrossBoundary)
    , m_canUseFastReject(m_selectorFilter.parentStackIsConsistent(context.parentNode()))
    , m_sameOriginOnly(false)
    , m_matchingUARules(false)
{ }

MatchResult& ElementRuleCollector::matchedResult()
{
    return m_result;
}

PassRefPtr<CSSRuleList> ElementRuleCollector::matchedRuleList()
{
    ASSERT(m_mode == SelectorChecker::CollectingRules);
    return m_ruleList.release();
}

inline void ElementRuleCollector::addMatchedRule(const RuleData* rule, CascadeScope cascadeScope, CascadeOrder cascadeOrder)
{
    if (!m_matchedRules)
        m_matchedRules = adoptPtr(new Vector<MatchedRule, 32>);
    m_matchedRules->append(MatchedRule(rule, cascadeScope, cascadeOrder));
}

void ElementRuleCollector::clearMatchedRules()
{
    if (!m_matchedRules)
        return;
    m_matchedRules->clear();
}

inline StaticCSSRuleList* ElementRuleCollector::ensureRuleList()
{
    if (!m_ruleList)
        m_ruleList = StaticCSSRuleList::create();
    return m_ruleList.get();
}

void ElementRuleCollector::addElementStyleProperties(const StylePropertySet* propertySet, bool isCacheable)
{
    if (!propertySet)
        return;
    m_result.ranges.lastAuthorRule = m_result.matchedProperties.size();
    if (m_result.ranges.firstAuthorRule == -1)
        m_result.ranges.firstAuthorRule = m_result.ranges.lastAuthorRule;
    m_result.addMatchedProperties(propertySet);
    if (!isCacheable)
        m_result.isCacheable = false;
}

static bool rulesApplicableInCurrentTreeScope(const Element* element, const ContainerNode* scopingNode, SelectorChecker::BehaviorAtBoundary behaviorAtBoundary, bool elementApplyAuthorStyles)
{
    TreeScope& treeScope = element->treeScope();

    // [skipped, because already checked] a) it's a UA rule
    // b) element is allowed to apply author rules
    if (elementApplyAuthorStyles)
        return true;
    // c) the rules comes from a scoped style sheet within the same tree scope
    if (!scopingNode || &treeScope == &scopingNode->treeScope())
        return true;
    // d) the rules comes from a scoped style sheet within an active shadow root whose host is the given element
    if (element->isInShadowTree() && (behaviorAtBoundary & SelectorChecker::ScopeIsShadowHost) && scopingNode == element->containingShadowRoot()->host())
        return true;
    // e) the rules can cross boundaries
    if ((behaviorAtBoundary & SelectorChecker::BoundaryBehaviorMask) == SelectorChecker::CrossesBoundary)
        return true;
    return false;
}

void ElementRuleCollector::collectMatchingRules(const MatchRequest& matchRequest, RuleRange& ruleRange, CascadeScope cascadeScope, CascadeOrder cascadeOrder)
{
    ASSERT(matchRequest.ruleSet);
    ASSERT(m_context.element());

    Element* element = m_context.element();
    const AtomicString& pseudoId = element->shadowPseudoId();
    if (!pseudoId.isEmpty()) {
        ASSERT(element->isStyledElement());
        collectMatchingRulesForList(matchRequest.ruleSet->shadowPseudoElementRules(pseudoId.impl()), ignoreCascadeScope, cascadeOrder, matchRequest, ruleRange);
    }

    if (element->isWebVTTElement())
        collectMatchingRulesForList(matchRequest.ruleSet->cuePseudoRules(), cascadeScope, cascadeOrder, matchRequest, ruleRange);
    // Check whether other types of rules are applicable in the current tree scope. Criteria for this:
    // a) it's a UA rule
    // b) the tree scope allows author rules
    // c) the rules comes from a scoped style sheet within the same tree scope
    // d) the rules comes from a scoped style sheet within an active shadow root whose host is the given element
    // e) the rules can cross boundaries
    // b)-e) is checked in rulesApplicableInCurrentTreeScope.
    if (!m_matchingUARules && !rulesApplicableInCurrentTreeScope(element, matchRequest.scope, m_behaviorAtBoundary, matchRequest.elementApplyAuthorStyles))
        return;

    // We need to collect the rules for id, class, tag, and everything else into a buffer and
    // then sort the buffer.
    if (element->hasID())
        collectMatchingRulesForList(matchRequest.ruleSet->idRules(element->idForStyleResolution().impl()), cascadeScope, cascadeOrder, matchRequest, ruleRange);
    if (element->isStyledElement() && element->hasClass()) {
        for (size_t i = 0; i < element->classNames().size(); ++i)
            collectMatchingRulesForList(matchRequest.ruleSet->classRules(element->classNames()[i].impl()), cascadeScope, cascadeOrder, matchRequest, ruleRange);
    }

    if (element->isLink())
        collectMatchingRulesForList(matchRequest.ruleSet->linkPseudoClassRules(), cascadeScope, cascadeOrder, matchRequest, ruleRange);
    if (SelectorChecker::matchesFocusPseudoClass(element))
        collectMatchingRulesForList(matchRequest.ruleSet->focusPseudoClassRules(), cascadeScope, cascadeOrder, matchRequest, ruleRange);
    collectMatchingRulesForList(matchRequest.ruleSet->tagRules(element->localName().impl()), cascadeScope, cascadeOrder, matchRequest, ruleRange);
    collectMatchingRulesForList(matchRequest.ruleSet->universalRules(), cascadeScope, cascadeOrder, matchRequest, ruleRange);
}

void ElementRuleCollector::collectMatchingRulesForRegion(const MatchRequest& matchRequest, RuleRange& ruleRange, CascadeScope cascadeScope, CascadeOrder cascadeOrder)
{
    if (!m_regionForStyling)
        return;

    unsigned size = matchRequest.ruleSet->m_regionSelectorsAndRuleSets.size();
    for (unsigned i = 0; i < size; ++i) {
        const CSSSelector* regionSelector = matchRequest.ruleSet->m_regionSelectorsAndRuleSets.at(i).selector;
        if (checkRegionSelector(regionSelector, toElement(m_regionForStyling->node()))) {
            RuleSet* regionRules = matchRequest.ruleSet->m_regionSelectorsAndRuleSets.at(i).ruleSet.get();
            ASSERT(regionRules);
            collectMatchingRules(MatchRequest(regionRules, matchRequest.includeEmptyRules, matchRequest.scope), ruleRange, cascadeScope, cascadeOrder);
        }
    }
}

void ElementRuleCollector::sortAndTransferMatchedRules()
{
    if (!m_matchedRules || m_matchedRules->isEmpty())
        return;

    sortMatchedRules();

    Vector<MatchedRule, 32>& matchedRules = *m_matchedRules;
    if (m_mode == SelectorChecker::CollectingRules) {
        for (unsigned i = 0; i < matchedRules.size(); ++i)
            ensureRuleList()->rules().append(matchedRules[i].ruleData()->rule()->createCSSOMWrapper());
        return;
    }

    // Now transfer the set of matched rules over to our list of declarations.
    for (unsigned i = 0; i < matchedRules.size(); i++) {
        // FIXME: Matching should not modify the style directly.
        const RuleData* ruleData = matchedRules[i].ruleData();
        if (m_style && ruleData->containsUncommonAttributeSelector())
            m_style->setUnique();
        m_result.addMatchedProperties(ruleData->rule()->properties(), ruleData->rule(), ruleData->linkMatchType(), ruleData->propertyWhitelistType(m_matchingUARules));
    }
}

inline bool ElementRuleCollector::ruleMatches(const RuleData& ruleData, const ContainerNode* scope, PseudoId& dynamicPseudo)
{
    if (ruleData.hasFastCheckableSelector()) {
        // We know this selector does not include any pseudo elements.
        if (m_pseudoStyleRequest.pseudoId != NOPSEUDO)
            return false;
        // We know a sufficiently simple single part selector matches simply because we found it from the rule hash.
        // This is limited to HTML only so we don't need to check the namespace.
        if (ruleData.hasRightmostSelectorMatchingHTMLBasedOnRuleHash() && m_context.element()->isHTMLElement()) {
            if (!ruleData.hasMultipartSelector())
                return true;
        }
        if (ruleData.selector()->m_match == CSSSelector::Tag && !SelectorChecker::tagMatches(m_context.element(), ruleData.selector()->tagQName()))
            return false;
        SelectorCheckerFastPath selectorCheckerFastPath(ruleData.selector(), m_context.element());
        if (!selectorCheckerFastPath.matchesRightmostAttributeSelector())
            return false;

        return selectorCheckerFastPath.matches();
    }

    // Slow path.
    SelectorChecker selectorChecker(document(), m_mode);
    SelectorChecker::SelectorCheckingContext context(ruleData.selector(), m_context.element(), SelectorChecker::VisitedMatchEnabled);
    context.elementStyle = m_style.get();
    context.scope = scope;
    context.pseudoId = m_pseudoStyleRequest.pseudoId;
    context.scrollbar = m_pseudoStyleRequest.scrollbar;
    context.scrollbarPart = m_pseudoStyleRequest.scrollbarPart;
    context.behaviorAtBoundary = m_behaviorAtBoundary;
    SelectorChecker::Match match = selectorChecker.match(context, dynamicPseudo, DOMSiblingTraversalStrategy());
    if (match != SelectorChecker::SelectorMatches)
        return false;
    if (m_pseudoStyleRequest.pseudoId != NOPSEUDO && m_pseudoStyleRequest.pseudoId != dynamicPseudo)
        return false;
    if (m_behaviorAtBoundary == SelectorChecker::ScopeIsShadowHost && scope == m_context.element() && !ruleData.selector()->hasHostPseudoClass())
        return false;
    return true;
}

void ElementRuleCollector::collectRuleIfMatches(const RuleData& ruleData, CascadeScope cascadeScope, CascadeOrder cascadeOrder, const MatchRequest& matchRequest, RuleRange& ruleRange)
{
    if (m_canUseFastReject && m_selectorFilter.fastRejectSelector<RuleData::maximumIdentifierCount>(ruleData.descendantSelectorIdentifierHashes()))
        return;

    StyleRule* rule = ruleData.rule();
    PseudoId dynamicPseudo = NOPSEUDO;
    if (ruleMatches(ruleData, matchRequest.scope, dynamicPseudo)) {
        // If the rule has no properties to apply, then ignore it in the non-debug mode.
        const StylePropertySet* properties = rule->properties();
        if (!properties || (properties->isEmpty() && !matchRequest.includeEmptyRules))
            return;
        // FIXME: Exposing the non-standard getMatchedCSSRules API to web is the only reason this is needed.
        if (m_sameOriginOnly && !ruleData.hasDocumentSecurityOrigin())
            return;
        // If we're matching normal rules, set a pseudo bit if
        // we really just matched a pseudo-element.
        if (dynamicPseudo != NOPSEUDO && m_pseudoStyleRequest.pseudoId == NOPSEUDO) {
            if (m_mode == SelectorChecker::CollectingRules)
                return;
            // FIXME: Matching should not modify the style directly.
            if (dynamicPseudo < FIRST_INTERNAL_PSEUDOID)
                m_style->setHasPseudoStyle(dynamicPseudo);
        } else {
            // Update our first/last rule indices in the matched rules array.
            ++ruleRange.lastRuleIndex;
            if (ruleRange.firstRuleIndex == -1)
                ruleRange.firstRuleIndex = ruleRange.lastRuleIndex;

            // Add this rule to our list of matched rules.
            addMatchedRule(&ruleData, cascadeScope, cascadeOrder);
            return;
        }
    }
}

void ElementRuleCollector::collectMatchingRulesForList(const RuleData* rules, CascadeScope cascadeScope, CascadeOrder cascadeOrder, const MatchRequest& matchRequest, RuleRange& ruleRange)
{
    if (!rules)
        return;
    while (!rules->isLastInArray())
        collectRuleIfMatches(*rules++, cascadeScope, cascadeOrder, matchRequest, ruleRange);
    collectRuleIfMatches(*rules, cascadeScope, cascadeOrder, matchRequest, ruleRange);
}

void ElementRuleCollector::collectMatchingRulesForList(const Vector<RuleData>* rules, CascadeScope cascadeScope, CascadeOrder cascadeOrder, const MatchRequest& matchRequest, RuleRange& ruleRange)
{
    if (!rules)
        return;
    unsigned size = rules->size();
    for (unsigned i = 0; i < size; ++i)
        collectRuleIfMatches(rules->at(i), cascadeScope, cascadeOrder, matchRequest, ruleRange);
}

static inline bool compareRules(const MatchedRule& matchedRule1, const MatchedRule& matchedRule2)
{
    unsigned specificity1 = matchedRule1.ruleData()->specificity();
    unsigned specificity2 = matchedRule2.ruleData()->specificity();
    return matchedRule1.cascadeScope() == matchedRule2.cascadeScope() ?
        ((specificity1 == specificity2) ? matchedRule1.position() < matchedRule2.position() : specificity1 < specificity2) :
        matchedRule1.cascadeScope() > matchedRule2.cascadeScope();
}

void ElementRuleCollector::sortMatchedRules()
{
    ASSERT(m_matchedRules);
    std::sort(m_matchedRules->begin(), m_matchedRules->end(), compareRules);
}

bool ElementRuleCollector::hasAnyMatchingRules(RuleSet* ruleSet)
{
    clearMatchedRules();

    m_mode = SelectorChecker::SharingRules;
    // To check whether a given RuleSet has any rule matching a given element,
    // should not see the element's treescope. Because RuleSet has no
    // information about "scope".
    m_behaviorAtBoundary = SelectorChecker::StaysWithinTreeScope;
    int firstRuleIndex = -1, lastRuleIndex = -1;
    RuleRange ruleRange(firstRuleIndex, lastRuleIndex);
    // FIXME: Verify whether it's ok to ignore CascadeScope here.
    collectMatchingRules(MatchRequest(ruleSet), ruleRange);

    return m_matchedRules && !m_matchedRules->isEmpty();
}

} // namespace WebCore
