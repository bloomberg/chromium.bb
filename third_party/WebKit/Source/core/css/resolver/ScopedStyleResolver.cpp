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
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/ElementRuleCollector.h"
#include "core/css/PageRuleCollector.h"
#include "core/css/RuleFeature.h"
#include "core/css/RuleSet.h"
#include "core/css/StyleRule.h"
#include "core/dom/ContextFeatures.h"
#include "core/dom/Document.h"
#include "core/dom/WebCoreMemoryInstrumentation.h"
#include "core/dom/shadow/ContentDistributor.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLStyleElement.h"
#include "wtf/MemoryInstrumentationHashMap.h"
#include "wtf/MemoryInstrumentationHashSet.h"

namespace WebCore {

ScopedStyleResolver* ScopedStyleTree::ensureScopedStyleResolver(const ContainerNode* scope)
{
    ASSERT(scope);
    bool isNewEntry;
    ScopedStyleResolver* scopeStyleResolver = addScopedStyleResolver(scope, isNewEntry);
    if (isNewEntry)
        setupScopeStylesTree(scopeStyleResolver);
    return scopeStyleResolver;
}

ScopedStyleResolver* ScopedStyleTree::scopedStyleResolverFor(const ContainerNode* scope)
{
    if (!scope->hasScopedHTMLStyleChild()
        && !(scope->isElementNode() && toElement(scope)->shadow())
        && !scope->isDocumentNode()
        && !scope->isShadowRoot())
        return 0;
    HashMap<const ContainerNode*, OwnPtr<ScopedStyleResolver> >::iterator it = m_authorStyles.find(scope);
    return it != m_authorStyles.end() ? it->value.get() : 0;
}

ScopedStyleResolver* ScopedStyleTree::addScopedStyleResolver(const ContainerNode* scope, bool& isNewEntry)
{
    HashMap<const ContainerNode*, OwnPtr<ScopedStyleResolver> >::AddResult addResult = m_authorStyles.add(scope, nullptr);

    if (addResult.isNewEntry) {
        addResult.iterator->value = ScopedStyleResolver::create(scope);
        if (!scope || scope->isDocumentNode())
            m_scopeResolverForDocument = addResult.iterator->value.get();
    }
    isNewEntry = addResult.isNewEntry;
    return addResult.iterator->value.get();
}

void ScopedStyleTree::setupScopeStylesTree(ScopedStyleResolver* target)
{
    ASSERT(target);
    ASSERT(target->scope());

    // Since StyleResolver creates RuleSets according to styles' document
    // order, a parent of the given ScopedRuleData has been already
    // prepared.
    const ContainerNode* e = target->scope()->parentOrShadowHostNode();
    for (; e; e = e->parentOrShadowHostNode()) {
        if (ScopedStyleResolver* scopeResolver = scopedStyleResolverFor(e)) {
            target->setParent(scopeResolver);
            break;
        }
        if (e->isShadowRoot() || e->isDocumentNode()) {
            bool dummy;
            ScopedStyleResolver* scopeResolver = addScopedStyleResolver(e, dummy);
            target->setParent(scopeResolver);
            setupScopeStylesTree(scopeResolver);
            break;
        }
    }
}

void ScopedStyleTree::clear()
{
    m_authorStyles.clear();
    m_scopeResolverForDocument = 0;
    m_cache.clear();
}

void ScopedStyleTree::resolveScopeStyles(const Element* element, Vector<std::pair<ScopedStyleResolver*, bool>, 8>& resolvers)
{
    ScopedStyleResolver* scopeResolver = scopedResolverFor(element);
    if (!scopeResolver)
        return;

    bool applyAuthorStylesOfElementTreeScope = element->treeScope()->applyAuthorStyles();
    bool applyAuthorStyles = m_cache.authorStyleBoundsIndex == m_cache.scopeResolverBoundsIndex ? applyAuthorStylesOfElementTreeScope : false;

    for ( ; scopeResolver; scopeResolver = scopeResolver->parent()) {
        resolvers.append(std::pair<ScopedStyleResolver*, bool>(scopeResolver, applyAuthorStyles));
        if (scopeResolver->scope()->isShadowRoot()) {
            if (scopeResolver->parent()->scope()->isInShadowTree())
                applyAuthorStyles = applyAuthorStyles && toShadowRoot(scopeResolver->scope())->applyAuthorStyles();
            else
                applyAuthorStyles = applyAuthorStylesOfElementTreeScope;
        }
    }
}

inline ScopedStyleResolver* ScopedStyleTree::enclosingScopedStyleResolverFor(const ContainerNode* scope, int& authorStyleBoundsIndex)
{
    for (; scope; scope = scope->parentOrShadowHostNode()) {
        if (ScopedStyleResolver* scopeStyleResolver = scopedStyleResolverFor(scope))
            return scopeStyleResolver;
        if (scope->isShadowRoot() && !toShadowRoot(scope)->applyAuthorStyles())
            --authorStyleBoundsIndex;
    }
    return 0;
}

void ScopedStyleTree::resolveStyleCache(const ContainerNode* scope)
{
    int authorStyleBoundsIndex = 0;
    m_cache.scopeResolver = enclosingScopedStyleResolverFor(scope, authorStyleBoundsIndex);
    m_cache.scopeResolverBoundsIndex = authorStyleBoundsIndex;
    m_cache.nodeForScopeStyles = scope;
    m_cache.authorStyleBoundsIndex = 0;
}

void ScopedStyleTree::pushStyleCache(const ContainerNode* scope, const ContainerNode* parent)
{
    if (m_authorStyles.isEmpty())
        return;

    if (!cacheIsValid(parent)) {
        resolveStyleCache(scope);
        return;
    }

    if (scope->isShadowRoot() && !toShadowRoot(scope)->applyAuthorStyles())
        ++m_cache.authorStyleBoundsIndex;

    ScopedStyleResolver* scopeResolver = scopedStyleResolverFor(scope);
    if (scopeResolver) {
        m_cache.scopeResolver = scopeResolver;
        m_cache.scopeResolverBoundsIndex = m_cache.authorStyleBoundsIndex;
    }
    m_cache.nodeForScopeStyles = scope;
}

void ScopedStyleTree::popStyleCache(const ContainerNode* scope)
{
    if (cacheIsValid(scope)) {
        bool needUpdateBoundsIndex = scope->isShadowRoot() && !toShadowRoot(scope)->applyAuthorStyles();

        if (m_cache.scopeResolver && m_cache.scopeResolver->scope() == scope) {
            m_cache.scopeResolver = m_cache.scopeResolver->parent();
            if (needUpdateBoundsIndex)
                --m_cache.scopeResolverBoundsIndex;
        }
        if (needUpdateBoundsIndex)
            --m_cache.authorStyleBoundsIndex;
        m_cache.nodeForScopeStyles = scope->parentOrShadowHostNode();
    }
}

void ScopedStyleTree::collectFeaturesTo(RuleFeatureSet& features)
{
    for (HashMap<const ContainerNode*, OwnPtr<ScopedStyleResolver> >::iterator it = m_authorStyles.begin(); it != m_authorStyles.end(); ++it)
        it->value->collectFeaturesTo(features);
}

void ScopedStyleTree::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_authorStyles, "authorStyles");
}

