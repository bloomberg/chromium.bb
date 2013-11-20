/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/loader/HistoryController.h"

#include "core/dom/Document.h"
#include "core/history/HistoryItem.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/inspector/InspectorController.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/FrameLoaderStateMachine.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameView.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "platform/Logging.h"
#include "wtf/Deque.h"
#include "wtf/text/CString.h"

namespace WebCore {

PassOwnPtr<HistoryNode> HistoryNode::create(HistoryEntry* entry, HistoryItem* value)
{
    return adoptPtr(new HistoryNode(entry, value));
}

HistoryNode* HistoryNode::addChild(PassRefPtr<HistoryItem> item)
{
    m_children.append(HistoryNode::create(m_entry, item.get()));
    return m_children.last().get();
}

PassOwnPtr<HistoryNode> HistoryNode::cloneAndReplace(HistoryEntry* newEntry, HistoryItem* newItem, HistoryItem* oldItem, bool clipAtTarget, Frame* frame)
{
    bool isNodeBeingNavigated = m_value == oldItem;
    HistoryItem* itemForCreate = isNodeBeingNavigated ? newItem : m_value.get();
    OwnPtr<HistoryNode> newHistoryNode = create(newEntry, itemForCreate);

    if (!clipAtTarget || !isNodeBeingNavigated) {
        for (Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
            HistoryNode* childHistoryNode = m_entry->m_framesToItems.get(child->frameID());
            if (!childHistoryNode)
                continue;
            newHistoryNode->m_children.append(childHistoryNode->cloneAndReplace(newEntry, newItem, oldItem, clipAtTarget, child));
        }
    }
    return newHistoryNode.release();
}

HistoryNode::HistoryNode(HistoryEntry* entry, HistoryItem* value)
    : m_entry(entry)
    , m_value(value)
{
    m_entry->m_framesToItems.add(value->targetFrameID(), this);
    String target = value->target();
    if (target.isNull())
        target = emptyString();
    m_entry->m_uniqueNamesToItems.add(target, this);
}

HistoryEntry::HistoryEntry(HistoryItem* root)
{
    m_root = HistoryNode::create(this, root);
}

PassOwnPtr<HistoryEntry> HistoryEntry::create(HistoryItem* root)
{
    return adoptPtr(new HistoryEntry(root));
}

PassOwnPtr<HistoryEntry> HistoryEntry::cloneAndReplace(HistoryItem* newItem, HistoryItem* oldItem, bool clipAtTarget, Page* page)
{
    OwnPtr<HistoryEntry> newEntry = adoptPtr(new HistoryEntry());
    newEntry->m_root = m_root->cloneAndReplace(newEntry.get(), newItem, oldItem, clipAtTarget, page->mainFrame());
    return newEntry.release();
}

HistoryNode* HistoryEntry::historyNodeForFrame(Frame* frame)
{
    if (HistoryNode* historyNode = m_framesToItems.get(frame->frameID()))
        return historyNode;
    String target = frame->tree().uniqueName();
    if (target.isNull())
        target = emptyString();
    return m_uniqueNamesToItems.get(target);
}

HistoryItem* HistoryEntry::itemForFrame(Frame* frame)
{
    if (HistoryNode* historyNode = historyNodeForFrame(frame))
        return historyNode->value();
    return 0;
}

HistoryController::HistoryController(Page* page)
    : m_page(page)
    , m_defersLoading(false)
{
}

HistoryController::~HistoryController()
{
}

void HistoryController::clearScrollPositionAndViewState()
{
    if (!m_currentEntry)
        return;

    m_currentEntry->root()->clearScrollPoint();
    m_currentEntry->root()->setPageScaleFactor(0);
}

/*
 There is a race condition between the layout and load completion that affects restoring the scroll position.
 We try to restore the scroll position at both the first layout and upon load completion.

 1) If first layout happens before the load completes, we want to restore the scroll position then so that the
 first time we draw the page is already scrolled to the right place, instead of starting at the top and later
 jumping down.  It is possible that the old scroll position is past the part of the doc laid out so far, in
 which case the restore silent fails and we will fix it in when we try to restore on doc completion.
 2) If the layout happens after the load completes, the attempt to restore at load completion time silently
 fails.  We then successfully restore it when the layout happens.
*/
void HistoryController::restoreScrollPositionAndViewState(Frame* frame)
{
    if (!m_currentEntry || !frame->loader().stateMachine()->committedFirstRealDocumentLoad())
        return;

    if (FrameView* view = frame->view()) {
        if (frame->isMainFrame()) {
            if (ScrollingCoordinator* scrollingCoordinator = m_page->scrollingCoordinator())
                scrollingCoordinator->frameViewRootLayerDidChange(view);
        }

        if (!view->wasScrolledByUser()) {
            if (frame->isMainFrame() && m_currentEntry->root()->pageScaleFactor())
                m_page->setPageScaleFactor(m_currentEntry->root()->pageScaleFactor(), m_currentEntry->root()->scrollPoint());
            else
                view->setScrollPositionNonProgrammatically(m_currentEntry->itemForFrame(frame)->scrollPoint());
        }
    }
}

void HistoryController::updateBackForwardListForFragmentScroll(Frame* frame)
{
    createNewBackForwardItem(frame, false);
}

void HistoryController::saveDocumentAndScrollState(Frame* frame)
{
    if (!m_currentEntry || !m_currentEntry->itemForFrame(frame))
        return;

    Document* document = frame->document();
    ASSERT(document);
    HistoryItem* item = m_currentEntry->itemForFrame(frame);

    if (item->isCurrentDocument(document) && document->isActive())
        item->setDocumentState(document->formElementsState());

    if (!frame->view())
        return;

    item->setScrollPoint(frame->view()->scrollPosition());
    if (frame->isMainFrame() && !m_page->inspectorController().deviceEmulationEnabled())
        item->setPageScaleFactor(m_page->pageScaleFactor());
}

void HistoryController::restoreDocumentState(Frame* frame)
{
    HistoryItem* item = m_currentEntry ? m_currentEntry->itemForFrame(frame) : 0;
    if (item && frame->loader().loadType() == FrameLoadTypeBackForward)
        frame->document()->setStateForNewFormElements(item->documentState());
}

void HistoryController::goToEntry(PassOwnPtr<HistoryEntry> targetEntry)
{
    ASSERT(m_sameDocumentLoadsInProgress.isEmpty());
    ASSERT(m_differentDocumentLoadsInProgress.isEmpty());

    m_provisionalEntry = targetEntry;
    recursiveGoToEntry(m_page->mainFrame());

    if (m_differentDocumentLoadsInProgress.isEmpty()) {
        m_previousEntry = m_currentEntry.release();
        m_currentEntry = m_provisionalEntry.release();
    } else {
        m_page->mainFrame()->loader().stopAllLoaders();
    }

    for (HistoryFrameLoadSet::iterator it = m_sameDocumentLoadsInProgress.begin(); it != m_sameDocumentLoadsInProgress.end(); ++it)
        it->key->loader().loadHistoryItem(it->value, HistorySameDocumentLoad);
    for (HistoryFrameLoadSet::iterator it = m_differentDocumentLoadsInProgress.begin(); it != m_differentDocumentLoadsInProgress.end(); ++it)
        it->key->loader().loadHistoryItem(it->value, HistoryDifferentDocumentLoad);
    m_sameDocumentLoadsInProgress.clear();
    m_differentDocumentLoadsInProgress.clear();
}

void HistoryController::recursiveGoToEntry(Frame* frame)
{
    HistoryItem* newItem = m_provisionalEntry->itemForFrame(frame);
    HistoryItem* oldItem = m_currentEntry ? m_currentEntry->itemForFrame(frame) : 0;
    if (!newItem)
        return;

    if (!oldItem || (newItem != oldItem && newItem->itemSequenceNumber() != oldItem->itemSequenceNumber())) {
        if (oldItem && newItem->documentSequenceNumber() == oldItem->documentSequenceNumber())
            m_sameDocumentLoadsInProgress.set(frame, newItem);
        else
            m_differentDocumentLoadsInProgress.set(frame, newItem);
        return;
    }

    for (Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling())
        recursiveGoToEntry(child);
}

void HistoryController::goToItem(HistoryItem* targetItem)
{
    if (m_defersLoading) {
        m_deferredItem = targetItem;
        return;
    }

    OwnPtr<HistoryEntry> newEntry = HistoryEntry::create(targetItem);
    Deque<HistoryNode*> historyNodes;
    historyNodes.append(newEntry->rootHistoryNode());
    while (!historyNodes.isEmpty()) {
        // For each item, read the children (if any) off the HistoryItem,
        // create a new HistoryNode for each child and attach it,
        // then clear the children on the HistoryItem.
        HistoryNode* historyNode = historyNodes.takeFirst();
        const HistoryItemVector& children = historyNode->value()->children();
        for (size_t i = 0; i < children.size(); i++) {
            HistoryNode* childHistoryNode = historyNode->addChild(children[i].get());
            historyNodes.append(childHistoryNode);
        }
        historyNode->value()->clearChildren();
    }
    goToEntry(newEntry.release());
}

void HistoryController::setDefersLoading(bool defer)
{
    m_defersLoading = defer;
    if (!defer && m_deferredItem) {
        goToItem(m_deferredItem.get());
        m_deferredItem = 0;
    }
}

// There are 2 things you might think of as "history", all of which are handled by these functions.
//
//     1) Back/forward: The m_currentItem is part of this mechanism.
//     2) Global history: Handled by the client.
//
void HistoryController::updateForStandardLoad(Frame* frame)
{
    LOG(History, "WebCoreHistory: Updating History for Standard Load in frame %s", frame->loader().documentLoader()->url().string().ascii().data());
    createNewBackForwardItem(frame, true);
}

void HistoryController::updateForInitialLoadInChildFrame(Frame* frame)
{
    ASSERT(frame->tree().parent());
    if (!m_currentEntry)
        return;
    if (HistoryNode* existingChildHistoryNode = m_currentEntry->historyNodeForFrame(frame))
        existingChildHistoryNode->updateValue(createItem(frame));
    else if (HistoryNode* parentHistoryNode = m_currentEntry->historyNodeForFrame(frame->tree().parent()))
        parentHistoryNode->addChild(createItem(frame));
}

void HistoryController::updateForCommit(Frame* frame)
{
#if !LOG_DISABLED
    if (frame->document())
        LOG(History, "WebCoreHistory: Updating History for commit in frame %s", frame->document()->title().utf8().data());
#endif
    FrameLoadType type = frame->loader().loadType();
    if (isBackForwardLoadType(type)) {
        // Once committed, we want to use current item for saving DocState, and
        // the provisional item for restoring state.
        // Note previousItem must be set before we close the URL, which will
        // happen when the data source is made non-provisional below
        if (m_provisionalEntry) {
            m_previousEntry = m_currentEntry.release();
            ASSERT(m_provisionalEntry);
            m_currentEntry = m_provisionalEntry.release();
        }
        frame->loader().setCurrentItem(m_currentEntry->itemForFrame(frame));
    } else if (type != FrameLoadTypeRedirectWithLockedBackForwardList) {
        m_provisionalEntry.clear();
    }

    if (type == FrameLoadTypeStandard)
        updateForStandardLoad(frame);
    else if (type == FrameLoadTypeInitialInChildFrame)
        updateForInitialLoadInChildFrame(frame);
    else
        updateWithoutCreatingNewBackForwardItem(frame);
}

void HistoryController::updateForSameDocumentNavigation(Frame* frame)
{
    if (frame->document()->url().isEmpty())
        return;
    if (HistoryItem* item = m_currentEntry->itemForFrame(frame))
        item->setURL(frame->document()->url());
}

static PassRefPtr<HistoryItem> itemForExport(HistoryNode* historyNode)
{
    RefPtr<HistoryItem> item = historyNode->value()->copy();
    const Vector<OwnPtr<HistoryNode> >& childEntries = historyNode->children();
    for (size_t i = 0; i < childEntries.size(); i++)
        item->addChildItem(itemForExport(childEntries[i].get()));
    return item;
}

PassRefPtr<HistoryItem> HistoryController::currentItemForExport(Frame* frame)
{
    if (!m_currentEntry)
        return 0;
    HistoryNode* historyNode = m_currentEntry->historyNodeForFrame(frame);
    return historyNode ? itemForExport(historyNode) : 0;
}

PassRefPtr<HistoryItem> HistoryController::previousItemForExport(Frame* frame)
{
    if (!m_previousEntry)
        return 0;
    HistoryNode* historyNode = m_previousEntry->historyNodeForFrame(frame);
    return historyNode ? itemForExport(historyNode) : 0;
}

PassRefPtr<HistoryItem> HistoryController::provisionalItemForExport(Frame* frame)
{
    if (!m_provisionalEntry)
        return 0;
    HistoryNode* historyNode = m_provisionalEntry->historyNodeForFrame(frame);
    return historyNode ? itemForExport(historyNode) : 0;
}

HistoryItem* HistoryController::currentItem(Frame* frame) const
{
    return m_currentEntry ? m_currentEntry->itemForFrame(frame) : 0;
}

HistoryItem* HistoryController::previousItem(Frame* frame) const
{
    return m_previousEntry ? m_previousEntry->itemForFrame(frame) : 0;
}

void HistoryController::clearProvisionalEntry()
{
    m_provisionalEntry.clear();
}

void HistoryController::initializeItem(HistoryItem* item, Frame* frame)
{
    DocumentLoader* documentLoader = frame->loader().documentLoader();
    ASSERT(documentLoader);

    KURL unreachableURL = documentLoader->unreachableURL();

    KURL url;
    KURL originalURL;

    if (!unreachableURL.isEmpty()) {
        url = unreachableURL;
        originalURL = unreachableURL;
    } else {
        url = documentLoader->url();
        originalURL = documentLoader->originalURL();
    }

    // Frames that have never successfully loaded any content
    // may have no URL at all. Currently our history code can't
    // deal with such things, so we nip that in the bud here.
    // Later we may want to learn to live with nil for URL.
    // See bug 3368236 and related bugs for more information.
    if (url.isEmpty())
        url = blankURL();
    if (originalURL.isEmpty())
        originalURL = blankURL();

    item->setURL(url);
    item->setTarget(frame->tree().uniqueName());
    item->setTargetFrameID(frame->frameID());
    item->setOriginalURLString(originalURL.string());

    // Save form state if this is a POST
    item->setFormInfoFromRequest(documentLoader->request());
}

PassRefPtr<HistoryItem> HistoryController::createItem(Frame* frame)
{
    RefPtr<HistoryItem> item = HistoryItem::create();
    initializeItem(item.get(), frame);
    frame->loader().setCurrentItem(item.get());
    return item.release();
}

void HistoryController::createItemTree(Frame* targetFrame, bool clipAtTarget)
{
    RefPtr<HistoryItem> newItem = createItem(targetFrame);
    if (!m_currentEntry) {
        m_currentEntry = HistoryEntry::create(newItem.get());
    } else {
        HistoryItem* oldItem = m_currentEntry->itemForFrame(targetFrame);
        if (!clipAtTarget)
            newItem->setDocumentSequenceNumber(oldItem->documentSequenceNumber());
        m_previousEntry = m_currentEntry.release();
        m_currentEntry = m_previousEntry->cloneAndReplace(newItem.get(), oldItem, clipAtTarget, m_page);
    }
}

void HistoryController::createNewBackForwardItem(Frame* frame, bool doClip)
{
    // In the case of saving state about a page with frames, we store a tree of items that mirrors the frame tree.
    // The item that was the target of the user's navigation is designated as the "targetItem".
    // When this function is called with doClip=true we're able to create the whole tree except for the target's children,
    // which will be loaded in the future. That part of the tree will be filled out as the child loads are committed.
    if (!frame->loader().documentLoader()->isURLValidForNewHistoryEntry())
        return;
    createItemTree(frame, doClip);
}

void HistoryController::updateWithoutCreatingNewBackForwardItem(Frame* frame)
{
    if (!m_currentEntry || !m_currentEntry->itemForFrame(frame))
        return;

    DocumentLoader* documentLoader = frame->loader().documentLoader();

    if (!documentLoader->unreachableURL().isEmpty())
        return;

    HistoryItem* item = m_currentEntry->itemForFrame(frame);
    if (item->url() != documentLoader->url()) {
        item->reset();
        initializeItem(item, frame);
    } else {
        // Even if the final URL didn't change, the form data may have changed.
        item->setFormInfoFromRequest(documentLoader->request());
    }
}

void HistoryController::pushState(Frame* frame, PassRefPtr<SerializedScriptValue> stateObject, const String& urlString)
{
    if (!m_currentEntry)
        return;

    // Get a HistoryItem tree for the current frame tree, then override data to reflect
    // the pushState() arguments.
    createItemTree(frame, false);
    HistoryItem* item = m_currentEntry->itemForFrame(frame);
    if (!item) {
        updateForInitialLoadInChildFrame(frame);
        item = m_currentEntry->itemForFrame(frame);
    }
    item->setStateObject(stateObject);
    item->setURLString(urlString);
}

void HistoryController::replaceState(Frame* frame, PassRefPtr<SerializedScriptValue> stateObject, const String& urlString)
{
    if (!m_currentEntry)
        return;

    HistoryItem* item = m_currentEntry->itemForFrame(frame);
    if (!item)
        return;

    if (!urlString.isEmpty())
        item->setURLString(urlString);
    item->setStateObject(stateObject);
    item->setFormData(0);
    item->setFormContentType(String());
}

} // namespace WebCore
