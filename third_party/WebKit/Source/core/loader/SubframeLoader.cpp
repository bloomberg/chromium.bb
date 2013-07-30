/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
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
#include "core/loader/SubframeLoader.h"

#include "HTMLNames.h"
#include "bindings/v8/ScriptController.h"
#include "core/html/HTMLAppletElement.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLPlugInImageElement.h"
#include "core/html/PluginDocument.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/MIMETypeRegistry.h"
#include "core/plugins/PluginData.h"
#include "core/rendering/RenderEmbeddedObject.h"
#include "core/rendering/RenderView.h"
#include "weborigin/SecurityOrigin.h"
#include "weborigin/SecurityPolicy.h"

namespace WebCore {

using namespace HTMLNames;

SubframeLoader::SubframeLoader(Frame* frame)
    :  m_frame(frame)
{
}

bool SubframeLoader::loadOrRedirectSubframe(HTMLFrameOwnerElement* ownerElement, const KURL& url, const AtomicString& frameName, bool lockBackForwardList)
{
    if (Frame* frame = ownerElement->contentFrame()) {
        frame->navigationScheduler()->scheduleLocationChange(m_frame->document()->securityOrigin(), url.string(), m_frame->loader()->outgoingReferrer(), lockBackForwardList);
        return true;
    }

    return loadSubframe(ownerElement, url, frameName, m_frame->loader()->outgoingReferrer());
}

bool SubframeLoader::loadSubframe(HTMLFrameOwnerElement* ownerElement, const KURL& url, const String& name, const String& referrer)
{
    RefPtr<Frame> protect(m_frame);

    bool allowsScrolling = true;
    int marginWidth = -1;
    int marginHeight = -1;
    if (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag)) {
        HTMLFrameElementBase* o = static_cast<HTMLFrameElementBase*>(ownerElement);
        allowsScrolling = o->scrollingMode() != ScrollbarAlwaysOff;
        marginWidth = o->marginWidth();
        marginHeight = o->marginHeight();
    }

    if (!ownerElement->document()->securityOrigin()->canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(m_frame, url.string());
        return false;
    }

    String referrerToUse = SecurityPolicy::generateReferrerHeader(ownerElement->document()->referrerPolicy(), url, referrer);
    RefPtr<Frame> frame = m_frame->loader()->client()->createFrame(url, name, ownerElement, referrerToUse, allowsScrolling, marginWidth, marginHeight);

    if (!frame)  {
        m_frame->loader()->checkCallImplicitClose();
        return false;
    }

    // All new frames will have m_isComplete set to true at this point due to synchronously loading
    // an empty document in FrameLoader::init(). But many frames will now be starting an
    // asynchronous load of url, so we set m_isComplete to false and then check if the load is
    // actually completed below. (Note that we set m_isComplete to false even for synchronous
    // loads, so that checkCompleted() below won't bail early.)
    // FIXME: Can we remove this entirely? m_isComplete normally gets set to false when a load is committed.
    frame->loader()->started();

    RenderObject* renderer = ownerElement->renderer();
    FrameView* view = frame->view();
    if (renderer && renderer->isWidget() && view)
        toRenderWidget(renderer)->setWidget(view);

    m_frame->loader()->checkCallImplicitClose();

    // Some loads are performed synchronously (e.g., about:blank and loads
    // cancelled by returning a null ResourceRequest from requestFromDelegate).
    // In these cases, the synchronous load would have finished
    // before we could connect the signals, so make sure to send the
    // completed() signal for the child by hand and mark the load as being
    // complete.
    // FIXME: In this case the Frame will have finished loading before
    // it's being added to the child list. It would be a good idea to
    // create the child first, then invoke the loader separately.
    if (frame->loader()->state() == FrameStateComplete && !frame->loader()->policyDocumentLoader())
        frame->loader()->checkCompleted();
    return true;
}

Document* SubframeLoader::document() const
{
    return m_frame->document();
}

KURL SubframeLoader::completeURL(const String& url) const
{
    ASSERT(m_frame->document());
    return m_frame->document()->completeURL(url);
}

} // namespace WebCore
