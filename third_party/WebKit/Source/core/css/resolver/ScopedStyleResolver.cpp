/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/resolver/ScopedStyleResolver.h"

#include "HTMLNames.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/PageRuleCollector.h"
#include "core/css/RuleFeature.h"
#include "core/css/RuleSet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/StyleResolver.h" // For MatchRequest.
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLStyleElement.h"

namespace WebCore {

ContainerNode* ScopedStyleResolver::scopingNodeFor(Document& document, const CSSStyleSheet* sheet)
{
    ASSERT(sheet);

    Document* sheetDocument = sheet->ownerDocument();
    if (!sheetDocument)
        return 0;
    Node* ownerNode = sheet->ownerNode();
    if (!ownerNode || !isHTMLStyleElement(ownerNode))
        return &document;

    HTMLStyleElement* styleElement = toHTMLStyleElement(ownerNode);
    if (!styleElement->scoped()) {
        if (styleElement->isInShadowTree())
            return styleElement->containingShadowRoot();
        return &document;
    }

    ContainerNode* parent = styleElement->parentNode();
    if (!parent)
        return 0;

    return (parent->isElementNode() || parent->isShadowRoot()) ? parent : 0;
}

inline RuleSet* ScopedStyleResolver::ensureAuthorStyle()
{
    if (!m_authorStyle)
        m_authorStyle = RuleSet::create();
    return m_authorStyle.get();
}

void ScopedStyleResolver::addRulesFromSheet(StyleSheetContents* sheet, const MediaQueryEvaluator& medium, StyleResolver* resolver)
{
    AddRuleFlags addRuleFlags = resolver->document().securityOrigin()->canRequest(sheet->baseURL()) ? RuleHasDocumentSecurityOrigin : RuleHasNoSpecialState;
    ensureAuthorStyle()->addRulesFromSheet(sheet, medium, addRuleFlags);
    resolver->addMediaQueryResults(m_authorStyle->viewportDependentMediaQueryResults());
    resolver->processScopedRules(*m_authorStyle, sheet->baseURL(), &m_scopingNode);
}

inline RuleSet* ScopedStyleResolver::ensureAtHostRuleSetFor(const ShadowRoot* shadowRoot)
{
    HashMap<const ShadowRoot*, OwnPtr<RuleSet> >::AddResult addResult = m_atHostRules.add(shadowRoot, nullptr);
    if (addResult.isNewEntry)
        addResult.iterator->value = RuleSet::create();
    return addResult.iterator->value.get();
}

void ScopedStyleResolver::addHostRule(StyleRuleHost* hostRule, bool hasDocumentSecurityOrigin, const ContainerNode* scopingNode)
{
    if (!scopingNode)
        return;

    ShadowRoot* shadowRoot = scopingNode->containingShadowRoot();
    if (!shadowRoot || !shadowRoot->host())
        return;

    RuleSet* rule = ensureAtHostRuleSetFor(shadowRoot);

    const Vector<RefPtr<StyleRuleBase> >& childRules = hostRule->childRules();
    AddRuleFlags addRuleFlags = static_cast<AddRuleFlags>(hasDocumentSecurityOrigin ? RuleHasDocumentSecurityOrigin : RuleHasNoSpecialState);
    for (unsigned i = 0; i < childRules.size(); ++i) {
        StyleRuleBase* hostStylingRule = childRules[i].get();
        if (hostStylingRule->isStyleRule())
            rule->addStyleRule(static_cast<StyleRule*>(hostStylingRule), addRuleFlags);
    }
}

void ScopedStyleResolver::collectFeaturesTo(RuleFeatureSet& features)
{
    if (m_authorStyle)
        features.add(m_authorStyle->features());

    if (m_atHostRules.isEmpty())
        return;

    for (HashMap<const ShadowRoot*, OwnPtr<RuleSet> >::iterator it = m_atHostRules.begin(); it != m_atHostRules.end(); ++it)
        features.add(it->value->features());
}

void ScopedStyleResolver::resetAuthorStyle()
{
    m_authorStyle = RuleSet::create();
    m_keyframesRuleMap.clear();
}

void ScopedStyleResolver::resetAtHostRules(const ShadowRoot* shadowRoot)
{
    m_atHostRules.remove(shadowRoot);
}

bool ScopedStyleResolver::checkRegionStyle(Element* regionElement)
{
    if (!m_authorStyle)
        return false;

    unsigned rulesSize = m_authorStyle->m_regionSelectorsAndRuleSets.size();
    for (unsigned i = 0; i < rulesSize; ++i) {
        ASSERT(m_authorStyle->m_regionSelectorsAndRuleSets.at(i).ruleSet.get());
        if (checkRegionSelector(m_authorStyle->m_regionSelectorsAndRuleSets.at(i).selector, regionElement))
            return true;
    }
    return false;
}

const StyleRuleKeyframes* ScopedStyleResolver::keyframeStylesForAnimation(const StringImpl* animationName)
{
    if (m_keyframesRuleMap.isEmpty())
        return 0;

    KeyframesRuleMap::iterator it = m_keyframesRuleMap.find(animationName);
    if (it == m_keyframesRuleMap.end())
        return 0;

    return it->value.get();
}

void ScopedStyleResolver::addKeyframeStyle(PassRefPtr<StyleRuleKeyframes> rule)
{
    AtomicString s(rule->name());
    if (rule->isVendorPrefixed()) {
        KeyframesRuleMap::iterator it = m_keyframesRuleMap.find(rule->name().impl());
        if (it == m_keyframesRuleMap.end())
            m_keyframesRuleMap.set(s.impl(), rule);
        else if (it->value->isVendorPrefixed())
            m_keyframesRuleMap.set(s.impl(), rule);
    } else {
        m_keyframesRuleMap.set(s.impl(), rule);
    }
}

inline RuleSet* ScopedStyleResolver::atHostRuleSetFor(const ShadowRoot* shadowRoot) const
{
    HashMap<const ShadowRoot*, OwnPtr<RuleSet> >::const_iterator it = m_atHostRules.find(shadowRoot);
    return it != m_atHostRules.end() ? it->value.get() : 0;
}

void ScopedStyleResolver::matchHostRules(ElementRuleCollector& collector, bool includeEmptyRules)
{
    // FIXME: Determine tree position.
    CascadeScope cascadeScope = ignoreCascadeScope;

    if (m_atHostRules.isEmpty() || !m_scopingNode.isElementNode())
        return;

    ElementShadow* shadow = toElement(m_scopingNode).shadow();
    if (!shadow)
        return;

    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastAuthorRule = collector.matchedResult().matchedProperties.size() - 1;

    // FIXME(99827): https://bugs.webkit.org/show_bug.cgi?id=99827
    // add a new flag to ElementShadow and cache whether any @host @-rules are
    // applied to the element or not. So we can quickly exit this method
    // by using the flag.
    ShadowRoot* shadowRoot = shadow->youngestShadowRoot();
    for (; shadowRoot; shadowRoot = shadowRoot->olderShadowRoot())
        if (!shadowRoot->containsShadowElements())
            break;
    // All shadow roots have <shadow>.
    if (!shadowRoot)
        shadowRoot = shadow->oldestShadowRoot();

    RuleRange ruleRange = collector.matchedResult().ranges.authorRuleRange();
    SelectorChecker::BehaviorAtBoundary boundary = static_cast<SelectorChecker::BehaviorAtBoundary>(SelectorChecker::DoesNotCrossBoundary | SelectorChecker::ScopeContainsLastMatchedElement);
    for (; shadowRoot; shadowRoot = shadowRoot->youngerShadowRoot()) {
        if (RuleSet* ruleSet = atHostRuleSetFor(shadowRoot))
            collector.collectMatchingRules(MatchRequest(ruleSet, includeEmptyRules, &m_scopingNode), ruleRange, boundary, cascadeScope);
    }

    collector.sortAndTransferMatchedRules();
}

void ScopedStyleResolver::collectMatchingAuthorRules(ElementRuleCollector& collector, bool includeEmptyRules, bool applyAuthorStyles, CascadeScope cascadeScope, CascadeOrder cascadeOrder)
{
    if (!m_authorStyle)
        return;

    const ContainerNode* scopingNode = &m_scopingNode;
    unsigned behaviorAtBoundary = SelectorChecker::DoesNotCrossBoundary;

    if (!applyAuthorStyles)
        behaviorAtBoundary |= SelectorChecker::ScopeContainsLastMatchedElement;

    if (m_scopingNode.isShadowRoot()) {
        scopingNode = toShadowRoot(m_scopingNode).host();
        behaviorAtBoundary |= SelectorChecker::ScopeIsShadowHost;
    }

    MatchRequest matchRequest(m_authorStyle.get(), includeEmptyRules, scopingNode, applyAuthorStyles);
    RuleRange ruleRange = collector.matchedResult().ranges.authorRuleRange();
    collector.collectMatchingRules(matchRequest, ruleRange, static_cast<SelectorChecker::BehaviorAtBoundary>(behaviorAtBoundary), cascadeScope, cascadeOrder);
    collector.collectMatchingRulesForRegion(matchRequest, ruleRange, static_cast<SelectorChecker::BehaviorAtBoundary>(behaviorAtBoundary), cascadeScope, cascadeOrder);
}

void ScopedStyleResolver::matchPageRules(PageRuleCollector& collector)
{
    // Only consider the global author RuleSet for @page rules, as per the HTML5 spec.
    ASSERT(m_scopingNode.isDocumentNode());
    collector.matchPageRules(m_authorStyle.get());
}

void ScopedStyleResolver::collectViewportRulesTo(StyleResolver* resolver) const
{
    // Only consider the global author RuleSet for @viewport rules.
    if (!m_scopingNode.isDocumentNode() || !m_authorStyle)
        return;
    resolver->viewportStyleResolver()->collectViewportRules(m_authorStyle.get(), ViewportStyleResolver::AuthorOrigin);
}

} // namespace WebCore
