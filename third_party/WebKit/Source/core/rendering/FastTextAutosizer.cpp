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

using namespace std;

namespace WebCore {

#ifdef AUTOSIZING_DOM_DEBUG_INFO
static void writeDebugInfo(RenderObject* renderObject, const AtomicString& output)
{
    Node* node = renderObject->node();
    if (!node)
        return;
    if (node->isDocumentNode())
        node = toDocument(node)->documentElement();
    if (node->isElementNode())
        toElement(node)->setAttribute("data-autosizing", output, ASSERT_NO_EXCEPTION);
}

static void writeDebugPageInfo(const Document* document, float baseMultiplier, int layoutWidth, int frameWidth)
{
    if (Element* element = document->documentElement()) {
        element->setAttribute("data-autosizing-pageinfo",
            AtomicString(String::format("bm %f * (lw %d / fw %d)",
                baseMultiplier, layoutWidth, frameWidth)),
            ASSERT_NO_EXCEPTION);
    }
}
#endif

static const RenderObject* parentElementRenderer(const RenderObject* renderer)
{
    // At style recalc, the renderer's parent may not be attached,
    // so we need to obtain this from the DOM tree.
    const Node* node = renderer->node();
    if (!node)
        return 0;

    while ((node = node->parentNode())) {
        if (node->isElementNode())
            return node->renderer();
    }
    return 0;
}

static bool isFormInput(const Element* element)
{
    DEFINE_STATIC_LOCAL(Vector<QualifiedName>, formInputTags, ());
    if (formInputTags.isEmpty()) {
        formInputTags.append(HTMLNames::inputTag);
        formInputTags.append(HTMLNames::buttonTag);
        formInputTags.append(HTMLNames::selectTag);
    }
    return formInputTags.contains(element->tagQName());
}

static bool isPotentialClusterRoot(const RenderObject* renderer)
{
    // "Potential cluster roots" are the smallest unit for which we can
    // enable/disable text autosizing.
    // - Must not be inline, as different multipliers on one line looks terrible.
    //   Exceptions are inline-block and alike elements (inline-table, -webkit-inline-*),
    //   as they often contain entire multi-line columns of text.
    // - Must not be list items, as items in the same list should look consistent (*).
    // - Must not be normal list items, as items in the same list should look
    //   consistent, unless they are floating or position:absolute/fixed.
    Node* node = renderer->generatingNode();
    if (node && !node->hasChildren())
        return false;
    if (!renderer->isRenderBlock())
        return false;
    if (renderer->isInline() && !renderer->style()->isDisplayReplacedType())
        return false;
    if (renderer->isListItem())
        return (renderer->isFloating() || renderer->isOutOfFlowPositioned());
    // Avoid creating containers for text within text controls, buttons, or <select> buttons.
    Node* parentNode = renderer->parent() ? renderer->parent()->generatingNode() : 0;
    if (parentNode && parentNode->isElementNode() && isFormInput(toElement(parentNode)))
        return false;

    return true;
}

static bool isIndependentDescendant(const RenderBlock* renderer)
{
    ASSERT(isPotentialClusterRoot(renderer));

    RenderBlock* containingBlock = renderer->containingBlock();
    return renderer->isRenderView()
        || renderer->isFloating()
        || renderer->isOutOfFlowPositioned()
        || renderer->isTableCell()
        || renderer->isTableCaption()
        || renderer->isFlexibleBoxIncludingDeprecated()
        || renderer->hasColumns()
        || (containingBlock && containingBlock->isHorizontalWritingMode() != renderer->isHorizontalWritingMode())
        || renderer->style()->isDisplayReplacedType()
        || renderer->isTextArea()
        || renderer->style()->userModify() != READ_ONLY;
    // FIXME: Tables need special handling to multiply all their columns by
    // the same amount even if they're different widths; so do hasColumns()
    // containers, and probably flexboxes...
}

static bool blockIsRowOfLinks(const RenderBlock* block)
{
    // A "row of links" is a block for which:
    //  1. It does not contain non-link text elements longer than 3 characters
    //  2. It contains a minimum of 3 inline links and all links should
    //     have the same specified font size.
    //  3. It should not contain <br> elements.
    //  4. It should contain only inline elements unless they are containers,
    //     children of link elements or children of sub-containers.
    int linkCount = 0;
    RenderObject* renderer = block->nextInPreOrder(block);
    float matchingFontSize = -1;

    while (renderer) {
        if (!isPotentialClusterRoot(renderer)) {
            if (renderer->isText() && toRenderText(renderer)->text().impl()->stripWhiteSpace()->length() > 3)
                return false;
            if (!renderer->isInline() || renderer->isBR())
                return false;
        }
        if (renderer->style()->isLink()) {
            linkCount++;
            if (matchingFontSize < 0)
                matchingFontSize = renderer->style()->specifiedFontSize();
            else if (matchingFontSize != renderer->style()->specifiedFontSize())
                return false;

            // Skip traversing descendants of the link.
            renderer = renderer->nextInPreOrderAfterChildren(block);
            continue;
        }
        renderer = renderer->nextInPreOrder(block);
    }

    return (linkCount >= 3);
}

static bool blockHeightConstrained(const RenderBlock* block)
{
    // FIXME: Propagate constrainedness down the tree, to avoid inefficiently walking back up from each box.
    // FIXME: This code needs to take into account vertical writing modes.
    // FIXME: Consider additional heuristics, such as ignoring fixed heights if the content is already overflowing before autosizing kicks in.
    for (; block; block = block->containingBlock()) {
        RenderStyle* style = block->style();
        if (style->overflowY() >= OSCROLL)
            return false;
        if (style->height().isSpecified() || style->maxHeight().isSpecified() || block->isOutOfFlowPositioned()) {
            // Some sites (e.g. wikipedia) set their html and/or body elements to height:100%,
            // without intending to constrain the height of the content within them.
            return !block->isDocumentElement() && !block->isBody();
        }
        if (block->isFloating())
            return false;
    }
    return false;
}

static bool blockContainsFormInput(const RenderBlock* block)
{
    const RenderObject* renderer = block;
    while (renderer) {
        const Node* node = renderer->node();
        if (node && node->isElementNode() && isFormInput(toElement(node)))
            return true;
        if (renderer == block)
            renderer = renderer->nextInPreOrder(block);
        else
            renderer = renderer->nextInPreOrderAfterChildren(block);
    }

    return false;
}

// Some blocks are not autosized even if their parent cluster wants them to.
static bool blockSuppressesAutosizing(const RenderBlock* block)
{
    if (blockContainsFormInput(block))
        return true;

    if (blockIsRowOfLinks(block))
        return true;

    // Don't autosize block-level text that can't wrap (as it's likely to
    // expand sideways and break the page's layout).
    if (!block->style()->autoWrap())
        return true;

    if (blockHeightConstrained(block))
        return true;

    return false;
}

static bool mightBeWiderOrNarrowerDescendant(const RenderBlock* block)
{
    // FIXME: This heuristic may need to be expanded to other ways a block can be wider or narrower
    //        than its parent containing block.
    return block->style() && block->style()->width().isSpecified();
}

// Before a block enters layout we don't know for sure if it will become a cluster.
// Note: clusters are also created for blocks that do become autosizing clusters!
static bool blockMightBecomeAutosizingCluster(const RenderBlock* block)
{
    ASSERT(isPotentialClusterRoot(block));
    return isIndependentDescendant(block) || mightBeWiderOrNarrowerDescendant(block) || block->isTable();
}

FastTextAutosizer::FastTextAutosizer(const Document* document)
    : m_document(document)
    , m_frameWidth(0)
    , m_layoutWidth(0)
    , m_baseMultiplier(0)
    , m_pageNeedsAutosizing(false)
    , m_previouslyAutosized(false)
    , m_updatePageInfoDeferred(false)
    , m_firstBlockToBeginLayout(0)
#ifndef NDEBUG
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

