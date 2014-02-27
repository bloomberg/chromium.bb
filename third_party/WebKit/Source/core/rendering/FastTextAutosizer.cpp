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
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "core/rendering/InlineIterator.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/RenderListItem.h"
#include "core/rendering/RenderListMarker.h"
#include "core/rendering/RenderTableCell.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/TextAutosizer.h"

using namespace std;

namespace WebCore {

static const RenderObject* parentElementRenderer(const RenderObject* renderer)
{
    for (RenderObject* parent = renderer->parent(); parent; parent = parent->parent()) {
        Node* node = parent->generatingNode();
        if (node && node->isElementNode())
            return parent;
    }
    return 0;
}

FastTextAutosizer::FastTextAutosizer(const Document* document)
    : m_document(document)
    , m_frameWidth(0)
    , m_layoutWidth(0)
    , m_baseMultiplier(0)
    , m_firstBlock(0)
#ifndef NDEBUG
    , m_renderViewInfoPrepared(false)
    , m_blocksThatHaveBegunLayout()
#endif
    , m_superclusters()
    , m_clusterStack()
    , m_fingerprintMapper()
{
}

void FastTextAutosizer::record(const RenderBlock* block)
{
    if (!enabled())
        return;

    ASSERT(!m_blocksThatHaveBegunLayout.contains(block));

    if (!isFingerprintingCandidate(block))
        return;

    if (Fingerprint fingerprint = computeFingerprint(block))
        m_fingerprintMapper.addTentativeClusterRoot(block, fingerprint);
}

void FastTextAutosizer::destroy(const RenderBlock* block)
{
    if (!enabled())
        return;
    ASSERT(!m_blocksThatHaveBegunLayout.contains(block));

    m_fingerprintMapper.remove(block);
}

void FastTextAutosizer::prepareClusterStack(const RenderObject* renderer)
{
    if (!renderer)
        return;
    prepareClusterStack(renderer->parent());

    if (renderer->isRenderBlock()) {
        const RenderBlock* block = toRenderBlock(renderer);
#ifndef NDEBUG
        m_blocksThatHaveBegunLayout.add(block);
#endif
        if (Cluster* cluster = maybeCreateCluster(block))
            m_clusterStack.append(adoptPtr(cluster));
    }
}

void FastTextAutosizer::beginLayout(RenderBlock* block)
{
    ASSERT(enabled());
#ifndef NDEBUG
    m_blocksThatHaveBegunLayout.add(block);
#endif

    if (!m_firstBlock)  {
        prepareRenderViewInfo();
        prepareClusterStack(block->parent());
        m_firstBlock = block;
    } else if (block == currentCluster()->m_root) {
        // Ignore beginLayout on the same block twice.
        // This can happen with paginated overflow.
        return;
    }

    if (Cluster* cluster = maybeCreateCluster(block))
        m_clusterStack.append(adoptPtr(cluster));

    if (block->isTable())
        inflateTable(toRenderTable(block));
    else if (block->childrenInline())
        inflate(block);
}

void FastTextAutosizer::inflateListItem(RenderListItem* listItem, RenderListMarker* listItemMarker)
{
    if (!enabled())
        return;
    ASSERT(listItem && listItemMarker);
#ifndef NDEBUG
    m_blocksThatHaveBegunLayout.add(listItem);
#endif
    // Force the LI to be inside the DBCAT when computing the multiplier.
    // This guarantees that the DBCAT has entered layout, so we can ask for its width.
    // It also makes sense because the list marker is autosized like a text node.
    float multiplier = clusterMultiplier(currentCluster());

    applyMultiplier(listItem, multiplier);
    applyMultiplier(listItemMarker, multiplier);
}

void FastTextAutosizer::inflateTable(RenderTable* table)
{
    ASSERT(table);
    ASSERT(table->containingBlock());

    Cluster* cluster = currentCluster();
    ASSERT(cluster->m_root->isTable());

    // Pre-inflate cells that have enough text so that their inflated preferred widths will be used
    // for column sizing.
    // The multiplier used for cell descendants represents the maximum we can ever inflate
    // descendants without overflowing the cell width computed by the table layout. Therefore,
    // descendants of cells cannot use a multiplier higher than the table's multiplier.
    float multiplier = clusterMultiplier(cluster);
    for (RenderObject* section = table->firstChild(); section; section = section->nextSibling()) {
        if (!section->isTableSection())
            continue;
        for (RenderObject* row = section->firstChild(); row; row = row->nextSibling()) {
            if (!row->isTableRow())
                continue;
            for (RenderObject* cell = row->firstChild(); cell; cell = cell->nextSibling()) {
                if (!cell->isTableCell())
                    continue;
                RenderTableCell* renderTableCell = toRenderTableCell(cell);
                if (clusterWouldHaveEnoughTextToAutosize(renderTableCell, table)) {
                    for (RenderObject* child = cell; child; child = child->nextInPreOrder(cell)) {
                        if (child->isText()) {
                            applyMultiplier(child, multiplier);
                            applyMultiplier(child->parent(), multiplier); // Parent handles line spacing.
                        }
                    }
                }
            }
        }
    }
}

void FastTextAutosizer::endLayout(RenderBlock* block)
{
    ASSERT(enabled());

    if (currentCluster()->m_root == block)
        m_clusterStack.removeLast();

    if (block == m_firstBlock) {
        m_firstBlock = 0;
        m_clusterStack.clear();
        m_superclusters.clear();
#ifndef NDEBUG
        m_blocksThatHaveBegunLayout.clear();
#endif
    }
}

void FastTextAutosizer::inflate(RenderBlock* block)
{
    Cluster* cluster = currentCluster();
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
    if (!m_document->settings() || !m_document->page() || m_document->printing())
        return false;

    return m_document->settings()->textAutosizingEnabled();
}

void FastTextAutosizer::prepareRenderViewInfo()
{
    RenderView* renderView = toRenderView(m_document->renderer());
    bool horizontalWritingMode = isHorizontalWritingMode(renderView->style()->writingMode());

    LocalFrame* mainFrame = m_document->page()->mainFrame();
    IntSize frameSize = m_document->settings()->textAutosizingWindowSizeOverride();
    if (frameSize.isEmpty())
        frameSize = mainFrame->view()->unscaledVisibleContentSize(IncludeScrollbars);
    m_frameWidth = horizontalWritingMode ? frameSize.width() : frameSize.height();

    IntSize layoutSize = m_document->page()->mainFrame()->view()->layoutSize();
    m_layoutWidth = horizontalWritingMode ? layoutSize.width() : layoutSize.height();

    // Compute the base font scale multiplier based on device and accessibility settings.
    m_baseMultiplier = m_document->settings()->accessibilityFontScaleFactor();
    // If the page has a meta viewport or @viewport, don't apply the device scale adjustment.
    const ViewportDescription& viewportDescription = m_document->page()->mainFrame()->document()->viewportDescription();
    if (!viewportDescription.isSpecifiedByAuthor()) {
        float deviceScaleAdjustment = m_document->settings()->deviceScaleAdjustment();
        m_baseMultiplier *= deviceScaleAdjustment;
    }
#ifndef NDEBUG
    m_renderViewInfoPrepared = true;
#endif
}

bool FastTextAutosizer::isFingerprintingCandidate(const RenderBlock* block)
{
    // FIXME: move the logic out of TextAutosizer.cpp into this class.
    return block->isRenderView()
        || (TextAutosizer::isAutosizingContainer(block)
            && (TextAutosizer::isIndependentDescendant(block)
                || mightBeWiderOrNarrowerDescendant(block)));
}

bool FastTextAutosizer::clusterWouldHaveEnoughTextToAutosize(const RenderBlock* root, const RenderBlock* widthProvider)
{
    Cluster hypotheticalCluster(root, true, 0);
    return clusterHasEnoughTextToAutosize(&hypotheticalCluster, widthProvider);
}

bool FastTextAutosizer::clusterHasEnoughTextToAutosize(Cluster* cluster, const RenderBlock* widthProvider)
{
    const RenderBlock* root = cluster->m_root;
    if (!widthProvider)
        widthProvider = clusterWidthProvider(root);

    // TextAreas and user-modifiable areas get a free pass to autosize regardless of text content.
    if (root->isTextArea() || (root->style() && root->style()->userModify() != READ_ONLY))
        return true;

    static const float minLinesOfText = 4;
    // FIXME: This can be optimized because we only care if the text length is above a certain
    //        value, so we can stop computing the text length if we reach our target.
    if (textLength(cluster) >= widthFromBlock(widthProvider) * minLinesOfText)
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
        if (descendant->isRenderBlock()) {
            RenderBlock* block = toRenderBlock(descendant);
            if (TextAutosizer::isAutosizingContainer(block)) {
                // Note: Ideally we would check isWiderOrNarrowerDescendant here but we only know that
                //       after the block has entered layout, which may not be the case.
                bool isAutosizingClusterRoot = TextAutosizer::isIndependentDescendant(block);
                if (isAutosizingClusterRoot || !TextAutosizer::containerShouldBeAutosized(block)) {
                    descendant = descendant->nextInPreOrderAfterChildren(root);
                    continue;
                }
            }
        } else if (measureLocalText && descendant->isText()) {
            // Note: Using text().stripWhiteSpace().length() instead of renderedTextLength() because
            // the lineboxes will not be built until layout. These values can be different.
            length += toRenderText(descendant)->text().stripWhiteSpace().length() * descendant->style()->specifiedFontSize();
        }
        descendant = descendant->nextInPreOrder(root);
    }

