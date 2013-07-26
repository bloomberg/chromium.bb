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
#include "core/loader/ResourceLoadNotifier.h"

#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/ProgressTracker.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"

namespace WebCore {

ResourceLoadNotifier::ResourceLoadNotifier(Frame* frame)
    : m_frame(frame)
{
}

void ResourceLoadNotifier::dispatchWillSendRequest(DocumentLoader* loader, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse, const FetchInitiatorInfo& initiatorInfo)
{
    m_frame->loader()->applyUserAgent(request);
    m_frame->loader()->client()->dispatchWillSendRequest(loader, identifier, request, redirectResponse);
    InspectorInstrumentation::willSendRequest(m_frame, identifier, loader, request, redirectResponse, initiatorInfo);
}

void ResourceLoadNotifier::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r, ResourceLoader* resourceLoader)
{
    if (Page* page = m_frame->page())
        page->progress()->incrementProgress(identifier, r);
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willReceiveResourceResponse(m_frame, identifier, r);
    m_frame->loader()->client()->dispatchDidReceiveResponse(loader, identifier, r);
    InspectorInstrumentation::didReceiveResourceResponse(cookie, identifier, loader, r, resourceLoader);
}

void ResourceLoadNotifier::dispatchDidReceiveData(DocumentLoader*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    if (Page* page = m_frame->page())
        page->progress()->incrementProgress(identifier, data, dataLength);
    InspectorInstrumentation::didReceiveData(m_frame, identifier, data, dataLength, encodedDataLength);
}

void ResourceLoadNotifier::dispatchDidFinishLoading(DocumentLoader* loader, unsigned long identifier, double finishTime)
{
    if (Page* page = m_frame->page())
        page->progress()->completeProgress(identifier);
    m_frame->loader()->client()->dispatchDidFinishLoading(loader, identifier);

    InspectorInstrumentation::didFinishLoading(m_frame, identifier, loader, finishTime);
}

void ResourceLoadNotifier::dispatchDidFail(DocumentLoader* loader, unsigned long identifier, const ResourceError& error)
{
    if (Page* page = m_frame->page())
        page->progress()->completeProgress(identifier);
    InspectorInstrumentation::didFailLoading(m_frame, identifier, loader, error);
}

void ResourceLoadNotifier::sendRemainingDelegateMessages(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& response, const char* data, int dataLength, int encodedDataLength, const ResourceError& error)
{
    if (!response.isNull())
        dispatchDidReceiveResponse(loader, identifier, response);

    if (dataLength > 0)
        dispatchDidReceiveData(loader, identifier, data, dataLength, encodedDataLength);

    if (error.isNull())
        dispatchDidFinishLoading(loader, identifier, 0);
    else
        dispatchDidFail(loader, identifier, error);
}

} // namespace WebCore
