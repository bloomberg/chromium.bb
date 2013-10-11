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
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/frame/Frame.h"
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

bool BackForwardController::goBackOrForward(int distance)
{
    ASSERT(distance);
    if (distance > forwardCount())
        distance = forwardCount();
    else if (distance < -backCount())
        distance = backCount();

    if (!distance)
        return false;
    m_page->mainFrame()->loader()->client()->navigateBackForward(distance);
    return true;
}

void BackForwardController::addItem(PassRefPtr<HistoryItem> item)
{
    m_currentItem = item;
    m_client->didAddItem();
}

void BackForwardController::setCurrentItem(HistoryItem* item)
{
    m_currentItem = item;
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

} // namespace WebCore
