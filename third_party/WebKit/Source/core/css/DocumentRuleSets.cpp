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
#include "core/css/DocumentRuleSets.h"

#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/MatchRequest.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/StyleEngine.h"

namespace WebCore {

void ShadowDistributedRules::addRule(StyleRule* rule, size_t selectorIndex, ContainerNode* scopingNode, AddRuleFlags addRuleFlags)
{
    if (m_shadowDistributedRuleSetMap.contains(scopingNode))
        m_shadowDistributedRuleSetMap.get(scopingNode)->addRule(rule, selectorIndex, addRuleFlags);
    else {
        OwnPtr<RuleSet> ruleSetForScope = RuleSet::create();
        ruleSetForScope->addRule(rule, selectorIndex, addRuleFlags);
        m_shadowDistributedRuleSetMap.add(scopingNode, ruleSetForScope.release());
    }
}

void ShadowDistributedRules::collectMatchRequests(bool includeEmptyRules, Vector<MatchRequest>& matchRequests)
{
    for (ShadowDistributedRuleSetMap::iterator it = m_shadowDistributedRuleSetMap.begin(); it != m_shadowDistributedRuleSetMap.end(); ++it)
        matchRequests.append(MatchRequest(it->value.get(), includeEmptyRules, it->key));
}

void ShadowDistributedRules::reset(const ContainerNode* scopingNode)
{
    m_shadowDistributedRuleSetMap.remove(scopingNode);
}

void ShadowDistributedRules::collectFeaturesTo(RuleFeatureSet& features)
{
    for (ShadowDistributedRuleSetMap::iterator it = m_shadowDistributedRuleSetMap.begin(); it != m_shadowDistributedRuleSetMap.end(); ++it)
        features.add(it->value->features());
}

DocumentRuleSets::DocumentRuleSets()
{
}

DocumentRuleSets::~DocumentRuleSets()
{
}

void DocumentRuleSets::initUserStyle(StyleEngine* styleSheetCollection, const Vector<RefPtr<StyleRule> >& watchedSelectors, const MediaQueryEvaluator& medium, StyleResolver& resolver)
{
    OwnPtr<RuleSet> tempUserStyle = RuleSet::create();
    if (CSSStyleSheet* pageUserSheet = styleSheetCollection->pageUserSheet())
        tempUserStyle->addRulesFromSheet(pageUserSheet->contents(), medium, &resolver);
    collectRulesFromUserStyleSheets(styleSheetCollection->injectedUserStyleSheets(), *tempUserStyle, medium, resolver);
    collectRulesFromUserStyleSheets(styleSheetCollection->documentUserStyleSheets(), *tempUserStyle, medium, resolver);
    collectRulesFromWatchedSelectors(watchedSelectors, *tempUserStyle);
    if (tempUserStyle->ruleCount() > 0 || tempUserStyle->pageRules().size() > 0)
        m_userStyle = tempUserStyle.release();
}

void DocumentRuleSets::collectRulesFromUserStyleSheets(const Vector<RefPtr<CSSStyleSheet> >& userSheets, RuleSet& userStyle, const MediaQueryEvaluator& medium, StyleResolver& resolver)
{
    for (unsigned i = 0; i < userSheets.size(); ++i) {
        ASSERT(userSheets[i]->contents()->isUserStyleSheet());
        userStyle.addRulesFromSheet(userSheets[i]->contents(), medium, &resolver);
    }
}

void DocumentRuleSets::collectRulesFromWatchedSelectors(const Vector<RefPtr<StyleRule> >& watchedSelectors, RuleSet& userStyle)
{
    for (unsigned i = 0; i < watchedSelectors.size(); ++i)
        userStyle.addStyleRule(watchedSelectors[i].get(), RuleHasNoSpecialState);
}

void DocumentRuleSets::resetAuthorStyle()
{
    m_shadowDistributedRules.clear();
}

void DocumentRuleSets::collectFeaturesTo(RuleFeatureSet& features, bool isViewSource)
{
    // Collect all ids and rules using sibling selectors (:first-child and similar)
    // in the current set of stylesheets. Style sharing code uses this information to reject
    // sharing candidates.
    if (CSSDefaultStyleSheets::defaultStyle)
        features.add(CSSDefaultStyleSheets::defaultStyle->features());

    if (isViewSource)
        features.add(CSSDefaultStyleSheets::viewSourceStyle()->features());

    if (m_userStyle)
        features.add(m_userStyle->features());

    m_shadowDistributedRules.collectFeaturesTo(features);
}

} // namespace WebCore
