/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/shadow/ElementShadow.h"

#include "core/dom/ContainerNodeAlgorithms.h"

namespace WebCore {

ShadowRoot* ElementShadow::addShadowRoot(Element* shadowHost, ShadowRoot::ShadowRootType type)
{
    RefPtr<ShadowRoot> shadowRoot = ShadowRoot::create(shadowHost->document(), type);

    shadowRoot->setParentOrShadowHostNode(shadowHost);
    shadowRoot->setParentTreeScope(shadowHost->treeScope());
    m_shadowRoots.push(shadowRoot.get());
    ChildNodeInsertionNotifier(shadowHost).notify(shadowRoot.get());
    setNeedsDistributionRecalc();

    // addShadowRoot() affects apply-author-styles. However, we know that the youngest shadow root has not had any children yet.
    // The youngest shadow root's apply-author-styles is default (false). So we can just set m_applyAuthorStyles false.
    m_applyAuthorStyles = false;

    if (shadowHost->attached())
        shadowHost->lazyReattach();

    InspectorInstrumentation::didPushShadowRoot(shadowHost, shadowRoot.get());

    return shadowRoot.get();
}

void ElementShadow::removeAllShadowRoots()
{
    // Dont protect this ref count.
    Element* shadowHost = host();

    while (RefPtr<ShadowRoot> oldRoot = m_shadowRoots.head()) {
        InspectorInstrumentation::willPopShadowRoot(shadowHost, oldRoot.get());
        shadowHost->document()->removeFocusedElementOfSubtree(oldRoot.get());

        if (oldRoot->attached())
            oldRoot->detach();

        m_shadowRoots.removeHead();
        oldRoot->setParentOrShadowHostNode(0);
        oldRoot->setParentTreeScope(shadowHost->document());
        oldRoot->setPrev(0);
        oldRoot->setNext(0);
        ChildNodeRemovalNotifier(shadowHost).notify(oldRoot.get());
    }
}

void ElementShadow::attach(const Node::AttachContext& context)
{
    Node::AttachContext childrenContext(context);
    childrenContext.resolvedStyle = 0;

    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (!root->attached())
            root->attach(childrenContext);
    }
}

void ElementShadow::detach(const Node::AttachContext& context)
{
    Node::AttachContext childrenContext(context);
    childrenContext.resolvedStyle = 0;

    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (root->attached())
            root->detach(childrenContext);
    }
}

void ElementShadow::removeAllEventListeners()
{
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        for (Node* node = root; node; node = NodeTraversal::next(node))
            node->removeAllEventListeners();
    }
}

void ElementShadow::setNeedsDistributionRecalc()
{
    if (m_needsDistributionRecalc)
        return;
    m_needsDistributionRecalc = true;
    host()->markAncestorsWithChildNeedsDistributionRecalc();
    clearDistribution();
}

bool ElementShadow::didAffectApplyAuthorStyles()
{
    bool applyAuthorStyles = resolveApplyAuthorStyles();

    if (m_applyAuthorStyles == applyAuthorStyles)
        return false;

    m_applyAuthorStyles = applyAuthorStyles;
    return true;
}

bool ElementShadow::containsActiveStyles() const
{
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (root->hasScopedHTMLStyleChild())
            return true;
        if (!root->containsShadowElements())
            return false;
    }
    return false;
}

bool ElementShadow::resolveApplyAuthorStyles() const
{
    for (const ShadowRoot* shadowRoot = youngestShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot()) {
        if (shadowRoot->applyAuthorStyles())
            return true;
        if (!shadowRoot->containsShadowElements())
            break;
    }
    return false;
}

} // namespace
