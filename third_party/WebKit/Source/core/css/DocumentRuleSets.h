/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
 *
 */

#ifndef DocumentRuleSets_h
#define DocumentRuleSets_h

#include "core/css/RuleFeature.h"
#include "core/css/RuleSet.h"
#include "core/dom/DocumentOrderedList.h"

#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class CSSStyleRule;
class CSSStyleSheet;
class InspectorCSSOMWrappers;
class MatchRequest;
class MediaQueryEvaluator;
class RuleSet;
class StyleEngine;

class TreeBoundaryCrossingRules {
public:
    void addRule(StyleRule*, size_t selectorIndex, ContainerNode* scopingNode, AddRuleFlags);
    void clear() { m_treeBoundaryCrossingRuleSetMap.clear(); }
    void reset(const ContainerNode* scopingNode);
    bool isEmpty() const { return m_treeBoundaryCrossingRuleSetMap.isEmpty(); }
    void collectFeaturesTo(RuleFeatureSet&);

    DocumentOrderedList::iterator begin() { return m_scopingNodes.begin(); }
    DocumentOrderedList::iterator end() { return m_scopingNodes.end(); }
    RuleSet* ruleSetScopedBy(const ContainerNode* scopingNode) { return m_treeBoundaryCrossingRuleSetMap.get(scopingNode); }

private:
    DocumentOrderedList m_scopingNodes;
    typedef HashMap<const ContainerNode*, OwnPtr<RuleSet> > TreeBoundaryCrossingRuleSetMap;
    TreeBoundaryCrossingRuleSetMap m_treeBoundaryCrossingRuleSetMap;
};

class DocumentRuleSets {
public:
    DocumentRuleSets();
    ~DocumentRuleSets();
    RuleSet* userStyle() const { return m_userStyle.get(); }

    void initUserStyle(StyleEngine*, const Vector<RefPtr<StyleRule> >& watchedSelectors, const MediaQueryEvaluator&, StyleResolver&);
    void resetAuthorStyle();
    void collectFeaturesTo(RuleFeatureSet&, bool isViewSource);

    TreeBoundaryCrossingRules& treeBoundaryCrossingRules() { return m_treeBoundaryCrossingRules; }

private:
    void collectRulesFromUserStyleSheets(const Vector<RefPtr<CSSStyleSheet> >&, RuleSet& userStyle, const MediaQueryEvaluator&, StyleResolver&);
    void collectRulesFromWatchedSelectors(const Vector<RefPtr<StyleRule> >&, RuleSet& userStyle);
    OwnPtr<RuleSet> m_userStyle;
    TreeBoundaryCrossingRules m_treeBoundaryCrossingRules;
};

} // namespace WebCore

#endif // DocumentRuleSets_h
