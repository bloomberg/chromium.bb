/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#include "config.h"
#include "core/css/TreeBoundaryCrossingRules.h"

#include "core/css/ElementRuleCollector.h"
#include "core/css/RuleFeature.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ShadowRoot.h"

namespace blink {

static void addRules(RuleSet* ruleSet, const WillBeHeapVector<MinimalRuleData>& rules)
{
    for (unsigned i = 0; i < rules.size(); ++i) {
        const MinimalRuleData& info = rules[i];
        ruleSet->addRule(info.m_rule, info.m_selectorIndex, info.m_flags);
    }
}

void TreeBoundaryCrossingRules::addTreeBoundaryCrossingRules(const RuleSet& authorRules, CSSStyleSheet* parentStyleSheet, unsigned parentIndex, ContainerNode& scopingNode)
{
    if (authorRules.treeBoundaryCrossingRules().isEmpty() && (scopingNode.isDocumentNode() || authorRules.shadowDistributedRules().isEmpty()))
        return;
    OwnPtrWillBeRawPtr<RuleSet> ruleSetForScope = RuleSet::create();
    addRules(ruleSetForScope.get(), authorRules.treeBoundaryCrossingRules());
    if (!scopingNode.isDocumentNode())
        addRules(ruleSetForScope.get(), authorRules.shadowDistributedRules());

    if (!m_treeBoundaryCrossingRuleSetMap.contains(&scopingNode)) {
        m_treeBoundaryCrossingRuleSetMap.add(&scopingNode, adoptPtrWillBeNoop(new CSSStyleSheetRuleSubSet()));
        m_scopingNodes.add(&scopingNode);
    }
    CSSStyleSheetRuleSubSet* ruleSubSet = m_treeBoundaryCrossingRuleSetMap.get(&scopingNode);
    ruleSubSet->append(RuleSubSet::create(parentStyleSheet, parentIndex, ruleSetForScope.release()));
}

void TreeBoundaryCrossingRules::collectTreeBoundaryCrossingRules(Element* element, ElementRuleCollector& collector, bool includeEmptyRules)
{
    if (m_treeBoundaryCrossingRuleSetMap.isEmpty())
        return;

    RuleRange ruleRange = collector.matchedResult().ranges.authorRuleRange();

    // When comparing rules declared in outer treescopes, outer's rules win.
    CascadeOrder outerCascadeOrder = size() + size();
    // When comparing rules declared in inner treescopes, inner's rules win.
    CascadeOrder innerCascadeOrder = size();

    ASSERT(!collector.scopeContainsLastMatchedElement());
    collector.setScopeContainsLastMatchedElement(true);
    for (DocumentOrderedList::iterator it = m_scopingNodes.begin(); it != m_scopingNodes.end(); ++it) {
        const ContainerNode* scopingNode = toContainerNode(*it);
        CSSStyleSheetRuleSubSet* ruleSubSet = m_treeBoundaryCrossingRuleSetMap.get(scopingNode);
        bool isInnerTreeScope = element->treeScope().isInclusiveAncestorOf(scopingNode->treeScope());

        CascadeOrder cascadeOrder = isInnerTreeScope ? innerCascadeOrder : outerCascadeOrder;
        for (CSSStyleSheetRuleSubSet::iterator it = ruleSubSet->begin(); it != ruleSubSet->end(); ++it) {
            MatchRequest request((*it)->ruleSet.get(), includeEmptyRules, scopingNode, (*it)->parentStyleSheet, (*it)->parentIndex);
            collector.collectMatchingRules(request, ruleRange, ignoreCascadeScope, cascadeOrder, true);
        }
        ++innerCascadeOrder;
        --outerCascadeOrder;
    }
    collector.setScopeContainsLastMatchedElement(false);
}

void TreeBoundaryCrossingRules::reset(const ContainerNode* scopingNode)
{
    m_treeBoundaryCrossingRuleSetMap.remove(scopingNode);
    m_scopingNodes.remove(scopingNode);
}

void TreeBoundaryCrossingRules::collectFeaturesFromRuleSubSet(CSSStyleSheetRuleSubSet* ruleSubSet, RuleFeatureSet& features)
{
    for (CSSStyleSheetRuleSubSet::iterator it = ruleSubSet->begin(); it != ruleSubSet->end(); ++it)
        features.add((*it)->ruleSet->features());
}

void TreeBoundaryCrossingRules::collectFeaturesTo(RuleFeatureSet& features)
{
    for (TreeBoundaryCrossingRuleSetMap::iterator::Values it = m_treeBoundaryCrossingRuleSetMap.values().begin(); it != m_treeBoundaryCrossingRuleSetMap.values().end(); ++it)
        collectFeaturesFromRuleSubSet(it->get(), features);
}

void TreeBoundaryCrossingRules::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_treeBoundaryCrossingRuleSetMap);
    visitor->trace(m_scopingNodes);
#endif
}

void TreeBoundaryCrossingRules::RuleSubSet::trace(Visitor* visitor)
{
    visitor->trace(ruleSet);
}

} // namespace blink
