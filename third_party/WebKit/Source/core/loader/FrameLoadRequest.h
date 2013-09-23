/*
 * Copyright (C) 2003, 2006, 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FrameLoadRequest_h
#define FrameLoadRequest_h

#include "core/events/Event.h"
#include "core/html/HTMLFormElement.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/SubstituteData.h"
#include "weborigin/SecurityOrigin.h"
#include "core/platform/network/ResourceRequest.h"

namespace WebCore {
class Frame;

struct FrameLoadRequest {
public:
    explicit FrameLoadRequest(SecurityOrigin* requester)
        : m_requester(requester)
        , m_lockBackForwardList(false)
        , m_clientRedirect(false)
        , m_shouldSendReferrer(MaybeSendReferrer)
    {
    }

    FrameLoadRequest(SecurityOrigin* requester, const ResourceRequest& resourceRequest)
        : m_requester(requester)
        , m_resourceRequest(resourceRequest)
        , m_lockBackForwardList(false)
        , m_clientRedirect(false)
        , m_shouldSendReferrer(MaybeSendReferrer)
    {
    }

    FrameLoadRequest(SecurityOrigin* requester, const ResourceRequest& resourceRequest, const String& frameName)
        : m_requester(requester)
        , m_resourceRequest(resourceRequest)
        , m_frameName(frameName)
        , m_lockBackForwardList(false)
        , m_clientRedirect(false)
        , m_shouldSendReferrer(MaybeSendReferrer)
    {
    }

    FrameLoadRequest(SecurityOrigin* requester, const ResourceRequest& resourceRequest, const SubstituteData& substituteData)
        : m_requester(requester)
        , m_resourceRequest(resourceRequest)
        , m_substituteData(substituteData)
        , m_lockBackForwardList(false)
        , m_clientRedirect(false)
        , m_shouldSendReferrer(MaybeSendReferrer)
    {
    }

    const SecurityOrigin* requester() const { return m_requester.get(); }

    ResourceRequest& resourceRequest() { return m_resourceRequest; }
    const ResourceRequest& resourceRequest() const { return m_resourceRequest; }

    const String& frameName() const { return m_frameName; }
    void setFrameName(const String& frameName) { m_frameName = frameName; }

    const SubstituteData& substituteData() const { return m_substituteData; }

    bool lockBackForwardList() const { return m_lockBackForwardList; }
    void setLockBackForwardList(bool lockBackForwardList) { m_lockBackForwardList = lockBackForwardList; }

    bool clientRedirect() const { return m_clientRedirect; }
    void setClientRedirect(bool clientRedirect) { m_clientRedirect = clientRedirect; }

    Event* triggeringEvent() const { return m_triggeringEvent.get(); }
    void setTriggeringEvent(PassRefPtr<Event> triggeringEvent) { m_triggeringEvent = triggeringEvent; }

    FormState* formState() const { return m_formState.get(); }
    void setFormState(PassRefPtr<FormState> formState) { m_formState = formState; }

    ShouldSendReferrer shouldSendReferrer() const { return m_shouldSendReferrer; }
    void setShouldSendReferrer(ShouldSendReferrer shouldSendReferrer) { m_shouldSendReferrer = shouldSendReferrer; }

private:
    RefPtr<SecurityOrigin> m_requester;
    ResourceRequest m_resourceRequest;
    String m_frameName;
    SubstituteData m_substituteData;
    bool m_lockBackForwardList;
    bool m_clientRedirect;
    RefPtr<Event> m_triggeringEvent;
    RefPtr<FormState> m_formState;
    ShouldSendReferrer m_shouldSendReferrer;
};

}

#endif // FrameLoadRequest_h
