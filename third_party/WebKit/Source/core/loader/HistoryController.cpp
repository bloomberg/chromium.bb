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

#include "core/loader/FrameLoader.h"
#include "core/frame/Frame.h"
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
            HistoryNode* childHistoryNode = m_entry->m_framesToItems.get(child->frameID());
            if (!childHistoryNode) {
                if (targetFrame == child)
                    newHistoryNode->m_children.append(create(newEntry, newItem));
                continue;
            }
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
{
}

HistoryController::~HistoryController()
{
}

void HistoryController::updateBackForwardListForFragmentScroll(Frame* frame, HistoryItem* item)
{
    m_provisionalEntry.clear();
    createNewBackForwardItem(frame, item, false);
}

void HistoryController::goToEntry(PassOwnPtr<HistoryEntry> targetEntry)
{
    ASSERT(m_sameDocumentLoadsInProgress.isEmpty());
    ASSERT(m_differentDocumentLoadsInProgress.isEmpty());

    m_provisionalEntry = targetEntry;
    recursiveGoToEntry(m_page->mainFrame());

    if (m_sameDocumentLoadsInProgress.isEmpty() && m_differentDocumentLoadsInProgress.isEmpty())
        m_sameDocumentLoadsInProgress.set(m_page->mainFrame(), m_provisionalEntry->root());

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

void HistoryController::updateForInitialLoadInChildFrame(Frame* frame, HistoryItem* item)
{
    ASSERT(frame->tree().parent());
    if (!m_currentEntry)
        return;
    if (HistoryNode* existingChildHistoryNode = m_currentEntry->historyNodeForFrame(frame))
        existingChildHistoryNode->updateValue(item);
    else if (HistoryNode* parentHistoryNode = m_currentEntry->historyNodeForFrame(frame->tree().parent()))
        parentHistoryNode->addChild(item);
}

void HistoryController::updateForCommit(Frame* frame, HistoryItem* item)
{
    FrameLoadType type = frame->loader().loadType();
    if (isBackForwardLoadType(type) && m_provisionalEntry) {
        // Once committed, we want to use current item for saving DocState, and
        // the provisional item for restoring state.
        // Note previousItem must be set before we close the URL, which will
        // happen when the data source is made non-provisional below
        m_previousEntry = m_currentEntry.release();
        ASSERT(m_provisionalEntry);
        m_currentEntry = m_provisionalEntry.release();
    } else if (type != FrameLoadTypeRedirectWithLockedBackForwardList) {
        m_provisionalEntry.clear();
    }

    if (type == FrameLoadTypeStandard)
        createNewBackForwardItem(frame, item, true);
    else if (type == FrameLoadTypeInitialInChildFrame)
        updateForInitialLoadInChildFrame(frame, item);
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

HistoryItem* HistoryController::itemForNewChildFrame(Frame* frame) const
{
    return m_currentEntry ? m_currentEntry->itemForFrame(frame) : 0;
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
