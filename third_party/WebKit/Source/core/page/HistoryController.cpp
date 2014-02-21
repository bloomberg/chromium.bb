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
#include "core/page/HistoryController.h"

#include "core/frame/Frame.h"
#include "core/loader/FrameLoader.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "wtf/Deque.h"
#include "wtf/text/StringHash.h"

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

PassOwnPtr<HistoryNode> HistoryNode::cloneAndReplace(HistoryEntry* newEntry, HistoryItem* newItem, bool clipAtTarget, Frame* targetFrame, Frame* currentFrame)
{
    bool isNodeBeingNavigated = targetFrame == currentFrame;
    HistoryItem* itemForCreate = isNodeBeingNavigated ? newItem : m_value.get();
    OwnPtr<HistoryNode> newHistoryNode = create(newEntry, itemForCreate);

    if (!clipAtTarget || !isNodeBeingNavigated) {
        for (Frame* child = currentFrame->tree().firstChild(); child; child = child->tree().nextSibling()) {
            HistoryNode* childHistoryNode = m_entry->historyNodeForFrame(child);
            if (!childHistoryNode)
                continue;
            newHistoryNode->m_children.append(childHistoryNode->cloneAndReplace(newEntry, newItem, clipAtTarget, targetFrame, child));
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

void HistoryNode::removeChildren()
{
    // FIXME: This is inefficient. Figure out a cleaner way to ensure this HistoryNode isn't cached anywhere.
    for (unsigned i = 0; i < m_children.size(); i++) {
        m_children[i]->removeChildren();

        HashMap<uint64_t, HistoryNode*>::iterator framesEnd = m_entry->m_framesToItems.end();
        HashMap<String, HistoryNode*>::iterator uniqueNamesEnd = m_entry->m_uniqueNamesToItems.end();
        for (HashMap<uint64_t, HistoryNode*>::iterator it = m_entry->m_framesToItems.begin(); it != framesEnd; ++it) {
            if (it->value == m_children[i])
                m_entry->m_framesToItems.remove(it);
        }
        for (HashMap<String, HistoryNode*>::iterator it = m_entry->m_uniqueNamesToItems.begin(); it != uniqueNamesEnd; ++it) {
            if (it->value == m_children[i])
                m_entry->m_uniqueNamesToItems.remove(it);
        }
    }
    m_children.clear();
}

HistoryEntry::HistoryEntry(HistoryItem* root)
{
    m_root = HistoryNode::create(this, root);
}

PassOwnPtr<HistoryEntry> HistoryEntry::create(HistoryItem* root)
{
    return adoptPtr(new HistoryEntry(root));
}

PassOwnPtr<HistoryEntry> HistoryEntry::cloneAndReplace(HistoryItem* newItem, bool clipAtTarget, Frame* targetFrame, Page* page)
{
    OwnPtr<HistoryEntry> newEntry = adoptPtr(new HistoryEntry());
    newEntry->m_root = m_root->cloneAndReplace(newEntry.get(), newItem, clipAtTarget, targetFrame, page->mainFrame());
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
    , m_deferredCachePolicy(UseProtocolCachePolicy)
{
}

HistoryController::~HistoryController()
{
}

void HistoryController::updateBackForwardListForFragmentScroll(Frame* frame, HistoryItem* item)
{
    createNewBackForwardItem(frame, item, false);
}

void HistoryController::goToEntry(PassOwnPtr<HistoryEntry> targetEntry, ResourceRequestCachePolicy cachePolicy)
{
    HistoryFrameLoadSet sameDocumentLoads;
    HistoryFrameLoadSet differentDocumentLoads;

    m_provisionalEntry = targetEntry;
    if (m_currentEntry)
        recursiveGoToEntry(m_page->mainFrame(), sameDocumentLoads, differentDocumentLoads);
    else
        differentDocumentLoads.set(m_page->mainFrame(), m_provisionalEntry->root());

    if (sameDocumentLoads.isEmpty() && differentDocumentLoads.isEmpty())
        sameDocumentLoads.set(m_page->mainFrame(), m_provisionalEntry->root());

    if (differentDocumentLoads.isEmpty()) {
        m_previousEntry = m_currentEntry.release();
        m_currentEntry = m_provisionalEntry.release();
    }

    for (HistoryFrameLoadSet::iterator it = sameDocumentLoads.begin(); it != sameDocumentLoads.end(); ++it) {
        if (it->key->host())
            it->key->loader().loadHistoryItem(it->value.get(), HistorySameDocumentLoad, cachePolicy);
    }
    for (HistoryFrameLoadSet::iterator it = differentDocumentLoads.begin(); it != differentDocumentLoads.end(); ++it) {
        if (it->key->host())
            it->key->loader().loadHistoryItem(it->value.get(), HistoryDifferentDocumentLoad, cachePolicy);
    }
}

void HistoryController::recursiveGoToEntry(Frame* frame, HistoryFrameLoadSet& sameDocumentLoads, HistoryFrameLoadSet& differentDocumentLoads)
{
    ASSERT(m_provisionalEntry);
    ASSERT(m_currentEntry);
    HistoryItem* newItem = m_provisionalEntry->itemForFrame(frame);
    HistoryItem* oldItem = m_currentEntry->itemForFrame(frame);
    if (!newItem)
        return;

    if (!oldItem || (newItem != oldItem && newItem->itemSequenceNumber() != oldItem->itemSequenceNumber())) {
        if (oldItem && newItem->documentSequenceNumber() == oldItem->documentSequenceNumber())
            sameDocumentLoads.set(frame, newItem);
        else
            differentDocumentLoads.set(frame, newItem);
        return;
    }

    for (Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling())
        recursiveGoToEntry(child, sameDocumentLoads, differentDocumentLoads);
}

void HistoryController::goToItem(HistoryItem* targetItem, ResourceRequestCachePolicy cachePolicy)
{
    if (m_defersLoading) {
        m_deferredItem = targetItem;
        m_deferredCachePolicy = cachePolicy;
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
    goToEntry(newEntry.release(), cachePolicy);
}

void HistoryController::setDefersLoading(bool defer)
{
    m_defersLoading = defer;
    if (!defer && m_deferredItem) {
        goToItem(m_deferredItem.get(), m_deferredCachePolicy);
        m_deferredItem = nullptr;
        m_deferredCachePolicy = UseProtocolCachePolicy;
    }
}

void HistoryController::updateForInitialLoadInChildFrame(Frame* frame, HistoryItem* item)
{
    ASSERT(frame->tree().parent());
    if (!m_currentEntry)
        return;
    if (HistoryNode* existingNode = m_currentEntry->historyNodeForFrame(frame))
        existingNode->updateValue(item);
    else if (HistoryNode* parentHistoryNode = m_currentEntry->historyNodeForFrame(frame->tree().parent()))
        parentHistoryNode->addChild(item);
}

void HistoryController::updateForCommit(Frame* frame, HistoryItem* item, HistoryCommitType commitType)
{
    if (commitType == BackForwardCommit) {
        if (!m_provisionalEntry)
            return;
        // Once committed, we want to use current item for saving DocState, and
        // the provisional item for restoring state.
        // Note previousItem must be set before we close the URL, which will
        // happen when the data source is made non-provisional below
        m_previousEntry = m_currentEntry.release();
        ASSERT(m_provisionalEntry);
        m_currentEntry = m_provisionalEntry.release();
    } else if (commitType == StandardCommit) {
        createNewBackForwardItem(frame, item, true);
    } else if (commitType == InitialCommitInChildFrame) {
        updateForInitialLoadInChildFrame(frame, item);
    }
}

static PassRefPtr<HistoryItem> itemForExport(HistoryNode* historyNode)
{
    ASSERT(historyNode);
    RefPtr<HistoryItem> item = historyNode->value()->copy();
    const Vector<OwnPtr<HistoryNode> >& childEntries = historyNode->children();
    for (size_t i = 0; i < childEntries.size(); i++)
        item->addChildItem(itemForExport(childEntries[i].get()));
    return item;
}

PassRefPtr<HistoryItem> HistoryController::currentItemForExport()
{
    if (!m_currentEntry)
        return nullptr;
    return itemForExport(m_currentEntry->rootHistoryNode());
}

PassRefPtr<HistoryItem> HistoryController::previousItemForExport()
{
    if (!m_previousEntry)
        return nullptr;
    return itemForExport(m_previousEntry->rootHistoryNode());
}

HistoryItem* HistoryController::itemForNewChildFrame(Frame* frame) const
{
    return m_currentEntry ? m_currentEntry->itemForFrame(frame) : 0;
}

void HistoryController::removeChildrenForRedirect(Frame* frame)
{
    if (!m_provisionalEntry)
        return;
    if (HistoryNode* node = m_provisionalEntry->historyNodeForFrame(frame))
        node->removeChildren();
}

void HistoryController::createNewBackForwardItem(Frame* targetFrame, HistoryItem* item, bool clipAtTarget)
{
    RefPtr<HistoryItem> newItem = item;
    if (!m_currentEntry) {
        m_currentEntry = HistoryEntry::create(newItem.get());
    } else {
        HistoryItem* oldItem = m_currentEntry->itemForFrame(targetFrame);
        if (!clipAtTarget && oldItem)
            newItem->setDocumentSequenceNumber(oldItem->documentSequenceNumber());
        m_previousEntry = m_currentEntry.release();
        m_currentEntry = m_previousEntry->cloneAndReplace(newItem.get(), clipAtTarget, targetFrame, m_page);
    }
}

} // namespace WebCore