    if (!isPotentialClusterRoot(block))
        return;

    if (!blockMightBecomeAutosizingCluster(block))
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

FastTextAutosizer::BeginLayoutBehavior FastTextAutosizer::prepareForLayout(const RenderBlock* block)
{
#ifndef NDEBUG
    m_blocksThatHaveBegunLayout.add(block);
#endif

    if (!m_firstBlockToBeginLayout)  {
#ifdef AUTOSIZING_DOM_DEBUG_INFO
        writeDebugPageInfo(m_document, m_baseMultiplier, m_layoutWidth, m_frameWidth);
#endif
        m_firstBlockToBeginLayout = block;
        prepareClusterStack(block->parent());
    } else if (block == currentCluster()->m_root) {
        // Ignore beginLayout on the same block twice.
        // This can happen with paginated overflow.
        return StopLayout;
    }

    return ContinueLayout;
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
    ASSERT(enabled() && shouldHandleLayout());

    if (prepareForLayout(block) == StopLayout)
        return;

    if (Cluster* cluster = maybeCreateCluster(block)) {
        m_clusterStack.append(adoptPtr(cluster));
        if (block->isTable())
            inflateTable(toRenderTable(block));
    }

    if (block->childrenInline() && block->firstChild())
        inflate(block);
}

void FastTextAutosizer::inflateListItem(RenderListItem* listItem, RenderListMarker* listItemMarker)
{
    if (!enabled() || !shouldHandleLayout())
        return;
    ASSERT(listItem && listItemMarker);

    if (prepareForLayout(listItem) == StopLayout)
        return;

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
                if (!cell->isTableCell() || !cell->needsLayout())
                    continue;
                RenderTableCell* renderTableCell = toRenderTableCell(cell);

                bool shouldAutosize;
                if (blockSuppressesAutosizing(renderTableCell))
                    shouldAutosize = false;
                else if (Supercluster* supercluster = getSupercluster(renderTableCell))
                    shouldAutosize = anyClusterHasEnoughTextToAutosize(supercluster->m_roots, table);
                else
                    shouldAutosize = clusterWouldHaveEnoughTextToAutosize(renderTableCell, table);

                if (shouldAutosize) {
                    RenderObject* child = cell;
                    while (child) {
                        if (child->needsLayout() && !child->isTable()) {
                            if (child->isText()) {
                                applyMultiplier(child, multiplier);
                                applyMultiplier(child->parent(), multiplier); // Parent handles line spacing.
                            }
                            child = child->nextInPreOrder(cell);
                        } else {
                            // Skip inflation of this subtree.
                            child = child->nextInPreOrderAfterChildren(cell);
                        }
                    }
                }
            }
        }
    }
}

