/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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
#include "core/rendering/FastTextAutosizer.h"

#include "core/dom/Document.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "core/rendering/InlineIterator.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/TextAutosizer.h"

using namespace std;

namespace WebCore {

FastTextAutosizer::FastTextAutosizer(const Document* document)
    : m_document(document)
#ifndef NDEBUG
    , m_renderViewInfoPrepared(false)
#endif
{
}

void FastTextAutosizer::record(const RenderBlock* block)
{
    if (!enabled())
        return;
#ifndef NDEBUG
    ASSERT(!m_blocksThatHaveBegunLayout.contains(block));
#endif

    if (!isFingerprintingCandidate(block))
        return;

    AtomicString fingerprint = computeFingerprint(block);
    if (fingerprint.isNull())
        return;

    m_fingerprintMapper.add(block, fingerprint);
}

void FastTextAutosizer::destroy(const RenderBlock* block)
{
#ifndef NDEBUG
    ASSERT(!m_blocksThatHaveBegunLayout.contains(block));
#endif
    m_fingerprintMapper.remove(block);
}

void FastTextAutosizer::beginLayout(RenderBlock* block)
{
    if (!enabled())
        return;
#ifndef NDEBUG
    m_blocksThatHaveBegunLayout.add(block);
#endif

    if (block->isRenderView())
        prepareRenderViewInfo(toRenderView(block));

    if (Cluster* cluster = maybeGetOrCreateCluster(block))
        m_clusterStack.append(cluster);

    if (block->childrenInline())
        inflate(block);
}

void FastTextAutosizer::endLayout(RenderBlock* block)
{
    if (!enabled())
        return;
#ifndef NDEBUG
    m_blocksThatHaveBegunLayout.remove(block);
#endif

    Cluster* cluster = currentCluster();
    if (cluster && cluster->m_root == block)
        m_clusterStack.removeLast();
}

void FastTextAutosizer::inflate(RenderBlock* block)
{
    Cluster* cluster = currentCluster();
    if (!cluster)
        return;

    float multiplier = 0;
    for (RenderObject* descendant = nextChildSkippingChildrenOfBlocks(block, block); descendant; descendant = nextChildSkippingChildrenOfBlocks(descendant, block)) {
        if (descendant->isText()) {
            // We only calculate this multiplier on-demand to ensure the parent block of this text
            // has entered layout.
            if (!multiplier)
                multiplier = cluster->m_autosize ? clusterMultiplier(cluster) : 1.0f;
            applyMultiplier(descendant, multiplier);
            applyMultiplier(descendant->parent(), multiplier); // Parent handles line spacing.
        }
    }
}

bool FastTextAutosizer::enabled()
{
    return m_document->settings()
        && m_document->settings()->textAutosizingEnabled()
        && !m_document->printing()
        && m_document->page();
}

void FastTextAutosizer::prepareRenderViewInfo(RenderView* renderView)
{
    bool horizontalWritingMode = isHorizontalWritingMode(renderView->style()->writingMode());

    Frame* mainFrame = m_document->page()->mainFrame();
    IntSize frameSize = m_document->settings()->textAutosizingWindowSizeOverride();
    if (frameSize.isEmpty())
        frameSize = mainFrame->view()->unscaledVisibleContentSize(ScrollableArea::IncludeScrollbars);
    m_frameWidth = horizontalWritingMode ? frameSize.width() : frameSize.height();

    IntSize layoutSize = m_document->page()->mainFrame()->view()->layoutSize();
    m_layoutWidth = horizontalWritingMode ? layoutSize.width() : layoutSize.height();

    // Compute the base font scale multiplier based on device and accessibility settings.
    m_baseMultiplier = m_document->settings()->accessibilityFontScaleFactor();
    // If the page has a meta viewport or @viewport, don't apply the device scale adjustment.
    const ViewportDescription& viewportDescription = m_document->page()->mainFrame()->document()->viewportDescription();
    if (!viewportDescription.isSpecifiedByAuthor())
        m_baseMultiplier *= m_document->settings()->deviceScaleAdjustment();
#ifndef NDEBUG
    m_renderViewInfoPrepared = true;
#endif
}

bool FastTextAutosizer::isFingerprintingCandidate(const RenderBlock* block)
{
    // FIXME: move the logic out of TextAutosizer.cpp into this class.
    return block->isRenderView()
        || (TextAutosizer::isAutosizingContainer(block)
            && TextAutosizer::isIndependentDescendant(block));
}

bool FastTextAutosizer::clusterHasEnoughTextToAutosize(Cluster* cluster)
{
    const RenderBlock* root = cluster->m_root;

    // TextAreas and user-modifiable areas get a free pass to autosize regardless of text content.
    if (root->isTextArea() || (root->style() && root->style()->userModify() != READ_ONLY))
        return true;

    static const float minLinesOfText = 4;
    if (textLength(cluster) >= root->contentLogicalWidth() * minLinesOfText)
        return true;

    return false;
}

float FastTextAutosizer::textLength(Cluster* cluster)
{
    if (cluster->m_textLength >= 0)
        return cluster->m_textLength;

    float length = 0;
    const RenderBlock* root = cluster->m_root;
    bool measureLocalText = TextAutosizer::containerShouldBeAutosized(root);
    RenderObject* descendant = root->nextInPreOrder(root);
    while (descendant) {
        if (descendant->isRenderBlock() && m_clusters.contains(toRenderBlock(descendant))) {
            length += textLength(m_clusters.get(toRenderBlock(descendant)));
            descendant = descendant->nextInPreOrderAfterChildren(root);
            continue;
        }

        if (measureLocalText && descendant->isText()) {
            // Note: Using text().stripWhiteSpace().length() instead of renderedTextLength() because
            // the lineboxes will not be built until layout. These values can be different.
            length += toRenderText(descendant)->text().stripWhiteSpace().length() * descendant->style()->specifiedFontSize();
        }
        descendant = descendant->nextInPreOrder(root);
    }

    return cluster->m_textLength = length;
}

AtomicString FastTextAutosizer::computeFingerprint(const RenderBlock* block)
{
    // FIXME(crbug.com/322340): Implement a fingerprinting algorithm.
    return nullAtom;
}

FastTextAutosizer::Cluster* FastTextAutosizer::maybeGetOrCreateCluster(const RenderBlock* block)
{
    if (!TextAutosizer::isAutosizingContainer(block))
        return 0;

    Cluster* parentCluster = currentCluster();

    // Create clusters to suppress / unsuppress autosizing based on containerShouldBeAutosized.
    bool containerCanAutosize = TextAutosizer::containerShouldBeAutosized(block);
    bool parentClusterCanAutosize = parentCluster && parentCluster->m_autosize;

    // If the container would not alter the m_autosize bit, it doesn't need to be a cluster.
    if (!TextAutosizer::isIndependentDescendant(block) && containerCanAutosize == parentClusterCanAutosize)
        return 0;

    ClusterMap::AddResult addResult = m_clusters.add(block, PassOwnPtr<Cluster>());
    if (!addResult.isNewEntry)
        return addResult.iterator->value.get();

    AtomicString fingerprint = m_fingerprintMapper.get(block);
    if (fingerprint.isNull()) {
        addResult.iterator->value = adoptPtr(new Cluster(block, containerCanAutosize, parentCluster));
        return addResult.iterator->value.get();
    }
    return addSupercluster(fingerprint, block);
}

// FIXME: The supercluster logic does not work yet.
FastTextAutosizer::Cluster* FastTextAutosizer::addSupercluster(AtomicString fingerprint, const RenderBlock* returnFor)
{
    BlockSet& roots = m_fingerprintMapper.getBlocks(fingerprint);

    Cluster* result = 0;
    for (BlockSet::iterator it = roots.begin(); it != roots.end(); ++it) {
        Cluster* cluster = new Cluster(*it, TextAutosizer::containerShouldBeAutosized(*it), currentCluster());
        m_clusters.set(*it, adoptPtr(cluster));

        if (*it == returnFor)
            result = cluster;
    }
    return result;
}

const RenderBlock* FastTextAutosizer::deepestCommonAncestor(BlockSet& blocks)
{
    // Find the lowest common ancestor of blocks.
    // Note: this could be improved to not be O(b*h) for b blocks and tree height h.
    HashCountedSet<const RenderBlock*> ancestors;
    for (BlockSet::iterator it = blocks.begin(); it != blocks.end(); ++it) {
        for (const RenderBlock* block = (*it); block; block = block->containingBlock()) {
            ancestors.add(block);
            // The first ancestor that has all of the blocks as children wins.
            if (ancestors.count(block) == blocks.size())
                return block;
        }
    }
    ASSERT_NOT_REACHED();
    return 0;
}

float FastTextAutosizer::clusterMultiplier(Cluster* cluster)
{
#ifndef NDEBUG
    ASSERT(m_renderViewInfoPrepared);
#endif
    if (!cluster->m_multiplier) {
        if (TextAutosizer::isIndependentDescendant(cluster->m_root)) {
            if (clusterHasEnoughTextToAutosize(cluster)) {
                const RenderBlock* deepestBlockContainingAllText = findDeepestBlockContainingAllText(cluster->m_root);
#ifndef NDEBUG
                // This ensures the deepest block containing all text has a valid contentLogicalWidth.
                ASSERT(m_blocksThatHaveBegunLayout.contains(deepestBlockContainingAllText));
#endif
                // Block width, in CSS pixels.
                float textBlockWidth = deepestBlockContainingAllText->contentLogicalWidth();
                float multiplier = min(textBlockWidth, static_cast<float>(m_layoutWidth)) / m_frameWidth;
                cluster->m_multiplier = max(m_baseMultiplier * multiplier, 1.0f);
            } else {
                cluster->m_multiplier = 1.0f;
            }
        } else {
            cluster->m_multiplier = cluster->m_parent ? clusterMultiplier(cluster->m_parent) : 1.0f;
        }
    }
    return cluster->m_multiplier;
}

// FIXME: Refactor this to look more like FastTextAutosizer::deepestCommonAncestor. This is copied
//        from TextAutosizer::findDeepestBlockContainingAllText.
const RenderBlock* FastTextAutosizer::findDeepestBlockContainingAllText(const RenderBlock* root)
{
    size_t firstDepth = 0;
    const RenderObject* firstTextLeaf = findTextLeaf(root, firstDepth, First);
    if (!firstTextLeaf)
        return root;

    size_t lastDepth = 0;
    const RenderObject* lastTextLeaf = findTextLeaf(root, lastDepth, Last);
    ASSERT(lastTextLeaf);

    // Equalize the depths if necessary. Only one of the while loops below will get executed.
    const RenderObject* firstNode = firstTextLeaf;
    const RenderObject* lastNode = lastTextLeaf;
    while (firstDepth > lastDepth) {
        firstNode = firstNode->parent();
        --firstDepth;
    }
    while (lastDepth > firstDepth) {
        lastNode = lastNode->parent();
        --lastDepth;
    }

    // Go up from both nodes until the parent is the same. Both pointers will point to the LCA then.
    while (firstNode != lastNode) {
        firstNode = firstNode->parent();
        lastNode = lastNode->parent();
    }

    if (firstNode->isRenderBlock())
        return toRenderBlock(firstNode);

    // containingBlock() should never leave the cluster, since it only skips ancestors when finding
    // the container of position:absolute/fixed blocks, and those cannot exist between a cluster and
    // its text node's lowest common ancestor as isAutosizingCluster would have made them into their
    // own independent cluster.
    const RenderBlock* containingBlock = firstNode->containingBlock();
    ASSERT(containingBlock->isDescendantOf(root));

    return containingBlock;
}

const RenderObject* FastTextAutosizer::findTextLeaf(const RenderObject* parent, size_t& depth, TextLeafSearch firstOrLast)
{
    if (parent->isEmpty())
        return parent->isText() ? parent : 0;

    ++depth;
    const RenderObject* child = (firstOrLast == First) ? parent->firstChild() : parent->lastChild();
    while (child) {
        // At this point clusters may not have been created for these blocks so we cannot rely on m_clusters.
        if (!TextAutosizer::isAutosizingContainer(child) || !TextAutosizer::isIndependentDescendant(toRenderBlock(child))) {
            const RenderObject* leaf = findTextLeaf(child, depth, firstOrLast);
            if (leaf)
                return leaf;
        }
        child = (firstOrLast == First) ? child->nextSibling() : child->previousSibling();
    }
    --depth;

    return 0;
}

void FastTextAutosizer::applyMultiplier(RenderObject* renderer, float multiplier)
{
    RenderStyle* currentStyle = renderer->style();
    if (currentStyle->textAutosizingMultiplier() == multiplier)
        return;

    // We need to clone the render style to avoid breaking style sharing.
    RefPtr<RenderStyle> style = RenderStyle::clone(currentStyle);
    style->setTextAutosizingMultiplier(multiplier);
    style->setUnique();
    renderer->setStyleInternal(style.release());
}

FastTextAutosizer::Cluster* FastTextAutosizer::currentCluster() const
{
    return !m_clusterStack.isEmpty() ? m_clusterStack.last() : 0;
}

void FastTextAutosizer::FingerprintMapper::add(const RenderBlock* block, AtomicString fingerprint)
{
    m_fingerprints.set(block, fingerprint);

    ReverseFingerprintMap::AddResult addResult = m_blocksForFingerprint.add(fingerprint, PassOwnPtr<BlockSet>());
    if (addResult.isNewEntry)
        addResult.iterator->value = adoptPtr(new BlockSet);
    addResult.iterator->value->add(block);
}

void FastTextAutosizer::FingerprintMapper::remove(const RenderBlock* block)
{
    AtomicString fingerprint = m_fingerprints.take(block);
    if (fingerprint.isNull())
        return;

    ReverseFingerprintMap::iterator blocksIter = m_blocksForFingerprint.find(fingerprint);
    BlockSet& blocks = *blocksIter->value;
    blocks.remove(block);
    if (blocks.isEmpty())
        m_blocksForFingerprint.remove(blocksIter);
}

AtomicString FastTextAutosizer::FingerprintMapper::get(const RenderBlock* block)
{
    return m_fingerprints.get(block);
}

FastTextAutosizer::BlockSet& FastTextAutosizer::FingerprintMapper::getBlocks(AtomicString fingerprint)
{
    return *m_blocksForFingerprint.get(fingerprint);
}

RenderObject* FastTextAutosizer::nextChildSkippingChildrenOfBlocks(const RenderObject* current, const RenderObject* stayWithin)
{
    if (current == stayWithin || !current->isRenderBlock())
        return current->nextInPreOrder(stayWithin);
    return current->nextInPreOrderAfterChildren(stayWithin);
}

} // namespace WebCore