const ContainerNode* ScopedStyleResolver::scopeFor(const CSSStyleSheet* sheet)
{
    ASSERT(sheet);

    Document* document = sheet->ownerDocument();
    if (!document)
        return 0;
    Node* ownerNode = sheet->ownerNode();
    if (!ownerNode || !ownerNode->isHTMLElement() || !ownerNode->hasTagName(HTMLNames::styleTag))
        return 0;

    HTMLStyleElement* styleElement = static_cast<HTMLStyleElement*>(ownerNode);
    if (!styleElement->scoped())
        return styleElement->isInShadowTree() ? styleElement->containingShadowRoot() : 0;

    ContainerNode* parent = styleElement->parentNode();
    if (!parent)
        return 0;

    return (parent->isElementNode() || parent->isShadowRoot()) ? parent : 0;
}

void ScopedStyleResolver::addRulesFromSheet(StyleSheetContents* sheet, const MediaQueryEvaluator& medium, StyleResolver* resolver)
{
    if (!m_authorStyle)
        m_authorStyle = RuleSet::create();
    m_authorStyle->addRulesFromSheet(sheet, medium, resolver, m_scope);
}

inline RuleSet* ScopedStyleResolver::ensureAtHostRuleSetFor(const ShadowRoot* shadowRoot)
{
    HashMap<const ShadowRoot*, OwnPtr<RuleSet> >::AddResult addResult = m_atHostRules.add(shadowRoot, nullptr);
    if (addResult.isNewEntry)
        addResult.iterator->value = RuleSet::create();
    return addResult.iterator->value.get();
}

