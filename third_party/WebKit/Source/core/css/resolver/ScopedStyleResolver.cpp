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
#include "core/css/ElementRuleCollector.h"
#include "core/css/PageRuleCollector.h"
#include "core/css/RuleFeature.h"
#include "core/css/RuleSet.h"
#include "core/css/StyleRule.h"
#include "core/dom/Document.h"
#include "core/dom/WebCoreMemoryInstrumentation.h"
#include "core/dom/shadow/ContentDistributor.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLStyleElement.h"
#include "wtf/MemoryInstrumentationHashMap.h"

namespace WebCore {

ScopedStyleResolver* ScopedStyleTree::ensureScopedStyleResolver(const ContainerNode* scopingNode)
{
    ASSERT(scopingNode);
    bool isNewEntry;
    ScopedStyleResolver* scopedStyleResolver = addScopedStyleResolver(scopingNode, isNewEntry);
    if (isNewEntry)
        setupScopedStylesTree(scopedStyleResolver);
    return scopedStyleResolver;
}

ScopedStyleResolver* ScopedStyleTree::scopedStyleResolverFor(const ContainerNode* scopingNode)
{
    if (!scopingNode->hasScopedHTMLStyleChild()
        && !(scopingNode->isElementNode() && toElement(scopingNode)->shadow())
        && !scopingNode->isDocumentNode()
        && !scopingNode->isShadowRoot())
        return 0;
    HashMap<const ContainerNode*, OwnPtr<ScopedStyleResolver> >::iterator it = m_authorStyles.find(scopingNode);
    return it != m_authorStyles.end() ? it->value.get() : 0;
}

ScopedStyleResolver* ScopedStyleTree::addScopedStyleResolver(const ContainerNode* scopingNode, bool& isNewEntry)
{
    HashMap<const ContainerNode*, OwnPtr<ScopedStyleResolver> >::AddResult addResult = m_authorStyles.add(scopingNode, nullptr);

    if (addResult.isNewEntry) {
        addResult.iterator->value = ScopedStyleResolver::create(scopingNode);
        if (!scopingNode || scopingNode->isDocumentNode())
            m_scopedResolverForDocument = addResult.iterator->value.get();
    }
    isNewEntry = addResult.isNewEntry;
    return addResult.iterator->value.get();
}

void ScopedStyleTree::setupScopedStylesTree(ScopedStyleResolver* target)
{
    ASSERT(target);
    ASSERT(target->scopingNode());

    // Since StyleResolver creates RuleSets according to styles' document
    // order, a parent of the given ScopedRuleData has been already
    // prepared.
    const ContainerNode* e = target->scopingNode()->parentOrShadowHostNode();
    for (; e; e = e->parentOrShadowHostNode()) {
        if (ScopedStyleResolver* scopedResolver = scopedStyleResolverFor(e)) {
            target->setParent(scopedResolver);
            break;
        }
        if (e->isDocumentNode()) {
            bool dummy;
            ScopedStyleResolver* scopedResolver = addScopedStyleResolver(e, dummy);
            target->setParent(scopedResolver);
            setupScopedStylesTree(scopedResolver);
            break;
        }
    }
}

void ScopedStyleTree::clear()
{
    m_authorStyles.clear();
    m_scopedResolverForDocument = 0;
    m_cache.clear();
}

void ScopedStyleTree::resolveScopedStyles(const Element* element, Vector<ScopedStyleResolver*, 8>& resolvers)
{
    for (ScopedStyleResolver* scopedResolver = scopedResolverFor(element); scopedResolver; scopedResolver = scopedResolver->parent())
        resolvers.append(scopedResolver);
}

inline ScopedStyleResolver* ScopedStyleTree::enclosingScopedStyleResolverFor(const ContainerNode* scopingNode)
{
    for (; scopingNode; scopingNode = scopingNode->parentOrShadowHostNode()) {
        if (ScopedStyleResolver* scopedStyleResolver = scopedStyleResolverFor(scopingNode))
            return scopedStyleResolver;
    }
    return 0;
}

void ScopedStyleTree::resolveStyleCache(const ContainerNode* scopingNode)
{
    m_cache.scopedResolver = enclosingScopedStyleResolverFor(scopingNode);
    m_cache.nodeForScopedStyles = scopingNode;
}

void ScopedStyleTree::pushStyleCache(const ContainerNode* scopingNode, const ContainerNode* parent)
{
    if (m_authorStyles.isEmpty())
        return;

    if (!cacheIsValid(parent)) {
        resolveStyleCache(scopingNode);
        return;
    }

    ScopedStyleResolver* scopedResolver = scopedStyleResolverFor(scopingNode);
    if (scopedResolver)
        m_cache.scopedResolver = scopedResolver;
    m_cache.nodeForScopedStyles = scopingNode;
}

void ScopedStyleTree::popStyleCache(const ContainerNode* scopingNode)
{
    if (!cacheIsValid(scopingNode))
        return;

    if (m_cache.scopedResolver && m_cache.scopedResolver->scopingNode() == scopingNode)
        m_cache.scopedResolver = m_cache.scopedResolver->parent();
    m_cache.nodeForScopedStyles = scopingNode->parentOrShadowHostNode();
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

const ContainerNode* ScopedStyleResolver::scopingNodeFor(const CSSStyleSheet* sheet)
{
    ASSERT(sheet);

    Document* document = sheet->ownerDocument();
    if (!document)
        return 0;
    Node* ownerNode = sheet->ownerNode();
    if (!ownerNode || !ownerNode->hasTagName(HTMLNames::styleTag))
        return 0;

    HTMLStyleElement* styleElement = toHTMLStyleElement(ownerNode);
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
    m_authorStyle->addRulesFromSheet(sheet, medium, resolver, m_scopingNode);
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
    if (m_atHostRules.isEmpty() || !m_scopingNode->isElementNode())
        return;

    ElementShadow* shadow = toElement(m_scopingNode)->shadow();
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
    for (; shadowRoot; shadowRoot = shadowRoot->youngerShadowRoot()) {
        if (RuleSet* ruleSet = atHostRuleSetFor(shadowRoot))
            collector.collectMatchingRules(MatchRequest(ruleSet, includeEmptyRules, m_scopingNode), ruleRange);
    }

    collector.sortAndTransferMatchedRules();
}

void ScopedStyleResolver::matchAuthorRules(ElementRuleCollector& collector, bool includeEmptyRules, bool applyAuthorStyles)
{
    if (m_authorStyle) {
        collector.clearMatchedRules();
        collector.matchedResult().ranges.lastAuthorRule = collector.matchedResult().matchedProperties.size() - 1;

        // Match author rules.
        MatchRequest matchRequest(m_authorStyle.get(), includeEmptyRules, m_scopingNode);
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
    ASSERT(m_scopingNode->isDocumentNode());
    collector.matchPageRules(m_authorStyle.get());
}

void ScopedStyleResolver::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_authorStyle, "authorStyle");
    info.addMember(m_atHostRules, "atHostRules");
}

} // namespace WebCore
