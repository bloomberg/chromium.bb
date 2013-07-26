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

#include "core/css/PseudoStyleRequest.h"
#include "core/css/SelectorChecker.h"
#include "core/css/resolver/ElementResolveContext.h"
#include "core/css/resolver/MatchRequest.h"
#include "core/css/resolver/MatchResult.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class CSSRuleList;
class DocumentRuleSets;
class RenderRegion;
class RuleData;
class RuleSet;
class ScopedStyleResolver;
class SelectorFilter;
class StaticCSSRuleList;

// ElementRuleCollector is designed to be used as a stack object.
// Create one, ask what rules the ElementResolveContext matches
// and then let it go out of scope.
// FIXME: Currently it modifies the RenderStyle but should not!
class ElementRuleCollector {
    WTF_MAKE_NONCOPYABLE(ElementRuleCollector);
public:
    // FIXME: This should not need an InspectorCSSOMWrappers parameter.
    ElementRuleCollector(const ElementResolveContext&, const SelectorFilter&, RenderStyle*, InspectorCSSOMWrappers&);

    void setBehaviorAtBoundary(SelectorChecker::BehaviorAtBoundary boundary) { m_behaviorAtBoundary = boundary; }
    SelectorChecker::BehaviorAtBoundary behaviorAtBoundary() const { return m_behaviorAtBoundary; }
    void setCanUseFastReject(bool canUseFastReject) { m_canUseFastReject = canUseFastReject; }
    bool canUseFastReject() const { return m_canUseFastReject; }

    void setMode(SelectorChecker::Mode mode) { m_mode = mode; }
    void setPseudoStyleRequest(const PseudoStyleRequest& request) { m_pseudoStyleRequest = request; }
    void setSameOriginOnly(bool f) { m_sameOriginOnly = f; }
    void setRegionForStyling(const RenderRegion* regionForStyling) { m_regionForStyling = regionForStyling; }

    void setMatchingUARules(bool matchingUARules) { m_matchingUARules = matchingUARules; }
    bool hasAnyMatchingRules(RuleSet*);

    MatchResult& matchedResult();
    PassRefPtr<CSSRuleList> matchedRuleList();

    void collectMatchingRules(const MatchRequest&, RuleRange&);
    void collectMatchingRulesForRegion(const MatchRequest&, RuleRange&);
    void sortAndTransferMatchedRules();
    void clearMatchedRules();
    void addElementStyleProperties(const StylePropertySet*, bool isCacheable = true);

private:
    Document* document() { return m_context.document(); }

    void collectRuleIfMatches(const RuleData&, const MatchRequest&, RuleRange&);
    void collectMatchingRulesForList(const Vector<RuleData>*, const MatchRequest&, RuleRange&);
    void collectMatchingRulesForList(const RuleData*, const MatchRequest&, RuleRange&);
    bool ruleMatches(const RuleData&, const ContainerNode* scope, PseudoId&);

    void sortMatchedRules();

    void addMatchedRule(const RuleData*);

    StaticCSSRuleList* ensureRuleList();

private:
    const ElementResolveContext& m_context;
    const SelectorFilter& m_selectorFilter;
    InspectorCSSOMWrappers& m_inspectorCSSOMWrappers; // This should not be needed.
    RefPtr<RenderStyle> m_style; // FIXME: This can be mutated during matching!

    const RenderRegion* m_regionForStyling;
    PseudoStyleRequest m_pseudoStyleRequest;
    SelectorChecker::Mode m_mode;
    SelectorChecker::BehaviorAtBoundary m_behaviorAtBoundary;
    bool m_canUseFastReject;
    bool m_sameOriginOnly;
    bool m_matchingUARules;

    OwnPtr<Vector<const RuleData*, 32> > m_matchedRules;

    // Output.
    RefPtr<StaticCSSRuleList> m_ruleList;
    MatchResult m_result;
};

} // namespace WebCore

#endif // ElementRuleCollector_h