    return cluster->m_textLength = length;
}

FastTextAutosizer::Fingerprint FastTextAutosizer::getFingerprint(const RenderObject* renderer)
{
    Fingerprint result = m_fingerprintMapper.get(renderer);
    if (!result) {
        result = computeFingerprint(renderer);
        m_fingerprintMapper.add(renderer, result);
    }
    return result;
}

FastTextAutosizer::Fingerprint FastTextAutosizer::computeFingerprint(const RenderObject* renderer)
{
    Node* node = renderer->generatingNode();
    if (!node || !node->isElementNode())
        return 0;

    FingerprintSourceData data;

    // FIXME: Instead of computing and caching parent fingerprints on demand,
    // consider maintaining a fingerprint stack during the style recalc
    // tree walk (similar to the cluster stack used during layout).

    if (const RenderObject* parent = parentElementRenderer(renderer))
        data.m_parentHash = getFingerprint(parent);

    data.m_qualifiedNameHash = QualifiedNameHash::hash(toElement(node)->tagQName());

    if (RenderStyle* style = renderer->style()) {
        data.m_packedStyleProperties = style->direction();
        data.m_packedStyleProperties |= (style->position() << 1);
        data.m_packedStyleProperties |= (style->floating() << 4);
        data.m_packedStyleProperties |= (style->display() << 6);
        data.m_packedStyleProperties |= (style->width().type() << 11);
        // packedStyleProperties effectively using 15 bits now.

        // consider for adding: writing mode, padding.

        data.m_width = style->width().getFloatValue();
    }

    return StringHasher::computeHash<UChar>(
        static_cast<const UChar*>(static_cast<const void*>(&data)),
        sizeof data / sizeof(UChar));
}

