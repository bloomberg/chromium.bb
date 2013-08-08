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

#ifndef ScopedStyleResolver_h
#define ScopedStyleResolver_h

#include "core/css/CSSKeyframesRule.h"
#include "core/css/ElementRuleCollector.h"
#include "core/css/RuleSet.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/Element.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class ElementRuleCollector;
class MediaQueryEvaluator;
class PageRuleCollector;
class ShadowRoot;
class StyleSheetContents;

// This class selects a RenderStyle for a given element based on a collection of stylesheets.
class ScopedStyleResolver {
    WTF_MAKE_NONCOPYABLE(ScopedStyleResolver); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<ScopedStyleResolver> create(const ContainerNode* scopingNode) { return adoptPtr(new ScopedStyleResolver(scopingNode)); }

    static const ContainerNode* scopingNodeFor(const CSSStyleSheet*);

    const ContainerNode* scopingNode() const { return m_scopingNode; }
    const TreeScope* treeScope() const { return m_scopingNode->treeScope(); }
    void prepareEmptyRuleSet() { m_authorStyle = RuleSet::create(); }
    void setParent(ScopedStyleResolver* newParent) { m_parent = newParent; }
    ScopedStyleResolver* parent() { return m_parent; }

    bool hasOnlyEmptyRuleSets() const { return !m_authorStyle->ruleCount() && m_atHostRules.isEmpty(); }

public:
    bool checkRegionStyle(Element*);
    const StyleRuleKeyframes* keyframeStylesForAnimation(const StringImpl* animationName);
    void addKeyframeStyle(PassRefPtr<StyleRuleKeyframes>);

    void matchHostRules(ElementRuleCollector&, bool includeEmptyRules);
    void matchAuthorRules(ElementRuleCollector&, bool includeEmptyRules, bool applyAuthorStyles);
    void collectMatchingAuthorRules(ElementRuleCollector&, bool includeEmptyRules, bool applyAuthorStyles, CascadeScope, CascadeOrder = ignoreCascadeOrder);
    void matchPageRules(PageRuleCollector&);
    void addRulesFromSheet(StyleSheetContents*, const MediaQueryEvaluator&, StyleResolver*);
    void addHostRule(StyleRuleHost*, bool hasDocumentSecurityOrigin, const ContainerNode* scopingNode);
    void collectFeaturesTo(RuleFeatureSet&);
    void resetAuthorStyle();
    void resetAtHostRules(const ShadowRoot*);
    void collectViewportRulesTo(StyleResolver*) const;

private:
    ScopedStyleResolver() : m_scopingNode(0), m_parent(0) { }
    ScopedStyleResolver(const ContainerNode* scopingNode) : m_scopingNode(scopingNode), m_parent(0) { }

    RuleSet* ensureAtHostRuleSetFor(const ShadowRoot*);
    RuleSet* atHostRuleSetFor(const ShadowRoot*) const;

    const ContainerNode* m_scopingNode;
    ScopedStyleResolver* m_parent;

    OwnPtr<RuleSet> m_authorStyle;
    HashMap<const ShadowRoot*, OwnPtr<RuleSet> > m_atHostRules;

    typedef HashMap<const StringImpl*, RefPtr<StyleRuleKeyframes> > KeyframesRuleMap;
    KeyframesRuleMap m_keyframesRuleMap;
};

class ScopedStyleTree {
    WTF_MAKE_NONCOPYABLE(ScopedStyleTree); WTF_MAKE_FAST_ALLOCATED;
public:
    ScopedStyleTree() : m_scopedResolverForDocument(0), m_buildInDocumentOrder(true) { }

    ScopedStyleResolver* ensureScopedStyleResolver(const ContainerNode* scopingNode);
    ScopedStyleResolver* scopedStyleResolverFor(const ContainerNode* scopingNode);
    ScopedStyleResolver* addScopedStyleResolver(const ContainerNode* scopingNode, bool& isNewEntry);
    void clear();

    // for fast-path.
    bool hasOnlyScopedResolverForDocument() const { return m_scopedResolverForDocument && m_authorStyles.size() == 1; }
    ScopedStyleResolver* scopedStyleResolverForDocument() { return m_scopedResolverForDocument; }

    void resolveScopedStyles(const Element*, Vector<ScopedStyleResolver*, 8>&);
    void collectScopedResolversForHostedShadowTrees(const Element*, Vector<ScopedStyleResolver*, 8>&);
    void resolveScopedKeyframesRules(const Element*, Vector<ScopedStyleResolver*, 8>&);
    ScopedStyleResolver* scopedResolverFor(const Element*);

    void remove(const ContainerNode* scopingNode);

    void pushStyleCache(const ContainerNode* scopingNode, const ContainerNode* parent);
    void popStyleCache(const ContainerNode* scopingNode);

    void collectFeaturesTo(RuleFeatureSet& features);
    void setBuildInDocumentOrder(bool enabled) { m_buildInDocumentOrder = enabled; }
    bool buildInDocumentOrder() const { return m_buildInDocumentOrder; }

private:
    void setupScopedStylesTree(ScopedStyleResolver* target);

    bool cacheIsValid(const ContainerNode* parent) const { return parent && parent == m_cache.nodeForScopedStyles; }
    void resolveStyleCache(const ContainerNode* scopingNode);
    ScopedStyleResolver* enclosingScopedStyleResolverFor(const ContainerNode* scopingNode);

    void reparentNodes(const ScopedStyleResolver* oldParent, ScopedStyleResolver* newParent);

private:
    HashMap<const ContainerNode*, OwnPtr<ScopedStyleResolver> > m_authorStyles;
    ScopedStyleResolver* m_scopedResolverForDocument;
    bool m_buildInDocumentOrder;

    struct ScopedStyleCache {
        ScopedStyleResolver* scopedResolver;
        const ContainerNode* nodeForScopedStyles;

        void clear()
        {
            scopedResolver = 0;
            nodeForScopedStyles = 0;
        }
    };
    ScopedStyleCache m_cache;
};

inline ScopedStyleResolver* ScopedStyleTree::scopedResolverFor(const Element* element)
{
    if (!cacheIsValid(element))
        resolveStyleCache(element);

    return m_cache.scopedResolver;
}

} // namespace WebCore

#endif // ScopedStyleResolver_h