void FastTextAutosizer::endLayout(RenderBlock* block)
{
    ASSERT(enabled() && shouldHandleLayout());

    if (block == m_firstBlockToBeginLayout) {
        m_firstBlockToBeginLayout = 0;
        m_clusterStack.clear();
        m_superclusters.clear();
        m_stylesRetainedDuringLayout.clear();
#ifndef NDEBUG
        m_blocksThatHaveBegunLayout.clear();
#endif
    } else if (currentCluster()->m_root == block) {
        m_clusterStack.removeLast();
    }
}

void FastTextAutosizer::inflate(RenderBlock* block)
{
    Cluster* cluster = currentCluster();
    float multiplier = 0;
    RenderObject* descendant = block->nextInPreOrder();
    while (descendant) {
        // Skip block descendants because they will be inflate()'d on their own.
        if (descendant->isRenderBlock()) {
            descendant = descendant->nextInPreOrderAfterChildren(block);
            continue;
        }
        if (descendant->isText()) {
            // We only calculate this multiplier on-demand to ensure the parent block of this text
            // has entered layout.
            if (!multiplier)
                multiplier = cluster->m_autosize ? clusterMultiplier(cluster) : 1.0f;
            applyMultiplier(descendant, multiplier);
            applyMultiplier(descendant->parent(), multiplier); // Parent handles line spacing.
            // FIXME: Investigate why MarkOnlyThis is sufficient.
            if (descendant->parent()->isRenderInline())
                descendant->setPreferredLogicalWidthsDirty(MarkOnlyThis);
        }
        descendant = descendant->nextInPreOrder(block);
    }
}

bool FastTextAutosizer::enabled() const
{
    if (!m_document->settings() || !m_document->page() || m_document->printing())
        return false;

    return m_document->settings()->textAutosizingEnabled();
}