FastTextAutosizer::Cluster* FastTextAutosizer::maybeCreateCluster(const RenderBlock* block)
{
    if (!TextAutosizer::isAutosizingContainer(block))
        return 0;

    Cluster* parentCluster = m_clusterStack.isEmpty() ? 0 : currentCluster();
    ASSERT(parentCluster || block->isRenderView());

    // Create clusters to suppress / unsuppress autosizing based on containerShouldBeAutosized.
    bool containerCanAutosize = TextAutosizer::containerShouldBeAutosized(block);
    bool parentClusterCanAutosize = parentCluster && parentCluster->m_autosize;
    bool createClusterThatMightAutosize = block->isRenderView()
        || mightBeWiderOrNarrowerDescendant(block)
        || TextAutosizer::isIndependentDescendant(block)
        || block->isTable();

    // If the container would not alter the m_autosize bit, it doesn't need to be a cluster.
    if (!createClusterThatMightAutosize && containerCanAutosize == parentClusterCanAutosize)
        return 0;

    return new Cluster(block, containerCanAutosize, parentCluster, getSupercluster(block));
}

FastTextAutosizer::Supercluster* FastTextAutosizer::getSupercluster(const RenderBlock* block)
{
    Fingerprint fingerprint = m_fingerprintMapper.get(block);
    if (!fingerprint)
        return 0;

    BlockSet* roots = &m_fingerprintMapper.getTentativeClusterRoots(fingerprint);
    if (!roots || roots->size() < 2 || !roots->contains(block))
        return 0;

    SuperclusterMap::AddResult addResult = m_superclusters.add(fingerprint, PassOwnPtr<Supercluster>());
    if (!addResult.isNewEntry)
        return addResult.storedValue->value.get();

    Supercluster* supercluster = new Supercluster(roots);
    addResult.storedValue->value = adoptPtr(supercluster);
    return supercluster;
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
    ASSERT(m_renderViewInfoPrepared);
    if (!cluster->m_multiplier) {
        if (cluster->m_root->isTable()
            || TextAutosizer::isIndependentDescendant(cluster->m_root)
            || isWiderOrNarrowerDescendant(cluster)) {

            // FIXME: The table cell ancestor check disables superclusters in tables until we
            //        work out exactly how tables and superclusters should interact.
            if (cluster->m_supercluster && !cluster->m_hasTableAncestor) {
                cluster->m_multiplier = superclusterMultiplier(cluster->m_supercluster);
            } else if (clusterHasEnoughTextToAutosize(cluster)) {
                cluster->m_multiplier = multiplierFromBlock(clusterWidthProvider(cluster->m_root));
                // Do not inflate table descendants above the table's multiplier. See inflateTable(...) for details.
                if (cluster->m_hasTableAncestor)
                    cluster->m_multiplier = min(cluster->m_multiplier, clusterMultiplier(cluster->m_parent));
            } else {
                cluster->m_multiplier = 1.0f;
            }
        } else {
            cluster->m_multiplier = cluster->m_parent ? clusterMultiplier(cluster->m_parent) : 1.0f;
        }
    }
    ASSERT(cluster->m_multiplier);
    return cluster->m_multiplier;
}

