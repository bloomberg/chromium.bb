/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/history/BackForwardController.h"

#include "core/history/BackForwardClient.h"
#include "core/history/HistoryItem.h"
#include "core/page/Page.h"

namespace WebCore {

BackForwardController::BackForwardController(Page* page, BackForwardClient* client)
    : m_page(page)
    , m_client(client)
{
    ASSERT(m_client);
}

BackForwardController::~BackForwardController()
{
}

PassOwnPtr<BackForwardController> BackForwardController::create(Page* page, BackForwardClient* client)
{
    return adoptPtr(new BackForwardController(page, client));
}

void BackForwardController::goBackOrForward(int distance)
{
    if (distance == 0)
        return;

    HistoryItem* item = itemAtIndex(distance);
    if (!item) {
        if (distance > 0) {
            if (forwardCount())
                item = itemAtIndex(forwardCount());
        } else {
            if (backCount())
                item = itemAtIndex(-backCount());
        }
    }

    if (!item)
        return;

    m_page->goToItem(item);
}

bool BackForwardController::goBack()
{
    HistoryItem* item = backItem();

    if (item) {
        m_page->goToItem(item);
        return true;
    }
    return false;
}

bool BackForwardController::goForward()
{
    HistoryItem* item = forwardItem();

    if (item) {
        m_page->goToItem(item);
        return true;
    }
    return false;
}

void BackForwardController::addItem(PassRefPtr<HistoryItem> item)
{
    m_client->addItem(item);
}

void BackForwardController::setCurrentItem(HistoryItem* item)
{
    m_client->goToItem(item);
}

int BackForwardController::count() const
{
    return backCount() + 1 + forwardCount();
}

int BackForwardController::backCount() const
{
    return m_client->backListCount();
}

int BackForwardController::forwardCount() const
{
    return m_client->forwardListCount();
}

HistoryItem* BackForwardController::itemAtIndex(int i)
{
    return m_client->itemAtIndex(i);
}

bool BackForwardController::isActive()
{
    return m_client->isActive();
}

void BackForwardController::close()
{
    m_client->close();
}

} // namespace WebCore
