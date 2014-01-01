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
#include "core/rendering/TextAutosizer.h"

using namespace std;

namespace WebCore {

FastTextAutosizer::FastTextAutosizer(Document* document)
    : m_document(document)
{
}

void FastTextAutosizer::prepareForLayout()
{
    if (!m_document->settings()
        || !m_document->settings()->textAutosizingEnabled()
        || m_document->printing()
        || !m_document->page())
        return;
    bool windowWidthChanged = updateWindowWidth();

    // If needed, set existing clusters as needing their multiplier recalculated.
    for (FingerprintToClusterMap::iterator it = m_clusterForFingerprint.begin(), end = m_clusterForFingerprint.end(); it != end; ++it) {
        Cluster* cluster = it->value.get();
        ASSERT(cluster);
        WTF::HashSet<RenderBlock*>& blocks = cluster->m_blocks;

        if (windowWidthChanged) {
            // Clusters depend on the window width. Changes to the width should cause all clusters to recalc.
            cluster->setNeedsClusterRecalc();
        } else {
            // If any of the cluster's blocks need a layout, mark the entire cluster as needing a recalc.
            for (WTF::HashSet<RenderBlock*>::iterator block = blocks.begin(); block != blocks.end(); ++block) {
                if ((*block)->needsLayout()) {
                    cluster->setNeedsClusterRecalc();
                    break;
                }
            }
        }

        // If the cluster needs a recalc, mark all blocks as needing a layout so they pick up the new cluster info.
        if (cluster->needsClusterRecalc()) {
            for (WTF::HashSet<RenderBlock*>::iterator block = blocks.begin(); block != blocks.end(); ++block)
                (*block)->setNeedsLayout();
        }
    }
}

bool FastTextAutosizer::updateWindowWidth()
{
    int originalWindowWidth = m_windowWidth;

    Frame* mainFrame = m_document->page()->mainFrame();
    IntSize windowSize = m_document->settings()->textAutosizingWindowSizeOverride();
    if (windowSize.isEmpty())
        windowSize = mainFrame->view()->unscaledVisibleContentSize(ScrollableArea::IncludeScrollbars);
    m_windowWidth = windowSize.width();

    return m_windowWidth != originalWindowWidth;
}

void FastTextAutosizer::record(RenderBlock* block)
{
    if (!m_document->settings()
        || !m_document->settings()->textAutosizingEnabled()
        || m_document->printing()
        || !m_document->page())
        return;

    if (!TextAutosizer::isAutosizingContainer(block))
        return;

    AtomicString blockFingerprint = fingerprint(block);
    HashMap<AtomicString, OwnPtr<Cluster> >::AddResult result =
        m_clusterForFingerprint.add(blockFingerprint, PassOwnPtr<Cluster>());

    if (result.isNewEntry)
        result.iterator->value = adoptPtr(new Cluster(blockFingerprint));

    Cluster* cluster = result.iterator->value.get();
    cluster->addBlock(block);

    m_clusterForBlock.set(block, cluster);
}

void FastTextAutosizer::destroy(RenderBlock* block)
{
    Cluster* cluster = m_clusterForBlock.take(block);
    if (!cluster)
        return;
    cluster->m_blocks.remove(block);
    if (cluster->m_blocks.isEmpty()) {
        // This deletes the Cluster.
        m_clusterForFingerprint.remove(cluster->m_fingerprint);
        return;
    }
    cluster->setNeedsClusterRecalc();
}

static void applyMultiplier(RenderObject* renderer, float multiplier)
{
    RenderStyle* currentStyle  = renderer->style();
    if (currentStyle->textAutosizingMultiplier() == multiplier)
        return;

    // We need to clone the render style to avoid breaking style sharing.
    RefPtr<RenderStyle> style = RenderStyle::clone(currentStyle);
    style->setTextAutosizingMultiplier(multiplier);
    style->setUnique();
    renderer->setStyleInternal(style.release());
}

void FastTextAutosizer::inflate(RenderBlock* block)
{
    Cluster* cluster = 0;
    for (const RenderObject* clusterBlock = block; clusterBlock && !cluster; clusterBlock = clusterBlock->parent()) {
        if (clusterBlock->isRenderBlock())
            cluster = m_clusterForBlock.get(toRenderBlock(clusterBlock));
    }
    if (!cluster)
        return;

    recalcClusterIfNeeded(cluster);

    applyMultiplier(block, cluster->m_multiplier);

    // FIXME: Add an optimization to not do this walk if it's not needed.
    for (InlineWalker walker(block); !walker.atEnd(); walker.advance()) {
        RenderObject* inlineObj = walker.current();
        if (inlineObj->isRenderBlock() && m_clusterForBlock.contains(toRenderBlock(inlineObj)))
            continue;

        applyMultiplier(inlineObj, cluster->m_multiplier);
    }
}

AtomicString FastTextAutosizer::fingerprint(const RenderBlock* block)
{
    // FIXME(crbug.com/322340): Implement a better fingerprinting algorithm.
    return AtomicString::number((unsigned long long) block);
}

void FastTextAutosizer::recalcClusterIfNeeded(FastTextAutosizer::Cluster* cluster)
{
    ASSERT(m_windowWidth > 0);
    if (!cluster->needsClusterRecalc())
        return;

    WTF::HashSet<RenderBlock*>& blocks = cluster->m_blocks;

    bool shouldAutosize = false;
    for (WTF::HashSet<RenderBlock*>::iterator it = blocks.begin(); it != blocks.end(); ++it)
        shouldAutosize |= TextAutosizer::containerShouldBeAutosized(*it);

    if (!shouldAutosize) {
        cluster->m_multiplier = 1.0f;
        return;
    }

    // Find the lowest common ancestor of blocks.
    // Note: this could be improved to not be O(b*h) for b blocks and tree height h.
    cluster->m_clusterRoot = 0;
    HashCountedSet<const RenderObject*> ancestors;
    for (WTF::HashSet<RenderBlock*>::iterator it = blocks.begin(); !cluster->m_clusterRoot && it != blocks.end(); ++it) {
        const RenderObject* renderer = (*it);
        while (renderer && (renderer = renderer->parent())) {
            ancestors.add(renderer);
            // The first ancestor that has all of the blocks as children wins and is crowned the cluster root.
            if (ancestors.count(renderer) == blocks.size()) {
                cluster->m_clusterRoot = renderer->isRenderBlock() ? renderer : renderer->containingBlock();
                break;
            }
        }
    }

    ASSERT(cluster->m_clusterRoot);
    bool horizontalWritingMode = isHorizontalWritingMode(cluster->m_clusterRoot->style()->writingMode());

    // Largest area of block that can be visible at once (assuming the main
    // frame doesn't get scaled to less than overview scale), in CSS pixels.
    IntSize layoutSize = m_document->page()->mainFrame()->view()->layoutSize();
    float layoutWidth = horizontalWritingMode ? layoutSize.width() : layoutSize.height();

    // Cluster root layout width, in CSS pixels.
    float rootWidth = toRenderBlock(cluster->m_clusterRoot)->contentLogicalWidth();

    // FIXME: incorporate font scale factor.
    float multiplier = min(rootWidth, layoutWidth) / m_windowWidth;
    cluster->m_multiplier = max(multiplier, 1.0f);
}

} // namespace WebCore