float FastTextAutosizer::superclusterMultiplier(Supercluster* supercluster)
{
    if (!supercluster->m_multiplier) {
        const BlockSet* roots = supercluster->m_roots;
        BlockSet widthProviders;
        for (BlockSet::iterator it = roots->begin(); it != roots->end(); ++it)
            widthProviders.add(clusterWidthProvider(*it));
        const RenderBlock* widthProvider = deepestCommonAncestor(widthProviders);

        for (BlockSet::iterator it = roots->begin(); it != roots->end(); ++it)
            supercluster->m_anyClusterHasEnoughText |= clusterWouldHaveEnoughTextToAutosize(*it, widthProvider);

        supercluster->m_multiplier = supercluster->m_anyClusterHasEnoughText
            ? multiplierFromBlock(widthProvider) : 1.0f;
    }
    ASSERT(supercluster->m_multiplier);
    return supercluster->m_multiplier;
}

const RenderBlock* FastTextAutosizer::clusterWidthProvider(const RenderBlock* root)
{
    if (root->isTableCell())
        return toRenderTableCell(root)->table();
    if (root->isTable())
        return toRenderTable(root);

    return deepestBlockContainingAllText(root);
}

float FastTextAutosizer::widthFromBlock(const RenderBlock* block)
{
    if (block->isTable()) {
        RenderBlock* containingBlock = block->containingBlock();
        ASSERT(block->containingBlock());
        if (block->style()->logicalWidth().isSpecified())
            return floatValueForLength(block->style()->logicalWidth(), containingBlock->contentLogicalWidth());
        return containingBlock->contentLogicalWidth();
    }
    return block->contentLogicalWidth();
}

float FastTextAutosizer::multiplierFromBlock(const RenderBlock* block)
{
    // If block->needsLayout() is false, it does not need to be in m_blocksThatHaveBegunLayout.
    // This can happen during layout of a positioned object if the cluster's DBCAT is deeper
    // than the positioned object's containing block, and wasn't marked as needing layout.
    ASSERT(m_blocksThatHaveBegunLayout.contains(block) || !block->needsLayout());

    // Block width, in CSS pixels.
    float blockWidth = widthFromBlock(block);
    float multiplier = min(blockWidth, static_cast<float>(m_layoutWidth)) / m_frameWidth;

    return max(m_baseMultiplier * multiplier, 1.0f);
}

const RenderBlock* FastTextAutosizer::deepestBlockContainingAllText(Cluster* cluster)
{
    if (!cluster->m_deepestBlockContainingAllText)
        cluster->m_deepestBlockContainingAllText = deepestBlockContainingAllText(cluster->m_root);

    return cluster->m_deepestBlockContainingAllText;
}

