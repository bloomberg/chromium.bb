/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/PendingScript.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptStreamer.h"
#include "core/dom/Element.h"
#include "core/fetch/ScriptResource.h"

namespace blink {

PendingScript::~PendingScript()
{
}

void PendingScript::watchForLoad(ScriptResourceClient* client)
{
    ASSERT(!m_watchingForLoad);
    ASSERT(!isReady());
    if (m_streamer) {
        m_streamer->addClient(client);
    } else {
        // addClient() will call notifyFinished() if the load is
        // complete. Callers do not expect to be re-entered from this call, so
        // they should not become a client of an already-loaded Resource.
        resource()->addClient(client);
    }
    m_watchingForLoad = true;
}

void PendingScript::stopWatchingForLoad(ScriptResourceClient* client)
{
    if (!m_watchingForLoad)
        return;
    ASSERT(resource());
    if (m_streamer) {
        m_streamer->cancel();
        m_streamer->removeClient(client);
        m_streamer.clear();
    } else {
        resource()->removeClient(client);
    }
    m_watchingForLoad = false;
}

PassRefPtrWillBeRawPtr<Element> PendingScript::releaseElementAndClear()
{
    setScriptResource(0);
    m_watchingForLoad = false;
    m_startingPosition = TextPosition::belowRangePosition();
    m_streamer.release();
    return m_element.release();
}

void PendingScript::setScriptResource(ScriptResource* resource)
{
    setResource(resource);
}

void PendingScript::notifyFinished(Resource* resource)
{
    if (m_streamer)
        m_streamer->notifyFinished(resource);
}

void PendingScript::notifyAppendData(ScriptResource* resource)
{
    if (m_streamer)
        m_streamer->notifyAppendData(resource);
}

void PendingScript::trace(Visitor* visitor)
{
    visitor->trace(m_element);
}

ScriptSourceCode PendingScript::getSource(const KURL& documentURL, bool& errorOccurred) const
{
    if (resource()) {
        errorOccurred = resource()->errorOccurred();
        ASSERT(resource()->isLoaded());
        if (m_streamer && !m_streamer->streamingSuppressed())
            return ScriptSourceCode(m_streamer, resource());
        return ScriptSourceCode(resource());
    }
    errorOccurred = false;
    return ScriptSourceCode(m_element->textContent(), documentURL, startingPosition());
}

bool PendingScript::isReady() const
{
    if (resource() && !resource()->isLoaded())
        return false;
    if (m_streamer && !m_streamer->isFinished())
        return false;
    return true;
}

}
