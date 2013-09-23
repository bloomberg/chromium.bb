/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "core/loader/NavigationAction.h"

#include "core/events/MouseEvent.h"
#include "core/loader/FrameLoader.h"

namespace WebCore {

static NavigationType navigationType(FrameLoadType frameLoadType, bool isFormSubmission, bool haveEvent)
{
    bool isReload = frameLoadType == FrameLoadTypeReload || frameLoadType == FrameLoadTypeReloadFromOrigin;
    bool isBackForward = isBackForwardLoadType(frameLoadType);
    if (isFormSubmission)
        return (isReload || isBackForward) ? NavigationTypeFormResubmitted : NavigationTypeFormSubmitted;
    if (haveEvent)
        return NavigationTypeLinkClicked;
    if (isReload)
        return NavigationTypeReload;
    if (isBackForward)
        return NavigationTypeBackForward;
    return NavigationTypeOther;
}

NavigationAction::NavigationAction()
    : m_type(NavigationTypeOther)
{
}

NavigationAction::NavigationAction(const ResourceRequest& resourceRequest, FrameLoadType frameLoadType,
        bool isFormSubmission, PassRefPtr<Event> event)
    : m_resourceRequest(resourceRequest)
    , m_type(navigationType(frameLoadType, isFormSubmission, event))
    , m_event(event)
{
}

bool NavigationAction::specifiesNavigationPolicy(NavigationPolicy* policy) const
{
    const MouseEvent* event = 0;
    if (m_type == NavigationTypeLinkClicked && m_event->isMouseEvent())
        event = toMouseEvent(m_event.get());
    else if (m_type == NavigationTypeFormSubmitted && m_event && m_event->underlyingEvent() && m_event->underlyingEvent()->isMouseEvent())
        event = toMouseEvent(m_event->underlyingEvent());

    if (!event)
        return false;
    return navigationPolicyFromMouseEvent(event->button(), event->ctrlKey(), event->shiftKey(), event->altKey(), event->metaKey(), policy);
}

}