void ScopedStyleResolver::addHostRule(StyleRuleHost* hostRule, bool hasDocumentSecurityOrigin, const ContainerNode* scope)
{
    if (!scope)
        return;

    ShadowRoot* shadowRoot = scope->containingShadowRoot();
    if (!shadowRoot || !shadowRoot->host())
        return;

    RuleSet* rule = ensureAtHostRuleSetFor(shadowRoot);

    const Vector<RefPtr<StyleRuleBase> >& childRules = hostRule->childRules();
    AddRuleFlags addRuleFlags = hasDocumentSecurityOrigin ? RuleHasDocumentSecurityOrigin : RuleHasNoSpecialState;
    addRuleFlags = static_cast<AddRuleFlags>(addRuleFlags | RuleCanUseFastCheckSelector);
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
    m_authorStyle->disableAutoShrinkToFit();
    m_atHostRules.clear();
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

inline RuleSet* ScopedStyleResolver::atHostRuleSetFor(const ShadowRoot* shadowRoot) const
{
    HashMap<const ShadowRoot*, OwnPtr<RuleSet> >::const_iterator it = m_atHostRules.find(shadowRoot);
    return it != m_atHostRules.end() ? it->value.get() : 0;
}

void ScopedStyleResolver::matchHostRules(ElementRuleCollector& collector, bool includeEmptyRules)
{
    if (m_atHostRules.isEmpty() || !m_scope->isElementNode())
        return;

    ElementShadow* shadow = toElement(m_scope)->shadow();
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
        if (!ScopeContentDistribution::hasShadowElement(shadowRoot))
            break;
    // All shadow roots have <shadow>.
    if (!shadowRoot)
        shadowRoot = shadow->oldestShadowRoot();

    StyleResolver::RuleRange ruleRange = collector.matchedResult().ranges.authorRuleRange();
    collector.setBehaviorAtBoundary(static_cast<SelectorChecker::BehaviorAtBoundary>(SelectorChecker::DoesNotCrossBoundary | SelectorChecker::ScopeContainsLastMatchedElement));
    for (; shadowRoot; shadowRoot = shadowRoot->youngerShadowRoot())
        if (RuleSet* ruleSet = atHostRuleSetFor(shadowRoot))
            collector.collectMatchingRules(MatchRequest(ruleSet, includeEmptyRules, m_scope), ruleRange);

    collector.sortAndTransferMatchedRules();
}

void ScopedStyleResolver::matchAuthorRules(ElementRuleCollector& collector, bool includeEmptyRules, bool applyAuthorStyles)
{
    if (m_authorStyle) {
        collector.clearMatchedRules();
        collector.matchedResult().ranges.lastAuthorRule = collector.matchedResult().matchedProperties.size() - 1;

        // Match author rules.
        MatchRequest matchRequest(m_authorStyle.get(), includeEmptyRules, m_scope);
        StyleResolver::RuleRange ruleRange = collector.matchedResult().ranges.authorRuleRange();
        collector.setBehaviorAtBoundary(applyAuthorStyles ? SelectorChecker::DoesNotCrossBoundary : static_cast<SelectorChecker::BehaviorAtBoundary>(SelectorChecker::DoesNotCrossBoundary | SelectorChecker::ScopeContainsLastMatchedElement));
        collector.collectMatchingRules(matchRequest, ruleRange);
        collector.collectMatchingRulesForRegion(matchRequest, ruleRange);
        collector.sortAndTransferMatchedRules();
    }
}

void ScopedStyleResolver::matchPageRules(PageRuleCollector& collector)
{
    // Only consider the global author RuleSet for @page rules, as per the HTML5 spec.
    ASSERT(m_scope->isDocumentNode());
    collector.matchPageRules(m_authorStyle.get());
}

void ScopedStyleResolver::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_authorStyle, "authorStyle");
    info.addMember(m_atHostRules, "atHostRules");
}

} // namespace WebCore