// FIXME: Refactor this to look more like FastTextAutosizer::deepestCommonAncestor. This is copied
//        from TextAutosizer::findDeepestBlockContainingAllText.
const RenderBlock* FastTextAutosizer::deepestBlockContainingAllText(const RenderBlock* root)
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
    // List items are treated as text due to the marker.
    // The actual renderer for the marker (RenderListMarker) may not be in the tree yet since it is added during layout.
    if (parent->isListItem())
        return parent;

    if (parent->isEmpty())
        return parent->isText() ? parent : 0;

    ++depth;
    const RenderObject* child = (firstOrLast == First) ? parent->firstChild() : parent->lastChild();
    while (child) {
        // Note: At this point clusters may not have been created for these blocks so we cannot rely
        //       on m_clusters. Instead, we use a best-guess about whether the block will become a cluster.
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
    ASSERT(renderer);
    RenderStyle* currentStyle = renderer->style();
    if (currentStyle->textAutosizingMultiplier() == multiplier)
        return;

    // We need to clone the render style to avoid breaking style sharing.
    RefPtr<RenderStyle> style = RenderStyle::clone(currentStyle);
    style->setTextAutosizingMultiplier(multiplier);
    style->setUnique();
    renderer->setStyleInternal(style.release());
}

bool FastTextAutosizer::mightBeWiderOrNarrowerDescendant(const RenderBlock* block)
{
    // FIXME: This heuristic may need to be expanded to other ways a block can be wider or narrower
    //        than its parent containing block.
    return block->style() && block->style()->width().isSpecified();
}

bool FastTextAutosizer::isWiderOrNarrowerDescendant(Cluster* cluster)
{
    if (!cluster->m_parent || !mightBeWiderOrNarrowerDescendant(cluster->m_root))
        return true;

    const RenderBlock* parentDeepestBlockContainingAllText = deepestBlockContainingAllText(cluster->m_parent);
    ASSERT(m_blocksThatHaveBegunLayout.contains(cluster->m_root));
    ASSERT(m_blocksThatHaveBegunLayout.contains(parentDeepestBlockContainingAllText));

    float contentWidth = cluster->m_root->contentLogicalWidth();
    float clusterTextWidth = parentDeepestBlockContainingAllText->contentLogicalWidth();

    // Clusters with a root that is wider than the deepestBlockContainingAllText of their parent
    // autosize independently of their parent.
    if (contentWidth > clusterTextWidth)
        return true;

    // Clusters with a root that is significantly narrower than the deepestBlockContainingAllText of
    // their parent autosize independently of their parent.
    static float narrowWidthDifference = 200;
    if (clusterTextWidth - contentWidth > narrowWidthDifference)
        return true;

    return false;
}

FastTextAutosizer::Cluster* FastTextAutosizer::currentCluster() const
{
    ASSERT(!m_clusterStack.isEmpty());
    return m_clusterStack.last().get();
}

void FastTextAutosizer::FingerprintMapper::add(const RenderObject* renderer, Fingerprint fingerprint)
{
    m_fingerprints.set(renderer, fingerprint);
}

void FastTextAutosizer::FingerprintMapper::addTentativeClusterRoot(const RenderBlock* block, Fingerprint fingerprint)
{
    add(block, fingerprint);

    ReverseFingerprintMap::AddResult addResult = m_blocksForFingerprint.add(fingerprint, PassOwnPtr<BlockSet>());
    if (addResult.isNewEntry)
        addResult.storedValue->value = adoptPtr(new BlockSet);
    addResult.storedValue->value->add(block);
}

void FastTextAutosizer::FingerprintMapper::remove(const RenderObject* renderer)
{
    Fingerprint fingerprint = m_fingerprints.take(renderer);
    if (!fingerprint || !renderer->isRenderBlock())
        return;

    ReverseFingerprintMap::iterator blocksIter = m_blocksForFingerprint.find(fingerprint);
    if (blocksIter == m_blocksForFingerprint.end())
        return;

    BlockSet& blocks = *blocksIter->value;
    blocks.remove(toRenderBlock(renderer));
    if (blocks.isEmpty())
        m_blocksForFingerprint.remove(blocksIter);
}

FastTextAutosizer::Fingerprint FastTextAutosizer::FingerprintMapper::get(const RenderObject* renderer)
{
    return m_fingerprints.get(renderer);
}

FastTextAutosizer::BlockSet& FastTextAutosizer::FingerprintMapper::getTentativeClusterRoots(Fingerprint fingerprint)
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
