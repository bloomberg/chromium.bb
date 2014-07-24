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
#include "core/css/resolver/ScopedStyleTree.h"

#include "core/css/RuleFeature.h"
#include "core/dom/Document.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"

namespace blink {

class StyleSheetContents;

ScopedStyleTree::ScopedStyleTree()
{
}

ScopedStyleTree::~ScopedStyleTree()
{
    for (HashMap<const ContainerNode*, ScopedStyleResolver*>::iterator it = m_authorStyles.begin(); it != m_authorStyles.end(); ++it)
        it->key->treeScope().clearScopedStyleResolver();
}

ScopedStyleResolver* ScopedStyleTree::ensureScopedStyleResolver(ContainerNode& scopingNode)
{
    ASSERT(scopingNode.isShadowRoot() || scopingNode.isDocumentNode());

    m_authorStyles.add(&scopingNode, &scopingNode.treeScope().ensureScopedStyleResolver());
    return scopingNode.treeScope().scopedStyleResolver();
}

ScopedStyleResolver* ScopedStyleTree::scopedStyleResolverFor(const ContainerNode& scopingNode)
{
    if (!isShadowHost(&scopingNode)
        && !scopingNode.isDocumentNode()
        && !scopingNode.isShadowRoot())
        return 0;
    return scopingNode.treeScope().scopedStyleResolver();
}

void ScopedStyleTree::resolveScopedStyles(const Element* element, Vector<ScopedStyleResolver*, 8>& resolvers)
{
    for (ScopedStyleResolver* scopedResolver = scopedResolverFor(element); scopedResolver; scopedResolver = scopedResolver->parent())
        resolvers.append(scopedResolver);
}

void ScopedStyleTree::collectScopedResolversForHostedShadowTrees(const Element* element, Vector<ScopedStyleResolver*, 8>& resolvers)
{
    ElementShadow* shadow = element->shadow();
    if (!shadow)
        return;

    // Adding scoped resolver for active shadow roots for shadow host styling.
    for (ShadowRoot* shadowRoot = shadow->youngestShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot()) {
        if (shadowRoot->numberOfStyles() > 0) {
            if (ScopedStyleResolver* resolver = scopedStyleResolverFor(*shadowRoot))
                resolvers.append(resolver);
        }
    }
}

void ScopedStyleTree::resolveScopedKeyframesRules(const Element* element, Vector<ScopedStyleResolver*, 8>& resolvers)
{
    Document& document = element->document();
    TreeScope& treeScope = element->treeScope();
    bool applyAuthorStyles = treeScope.applyAuthorStyles();

    // Add resolvers for shadow roots hosted by the given element.
    collectScopedResolversForHostedShadowTrees(element, resolvers);

    // Add resolvers while walking up DOM tree from the given element.
    for (ScopedStyleResolver* scopedResolver = scopedResolverFor(element); scopedResolver; scopedResolver = scopedResolver->parent()) {
        if (scopedResolver->treeScope() == treeScope || (applyAuthorStyles && scopedResolver->treeScope() == document))
            resolvers.append(scopedResolver);
    }
}

inline ScopedStyleResolver* ScopedStyleTree::enclosingScopedStyleResolverFor(const ContainerNode* scopingNode)
{
    for (; scopingNode; scopingNode = scopingNode->parentOrShadowHostNode()) {
        if (ScopedStyleResolver* scopedStyleResolver = scopedStyleResolverFor(*scopingNode))
            return scopedStyleResolver;
    }
    return 0;
}

void ScopedStyleTree::resolveStyleCache(const ContainerNode* scopingNode)
{
    m_cache.scopedResolver = enclosingScopedStyleResolverFor(scopingNode);
    m_cache.nodeForScopedStyles = scopingNode;
}

void ScopedStyleTree::pushStyleCache(const ContainerNode& scopingNode, const ContainerNode* parent)
{
    if (m_authorStyles.isEmpty())
        return;

    if (!cacheIsValid(parent)) {
        resolveStyleCache(&scopingNode);
        return;
    }

    ScopedStyleResolver* scopedResolver = scopedStyleResolverFor(scopingNode);
    if (scopedResolver)
        m_cache.scopedResolver = scopedResolver;
    m_cache.nodeForScopedStyles = &scopingNode;
}

void ScopedStyleTree::popStyleCache(const ContainerNode& scopingNode)
{
    if (!cacheIsValid(&scopingNode))
        return;

    if (m_cache.scopedResolver && m_cache.scopedResolver->scopingNode() == scopingNode)
        m_cache.scopedResolver = m_cache.scopedResolver->parent();
    m_cache.nodeForScopedStyles = scopingNode.parentOrShadowHostNode();
}

void ScopedStyleTree::collectFeaturesTo(RuleFeatureSet& features)
{
    HashSet<const StyleSheetContents*> visitedSharedStyleSheetContents;
    for (HashMap<const ContainerNode*, ScopedStyleResolver*>::iterator it = m_authorStyles.begin(); it != m_authorStyles.end(); ++it)
        it->value->collectFeaturesTo(features, visitedSharedStyleSheetContents);
}

void ScopedStyleTree::remove(const ContainerNode* scopingNode)
{
    if (!scopingNode || scopingNode->isDocumentNode())
        return;

    ScopedStyleResolver* resolver = scopingNode->treeScope().scopedStyleResolver();
    if (!resolver)
        return;

    if (m_cache.scopedResolver == resolver)
        m_cache.clear();

    // resolver is going to be freed below.
    resolver = 0;
    m_authorStyles.remove(scopingNode);
    scopingNode->treeScope().clearScopedStyleResolver();
}

} // namespace blink
