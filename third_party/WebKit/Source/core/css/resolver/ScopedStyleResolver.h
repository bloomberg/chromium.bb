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

#include "core/css/CSSRuleList.h"
#include "core/css/CSSToStyleMap.h"
#include "core/css/CSSValueList.h"
#include "core/css/DocumentRuleSets.h"
#include "core/css/InspectorCSSOMWrappers.h"
#include "core/css/MediaQueryExp.h"
#include "core/css/RuleFeature.h"
#include "core/css/RuleSet.h"
#include "core/css/SelectorChecker.h"
#include "core/css/SelectorFilter.h"
#include "core/css/SiblingTraversalStrategies.h"
#if ENABLE(SVG)
#include "core/css/WebKitCSSSVGDocumentValue.h"
#endif
#include "core/css/WebKitCSSKeyframeRule.h"
#include "core/css/WebKitCSSKeyframesRule.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/platform/LinkHash.h"
#include "core/platform/ScrollTypes.h"
#include "core/platform/graphics/filters/custom/CustomFilterConstants.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/StyleInheritedData.h"
#include "wtf/Assertions.h"
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
    static PassOwnPtr<ScopedStyleResolver> create(const ContainerNode* scope) { return adoptPtr(new ScopedStyleResolver(scope)); }

    static const ContainerNode* scopeFor(const CSSStyleSheet*);

    // methods for building tree.
    const ContainerNode* scope() const { return m_scope; }
    const TreeScope* treeScope() const { return m_scope->treeScope(); }
    void prepareEmptyRuleSet() { m_authorStyle = RuleSet::create(); }
    void setParent(ScopedStyleResolver* newParent) { m_parent = newParent; }
    ScopedStyleResolver* parent() { return m_parent; }

public:
    bool checkRegionStyle(Element*);

    void matchHostRules(ElementRuleCollector&, bool includeEmptyRules);
    void matchAuthorRules(ElementRuleCollector&, bool includeEmptyRules, bool applyAuthorStyles);
    void matchPageRules(PageRuleCollector&);
    void addRulesFromSheet(StyleSheetContents*, const MediaQueryEvaluator&, StyleResolver*);
    void postAddRulesFromSheet() { m_authorStyle->shrinkToFit(); }
    void addHostRule(StyleRuleHost*, bool hasDocumentSecurityOrigin, const ContainerNode* scope);
    void collectFeaturesTo(RuleFeatureSet&);
    void resetAuthorStyle();
    void reportMemoryUsage(MemoryObjectInfo*) const;

private:
    ScopedStyleResolver() : m_scope(0), m_parent(0) { }
    ScopedStyleResolver(const ContainerNode* scope) : m_scope(scope), m_parent(0) { }

    RuleSet* ensureAtHostRuleSetFor(const ShadowRoot*);
    RuleSet* atHostRuleSetFor(const ShadowRoot*) const;

    const ContainerNode* m_scope;
    ScopedStyleResolver* m_parent;

    OwnPtr<RuleSet> m_authorStyle;
    HashMap<const ShadowRoot*, OwnPtr<RuleSet> > m_atHostRules;
};

class ScopedStyleTree {
    WTF_MAKE_NONCOPYABLE(ScopedStyleTree); WTF_MAKE_FAST_ALLOCATED;
public:
    ScopedStyleTree() : m_scopeResolverForDocument(0) { }

    ScopedStyleResolver* ensureScopedStyleResolver(const ContainerNode* scope);
    ScopedStyleResolver* scopedStyleResolverFor(const ContainerNode* scope);
    ScopedStyleResolver* addScopedStyleResolver(const ContainerNode* scope, bool& isNewEntry);
    void clear();

    // for fast-path.
    bool hasOnlyScopeResolverForDocument() const { return m_scopeResolverForDocument && m_authorStyles.size() == 1; }
    ScopedStyleResolver* scopedStyleResolverForDocument() { return m_scopeResolverForDocument; }

    void resolveScopeStyles(const Element*, Vector<std::pair<ScopedStyleResolver*, bool>, 8>& resolvers);
    ScopedStyleResolver* scopedResolverFor(const Element*);

    void pushStyleCache(const ContainerNode* scope, const ContainerNode* parent);
    void popStyleCache(const ContainerNode* scope);

    void collectFeaturesTo(RuleFeatureSet& features);

    void reportMemoryUsage(MemoryObjectInfo*) const;
private:
    void setupScopeStylesTree(ScopedStyleResolver* target);

    bool cacheIsValid(const ContainerNode* parent) const { return parent && parent == m_cache.nodeForScopeStyles; }
    void resolveStyleCache(const ContainerNode* scope);
    ScopedStyleResolver* enclosingScopedStyleResolverFor(const ContainerNode* scope, int& authorStyleBoundsIndex);

private:
    HashMap<const ContainerNode*, OwnPtr<ScopedStyleResolver> > m_authorStyles;
    ScopedStyleResolver* m_scopeResolverForDocument;

    struct ScopeStyleCache {
        ScopedStyleResolver* scopeResolver;
        int scopeResolverBoundsIndex;
        const ContainerNode* nodeForScopeStyles;
        int authorStyleBoundsIndex;

        void clear()
        {
            scopeResolver = 0;
            scopeResolverBoundsIndex = 0;
            nodeForScopeStyles = 0;
            authorStyleBoundsIndex = 0;
        }
    };
    ScopeStyleCache m_cache;
};

inline ScopedStyleResolver* ScopedStyleTree::scopedResolverFor(const Element* element)
{
    if (!cacheIsValid(element))
        resolveStyleCache(element);

    return m_cache.scopeResolver;
}

} // namespace WebCore

#endif // ScopedStyleResolver_h