bool FastTextAutosizer::shouldHandleLayout() const
{
    return m_pageNeedsAutosizing && !m_updatePageInfoDeferred;
}

void FastTextAutosizer::updatePageInfoInAllFrames()
{
    ASSERT(!enabled() || m_document->frame()->isMainFrame());

    for (LocalFrame* frame = m_document->frame(); frame; frame = frame->tree().traverseNext()) {
        if (FastTextAutosizer* textAutosizer = frame->document()->fastTextAutosizer())
            textAutosizer->updatePageInfo();
    }
}

void FastTextAutosizer::updatePageInfo()
{
    if (!enabled()) {
        if (m_previouslyAutosized)
            resetMultipliers();
        return;
    }

    if (m_updatePageInfoDeferred)
        return;

    int previousFrameWidth = m_frameWidth;
    int previousLayoutWidth = m_layoutWidth;
    float previousBaseMultiplier = m_baseMultiplier;

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

    m_pageNeedsAutosizing = !!m_frameWidth
        && (m_baseMultiplier * (static_cast<float>(m_layoutWidth) / m_frameWidth) > 1.0f);

    // If we are no longer autosizing the page, we won't do anything during the next layout.
    // Set all the multipliers back to 1 now.
    if (!m_pageNeedsAutosizing && m_previouslyAutosized)
        resetMultipliers();

    // If page info has changed, multipliers may have changed. Force a layout to recompute them.
    if (m_pageNeedsAutosizing
        && (m_frameWidth != previousFrameWidth
            || m_layoutWidth != previousLayoutWidth
            || m_baseMultiplier != previousBaseMultiplier))
        setAllTextNeedsLayout();
}

void FastTextAutosizer::resetMultipliers()
{
    RenderObject* renderer = m_document->renderer();
    while (renderer) {
        if (RenderStyle* style = renderer->style()) {
            if (style->textAutosizingMultiplier() != 1)
                applyMultiplier(renderer, 1, LayoutNeeded);
        }
        renderer = renderer->nextInPreOrder();
    }
    m_previouslyAutosized = false;
}

void FastTextAutosizer::setAllTextNeedsLayout()
{
    RenderObject* renderer = m_document->renderer();
    while (renderer) {
        if (renderer->isText())
            renderer->setNeedsLayout();
        renderer = renderer->nextInPreOrder();
    }
}

bool FastTextAutosizer::clusterWouldHaveEnoughTextToAutosize(const RenderBlock* root, const RenderBlock* widthProvider)
{
    Cluster hypotheticalCluster(root, true, 0);
    return clusterHasEnoughTextToAutosize(&hypotheticalCluster, widthProvider);
}

bool FastTextAutosizer::clusterHasEnoughTextToAutosize(Cluster* cluster, const RenderBlock* widthProvider)
{
    if (cluster->m_hasEnoughTextToAutosize != UnknownAmountOfText)
        return cluster->m_hasEnoughTextToAutosize == HasEnoughText;

    const RenderBlock* root = cluster->m_root;
    if (!widthProvider)
        widthProvider = clusterWidthProvider(root);

    // TextAreas and user-modifiable areas get a free pass to autosize regardless of text content.
    if (root->isTextArea() || (root->style() && root->style()->userModify() != READ_ONLY)) {
        cluster->m_hasEnoughTextToAutosize = HasEnoughText;
        return true;
    }

    if (blockSuppressesAutosizing(root)) {
        cluster->m_hasEnoughTextToAutosize = NotEnoughText;
        return false;
    }

    // 4 lines of text is considered enough to autosize.
    float minimumTextLengthToAutosize = widthFromBlock(widthProvider) * 4;

    float length = 0;
    RenderObject* descendant = root->nextInPreOrder(root);
    while (descendant) {
        if (descendant->isRenderBlock()) {
            RenderBlock* block = toRenderBlock(descendant);

            // Note: Ideally we would check isWiderOrNarrowerDescendant here but we only know that
            //       after the block has entered layout, which may not be the case.
            if (isPotentialClusterRoot(block)) {
                bool isAutosizingClusterRoot = isIndependentDescendant(block) || block->isTable();
                if ((isAutosizingClusterRoot && !block->isTableCell()) || blockSuppressesAutosizing(block)) {
                    descendant = descendant->nextInPreOrderAfterChildren(root);
                    continue;
                }
            }
        } else if (descendant->isText()) {
            // Note: Using text().stripWhiteSpace().length() instead of renderedTextLength() because
            // the lineboxes will not be built until layout. These values can be different.
            // Note: This is an approximation assuming each character is 1em wide.
            length += toRenderText(descendant)->text().stripWhiteSpace().length() * descendant->style()->specifiedFontSize();

            if (length >= minimumTextLengthToAutosize) {
                cluster->m_hasEnoughTextToAutosize = HasEnoughText;
                return true;
            }
        }
        descendant = descendant->nextInPreOrder(root);
    }

    cluster->m_hasEnoughTextToAutosize = NotEnoughText;
    return false;
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

    // Use nodeIndex as a rough approximation of column number
    // (it's too early to call RenderTableCell::col).
    // FIXME: account for colspan
    if (renderer->isTableCell())
        data.m_column = renderer->node()->nodeIndex();

    return StringHasher::computeHash<UChar>(
        static_cast<const UChar*>(static_cast<const void*>(&data)),
        sizeof data / sizeof(UChar));
}

