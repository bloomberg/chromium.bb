/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef ElementRuleCollector_h
#define ElementRuleCollector_h

#include "core/css/MediaQueryEvaluator.h"
#include "core/css/SelectorChecker.h"
#include "core/css/StyleResolver.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CSSRuleList;
class DocumentRuleSets;
class RenderRegion;
class RuleData;
class RuleSet;
class SelectorFilter;
class ScopedStyleResolver;
class StaticCSSRuleList;

class ElementRuleCollector {
public:
    ElementRuleCollector(StyleResolver* styleResolver, const StyleResolverState& state)
        : m_state(state)
        , m_selectorFilter(styleResolver->selectorFilter())
        , m_inspectorCSSOMWrappers(styleResolver->inspectorCSSOMWrappers())
        , m_regionForStyling(0)
        , m_pseudoStyleRequest(NOPSEUDO)
        , m_sameOriginOnly(false)
        , m_mode(SelectorChecker::ResolvingStyle)
        , m_canUseFastReject(m_selectorFilter.parentStackIsConsistent(state.parentNode()))
        , m_behaviorAtBoundary(SelectorChecker::DoesNotCrossBoundary) { }

    void setBehaviorAtBoundary(SelectorChecker::BehaviorAtBoundary boundary) { m_behaviorAtBoundary = boundary; }
    SelectorChecker::BehaviorAtBoundary behaviorAtBoundary() const { return m_behaviorAtBoundary; }
    void setCanUseFastReject(bool canUseFastReject) { m_canUseFastReject = canUseFastReject; }
    bool canUseFastReject() const { return m_canUseFastReject; }

    void setMode(SelectorChecker::Mode mode) { m_mode = mode; }
    void setPseudoStyleRequest(const PseudoStyleRequest& request) { m_pseudoStyleRequest = request; }
    void setSameOriginOnly(bool f) { m_sameOriginOnly = f; } 
    void setRegionForStyling(RenderRegion* regionForStyling) { m_regionForStyling = regionForStyling; }

    void setMatchingUARules(bool matchingUARules) { m_matchingUARules = matchingUARules; }
    bool hasAnyMatchingRules(RuleSet*);

    StyleResolver::MatchResult& matchedResult();
    PassRefPtr<CSSRuleList> matchedRuleList();

    void collectMatchingRules(const MatchRequest&, StyleResolver::RuleRange&);
    void collectMatchingRulesForRegion(const MatchRequest&, StyleResolver::RuleRange&);
    void sortAndTransferMatchedRules();
    void clearMatchedRules();
    void addElementStyleProperties(const StylePropertySet*, bool isCacheable = true);

private:
    Document* document() { return m_state.document(); }

    void collectMatchingRulesForList(const Vector<RuleData>*, const MatchRequest&, StyleResolver::RuleRange&);
    bool ruleMatches(const RuleData&, const ContainerNode* scope, PseudoId&);

    void sortMatchedRules();

    void addMatchedRule(const RuleData*);

    StaticCSSRuleList* ensureRuleList();
        
private:
    const StyleResolverState& m_state;
    SelectorFilter& m_selectorFilter;
    InspectorCSSOMWrappers& m_inspectorCSSOMWrappers;

    RenderRegion* m_regionForStyling;
    PseudoStyleRequest m_pseudoStyleRequest;
    bool m_sameOriginOnly;
    SelectorChecker::Mode m_mode;
    bool m_canUseFastReject;
    SelectorChecker::BehaviorAtBoundary m_behaviorAtBoundary;
    bool m_matchingUARules;

    OwnPtr<Vector<const RuleData*, 32> > m_matchedRules;

    // Output.
    RefPtr<StaticCSSRuleList> m_ruleList;
    StyleResolver::MatchResult m_result;
};

} // namespace WebCore

#endif // ElementRuleCollector_h
