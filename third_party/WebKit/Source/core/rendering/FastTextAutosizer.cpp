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
#include "core/frame/Settings.h"
#include "core/rendering/InlineIterator.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/TextAutosizer.h"

namespace WebCore {

FastTextAutosizer::FastTextAutosizer(Document* document)
    : m_document(document)
{
}

void FastTextAutosizer::record(const RenderBlock* block)
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
    cluster->m_blocks.add(block);

    m_clusterForBlock.set(block, cluster);
}

void FastTextAutosizer::destroy(const RenderBlock* block)
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
    cluster->m_multiplier = 0;
}

static void applyMultiplier(RenderObject* renderer, float multiplier)
{
    // We need to clone the render style to avoid breaking style sharing.
    RefPtr<RenderStyle> style = RenderStyle::clone(renderer->style());
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
    if (!cluster->m_multiplier)
        cluster->m_multiplier = computeMultiplier(cluster);
    if (cluster->m_multiplier == 1)
        return;

    applyMultiplier(block, cluster->m_multiplier);
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
    return String::number((unsigned long long) block);
}

float FastTextAutosizer::computeMultiplier(const FastTextAutosizer::Cluster* cluster)
{
    const WTF::HashSet<const RenderBlock*>& blocks = cluster->m_blocks;

    bool shouldAutosize = false;
    for (WTF::HashSet<const RenderBlock*>::iterator it = blocks.begin(); it != blocks.end(); ++it)
        shouldAutosize |= TextAutosizer::containerShouldBeAutosized(*it);

    if (!shouldAutosize)
        return 1.0f;

    // FIXME(crbug.com/322344): Implement multiplier computation.
    return 1.5f;
}

} // namespace WebCore