FastTextAutosizer::Cluster* FastTextAutosizer::maybeCreateCluster(const RenderBlock* block)
{
    if (!isPotentialClusterRoot(block))
        return 0;

    Cluster* parentCluster = m_clusterStack.isEmpty() ? 0 : currentCluster();
    ASSERT(parentCluster || block->isRenderView());

    bool mightAutosize = blockMightBecomeAutosizingCluster(block);
    bool suppressesAutosizing = blockSuppressesAutosizing(block);

    // If the block would not alter the m_autosize bit, it doesn't need to be a cluster.
    bool parentSuppressesAutosizing = parentCluster && !parentCluster->m_autosize;
    if (!mightAutosize && suppressesAutosizing == parentSuppressesAutosizing)
        return 0;

    return new Cluster(block, !suppressesAutosizing, parentCluster, getSupercluster(block));
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
    if (cluster->m_multiplier)
        return cluster->m_multiplier;

    if (cluster->m_root->isTable()
        || isIndependentDescendant(cluster->m_root)
        || isWiderOrNarrowerDescendant(cluster)) {

        if (cluster->m_supercluster) {
            cluster->m_multiplier = superclusterMultiplier(cluster);
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

#ifdef AUTOSIZING_DOM_DEBUG_INFO
    // FIXME(crbug.com/339213): Reduce redundant logic by storing the explanation category in the Cluster.
    String explanation = "";
    if (!cluster->m_autosize) {
        explanation = "[suppressed]";
    } else if (!(cluster->m_root->isTable() || isIndependentDescendant(cluster->m_root) || isWiderOrNarrowerDescendant(cluster))) {
        explanation = "[inherited]";
    } else if (cluster->m_supercluster) {
        explanation = "[supercluster]";
    } else if (!clusterHasEnoughTextToAutosize(cluster)) {
        explanation = "[insufficient-text]";
    } else {
        const RenderBlock* widthProvider = clusterWidthProvider(cluster->m_root);
        if (cluster->m_hasTableAncestor && cluster->m_multiplier < multiplierFromBlock(widthProvider)) {
            explanation = "[table-ancestor-limited]";
        } else {
            explanation = String::format("[from width %d of %s]",
                static_cast<int>(widthFromBlock(widthProvider)), widthProvider->debugName().utf8().data());
        }
    }
    writeDebugInfo(const_cast<RenderBlock*>(cluster->m_root),
        AtomicString(String::format("cluster: %f %s", cluster->m_multiplier, explanation.utf8().data())));
#endif

    ASSERT(cluster->m_multiplier);
    return cluster->m_multiplier;
}

bool FastTextAutosizer::anyClusterHasEnoughTextToAutosize(const BlockSet* roots, const RenderBlock* widthProvider)
{
    for (BlockSet::iterator it = roots->begin(); it != roots->end(); ++it) {
        if (clusterWouldHaveEnoughTextToAutosize(*it, widthProvider))
            return true;
    }
    return false;
}

float FastTextAutosizer::superclusterMultiplier(Cluster* cluster)
{
    Supercluster* supercluster = cluster->m_supercluster;
    if (!supercluster->m_multiplier) {
        const BlockSet* roots = supercluster->m_roots;
        const RenderBlock* widthProvider;

        if (cluster->m_root->isTableCell()) {
            widthProvider = clusterWidthProvider(cluster->m_root);
        } else {
            BlockSet widthProviders;
            for (BlockSet::iterator it = roots->begin(); it != roots->end(); ++it)
                widthProviders.add(clusterWidthProvider(*it));
            widthProvider = deepestCommonAncestor(widthProviders);
        }

        supercluster->m_multiplier = anyClusterHasEnoughTextToAutosize(roots, widthProvider)
            ? multiplierFromBlock(widthProvider) : 1.0f;
    }
    ASSERT(supercluster->m_multiplier);
    return supercluster->m_multiplier;
}

const RenderBlock* FastTextAutosizer::clusterWidthProvider(const RenderBlock* root)
{
    if (root->isTable() || root->isTableCell())
        return root;

    return deepestBlockContainingAllText(root);
}

float FastTextAutosizer::widthFromBlock(const RenderBlock* block)
{
    if (block->isTable()) {
        RenderBlock* containingBlock = block->containingBlock();
        ASSERT(block->containingBlock());
        if (block->style()->logicalWidth().isSpecified())
            return floatValueForLength(block->style()->logicalWidth(), containingBlock->contentLogicalWidth().toFloat());
        return containingBlock->contentLogicalWidth().toFloat();
    }
    return block->contentLogicalWidth().toFloat();
}

float FastTextAutosizer::multiplierFromBlock(const RenderBlock* block)
{
    // If block->needsLayout() is false, it does not need to be in m_blocksThatHaveBegunLayout.
    // This can happen during layout of a positioned object if the cluster's DBCAT is deeper
    // than the positioned object's containing block, and wasn't marked as needing layout.
    ASSERT(m_blocksThatHaveBegunLayout.contains(block) || !block->needsLayout());

    // Block width, in CSS pixels.
    float blockWidth = widthFromBlock(block);
    float multiplier = m_frameWidth ? min(blockWidth, static_cast<float>(m_layoutWidth)) / m_frameWidth : 1.0f;

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
        if (!isPotentialClusterRoot(child) || !isIndependentDescendant(toRenderBlock(child))) {
            const RenderObject* leaf = findTextLeaf(child, depth, firstOrLast);
            if (leaf)
                return leaf;
        }
        child = (firstOrLast == First) ? child->nextSibling() : child->previousSibling();
    }
    --depth;

    return 0;
}

void FastTextAutosizer::applyMultiplier(RenderObject* renderer, float multiplier, RelayoutBehavior relayoutBehavior)
{
    ASSERT(renderer);
    RenderStyle* currentStyle = renderer->style();
    if (currentStyle->textAutosizingMultiplier() == multiplier)
        return;

    // We need to clone the render style to avoid breaking style sharing.
    RefPtr<RenderStyle> style = RenderStyle::clone(currentStyle);
    style->setTextAutosizingMultiplier(multiplier);
    style->setUnique();

    // Don't free currentStyle until the end of the layout pass. This allows other parts of the system
    // to safely hold raw RenderStyle* pointers during layout, e.g. BreakingContext::m_currentStyle.
    m_stylesRetainedDuringLayout.append(currentStyle);

    switch (relayoutBehavior) {
    case AlreadyInLayout:
        renderer->setStyleInternal(style.release());
        if (renderer->isRenderBlock())
            toRenderBlock(renderer)->invalidateLineHeight();
        break;

    case LayoutNeeded:
        renderer->setStyle(style.release());
        break;
    }

    if (multiplier != 1)
        m_previouslyAutosized = true;
}

bool FastTextAutosizer::isWiderOrNarrowerDescendant(Cluster* cluster)
{
    if (!cluster->m_parent || !mightBeWiderOrNarrowerDescendant(cluster->m_root))
        return true;

    const RenderBlock* parentDeepestBlockContainingAllText = deepestBlockContainingAllText(cluster->m_parent);
    ASSERT(m_blocksThatHaveBegunLayout.contains(cluster->m_root));
    ASSERT(m_blocksThatHaveBegunLayout.contains(parentDeepestBlockContainingAllText));

    float contentWidth = cluster->m_root->contentLogicalWidth().toFloat();
    float clusterTextWidth = parentDeepestBlockContainingAllText->contentLogicalWidth().toFloat();

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
    ASSERT_WITH_SECURITY_IMPLICATION(!m_clusterStack.isEmpty());
    return m_clusterStack.last().get();
}

#ifndef NDEBUG
void FastTextAutosizer::FingerprintMapper::assertMapsAreConsistent()
{
    // For each fingerprint -> block mapping in m_blocksForFingerprint we should have an associated
    // map from block -> fingerprint in m_fingerprints.
    ReverseFingerprintMap::iterator end = m_blocksForFingerprint.end();
    for (ReverseFingerprintMap::iterator fingerprintIt = m_blocksForFingerprint.begin(); fingerprintIt != end; ++fingerprintIt) {
        Fingerprint fingerprint = fingerprintIt->key;
        BlockSet* blocks = fingerprintIt->value.get();
        for (BlockSet::iterator blockIt = blocks->begin(); blockIt != blocks->end(); ++blockIt) {
            const RenderBlock* block = (*blockIt);
            ASSERT(m_fingerprints.get(block) == fingerprint);
        }
    }
}
#endif

void FastTextAutosizer::FingerprintMapper::add(const RenderObject* renderer, Fingerprint fingerprint)
{
    remove(renderer);

    m_fingerprints.set(renderer, fingerprint);
#ifndef NDEBUG
    assertMapsAreConsistent();
#endif
}

void FastTextAutosizer::FingerprintMapper::addTentativeClusterRoot(const RenderBlock* block, Fingerprint fingerprint)
{
    add(block, fingerprint);

    ReverseFingerprintMap::AddResult addResult = m_blocksForFingerprint.add(fingerprint, PassOwnPtr<BlockSet>());
    if (addResult.isNewEntry)
        addResult.storedValue->value = adoptPtr(new BlockSet);
    addResult.storedValue->value->add(block);
#ifndef NDEBUG
    assertMapsAreConsistent();
#endif
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
#ifndef NDEBUG
    assertMapsAreConsistent();
#endif
}

FastTextAutosizer::Fingerprint FastTextAutosizer::FingerprintMapper::get(const RenderObject* renderer)
{
    return m_fingerprints.get(renderer);
}

FastTextAutosizer::BlockSet& FastTextAutosizer::FingerprintMapper::getTentativeClusterRoots(Fingerprint fingerprint)
{
    return *m_blocksForFingerprint.get(fingerprint);
}

FastTextAutosizer::LayoutScope::LayoutScope(RenderBlock* block)
    : m_textAutosizer(block->document().fastTextAutosizer())
    , m_block(block)
{
    if (!m_textAutosizer)
        return;

    if (m_textAutosizer->enabled() && m_textAutosizer->shouldHandleLayout())
        m_textAutosizer->beginLayout(m_block);
    else
        m_textAutosizer = 0;
}

FastTextAutosizer::LayoutScope::~LayoutScope()
{
    if (m_textAutosizer)
        m_textAutosizer->endLayout(m_block);
}

FastTextAutosizer::DeferUpdatePageInfo::DeferUpdatePageInfo(Page* page)
    : m_mainFrame(page->mainFrame())
{
    if (FastTextAutosizer* textAutosizer = m_mainFrame->document()->fastTextAutosizer()) {
        ASSERT(!textAutosizer->m_updatePageInfoDeferred);
        textAutosizer->m_updatePageInfoDeferred = true;
    }
}

FastTextAutosizer::DeferUpdatePageInfo::~DeferUpdatePageInfo()
{
    if (FastTextAutosizer* textAutosizer = m_mainFrame->document()->fastTextAutosizer()) {
        ASSERT(textAutosizer->m_updatePageInfoDeferred);
        textAutosizer->m_updatePageInfoDeferred = false;
        textAutosizer->updatePageInfoInAllFrames();
    }
}

} // namespace WebCore
