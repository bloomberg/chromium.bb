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

#include "core/css/RuleSet.h"
#include "core/css/SiblingTraversalStrategies.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class ContainerNode;
class ElementRuleCollector;
class MediaQueryEvaluator;
class PageRuleCollector;
class ScopedStyleResolver;
class ShadowRoot;
class StyleSheetContents;

// This class selects a RenderStyle for a given element based on a collection of stylesheets.
class ScopedStyleResolver {
    WTF_MAKE_NONCOPYABLE(ScopedStyleResolver); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<ScopedStyleResolver> create(const ContainerNode* scopingNode) { return adoptPtr(new ScopedStyleResolver(scopingNode)); }

    static const ContainerNode* scopingNodeFor(const CSSStyleSheet*);

    // methods for building tree.
    const ContainerNode* scopingNode() const { return m_scopingNode; }
    const TreeScope* treeScope() const { return m_scopingNode->treeScope(); }
    void prepareEmptyRuleSet() { m_authorStyle = RuleSet::create(); }
    void setParent(ScopedStyleResolver* newParent) { m_parent = newParent; }
    ScopedStyleResolver* parent() { return m_parent; }

public:
    bool checkRegionStyle(Element*);

    void matchHostRules(ElementRuleCollector&, bool includeEmptyRules);
    void matchAuthorRules(ElementRuleCollector&, bool includeEmptyRules, bool applyAuthorStyles);
    void matchPageRules(PageRuleCollector&);
    void addRulesFromSheet(StyleSheetContents*, const MediaQueryEvaluator&, StyleResolver*);
    void addHostRule(StyleRuleHost*, bool hasDocumentSecurityOrigin, const ContainerNode* scopingNode);
    void collectFeaturesTo(RuleFeatureSet&);
    void resetAuthorStyle();
    void reportMemoryUsage(MemoryObjectInfo*) const;

private:
    ScopedStyleResolver() : m_scopingNode(0), m_parent(0) { }
    ScopedStyleResolver(const ContainerNode* scopingNode) : m_scopingNode(scopingNode), m_parent(0) { }

    RuleSet* ensureAtHostRuleSetFor(const ShadowRoot*);
    RuleSet* atHostRuleSetFor(const ShadowRoot*) const;

    const ContainerNode* m_scopingNode;
    ScopedStyleResolver* m_parent;

    OwnPtr<RuleSet> m_authorStyle;
    HashMap<const ShadowRoot*, OwnPtr<RuleSet> > m_atHostRules;
};

class ScopedStyleTree {
    WTF_MAKE_NONCOPYABLE(ScopedStyleTree); WTF_MAKE_FAST_ALLOCATED;
public:
    ScopedStyleTree() : m_scopedResolverForDocument(0) { }

    ScopedStyleResolver* ensureScopedStyleResolver(const ContainerNode* scopingNode);
    ScopedStyleResolver* scopedStyleResolverFor(const ContainerNode* scopingNode);
    ScopedStyleResolver* addScopedStyleResolver(const ContainerNode* scopingNode, bool& isNewEntry);
    void clear();

    // for fast-path.
    bool hasOnlyScopedResolverForDocument() const { return m_scopedResolverForDocument && m_authorStyles.size() == 1; }
    ScopedStyleResolver* scopedStyleResolverForDocument() { return m_scopedResolverForDocument; }

    void resolveScopedStyles(const Element*, Vector<ScopedStyleResolver*, 8>&);
    ScopedStyleResolver* scopedResolverFor(const Element*);

    void pushStyleCache(const ContainerNode* scopingNode, const ContainerNode* parent);
    void popStyleCache(const ContainerNode* scopingNode);

    void collectFeaturesTo(RuleFeatureSet& features);

    void reportMemoryUsage(MemoryObjectInfo*) const;
private:
    void setupScopedStylesTree(ScopedStyleResolver* target);

    bool cacheIsValid(const ContainerNode* parent) const { return parent && parent == m_cache.nodeForScopedStyles; }
    void resolveStyleCache(const ContainerNode* scopingNode);
    ScopedStyleResolver* enclosingScopedStyleResolverFor(const ContainerNode* scopingNode);

private:
    HashMap<const ContainerNode*, OwnPtr<ScopedStyleResolver> > m_authorStyles;
    ScopedStyleResolver* m_scopedResolverForDocument;

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
