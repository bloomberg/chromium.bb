/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "core/page/CreateWindow.h"

#include "core/dom/Document.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/page/WindowFeatures.h"
#include "core/platform/network/ResourceRequest.h"
#include "weborigin/KURL.h"
#include "weborigin/SecurityOrigin.h"
#include "weborigin/SecurityPolicy.h"

namespace WebCore {

static Frame* createWindow(Frame* openerFrame, Frame* lookupFrame, const FrameLoadRequest& request, const WindowFeatures& features, bool& created)
{
    ASSERT(!features.dialog || request.frameName().isEmpty());

    if (!request.frameName().isEmpty() && request.frameName() != "_blank") {
        if (Frame* frame = lookupFrame->loader()->findFrameForNavigation(request.frameName(), openerFrame->document())) {
            if (request.frameName() != "_self") {
                if (Page* page = frame->page())
                    page->chrome().focus();
            }
            created = false;
            return frame;
        }
    }

    // Sandboxed frames cannot open new auxiliary browsing contexts.
    if (openerFrame->document()->isSandboxed(SandboxPopups)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        openerFrame->document()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, "Blocked opening '" + request.resourceRequest().url().elidedString() + "' in a new window because the request was made in a sandboxed frame whose 'allow-popups' permission is not set.");
        return 0;
    }

    if (openerFrame->settings() && !openerFrame->settings()->supportsMultipleWindows()) {
        created = false;
        return openerFrame->tree()->top();
    }

    Page* oldPage = openerFrame->page();
    if (!oldPage)
        return 0;

    Page* page = oldPage->chrome().client().createWindow(openerFrame, request, features);
    if (!page)
        return 0;

    Frame* frame = page->mainFrame();

    frame->loader()->forceSandboxFlags(openerFrame->document()->sandboxFlags());

    if (request.frameName() != "_blank")
        frame->tree()->setName(request.frameName());

    page->chrome().setWindowFeatures(features);

    // 'x' and 'y' specify the location of the window, while 'width' and 'height'
    // specify the size of the viewport. We can only resize the window, so adjust
    // for the difference between the window size and the viewport size.

    FloatRect windowRect = page->chrome().windowRect();
    FloatSize viewportSize = page->chrome().pageRect().size();

    if (features.xSet)
        windowRect.setX(features.x);
    if (features.ySet)
        windowRect.setY(features.y);
    if (features.widthSet)
        windowRect.setWidth(features.width + (windowRect.width() - viewportSize.width()));
    if (features.heightSet)
        windowRect.setHeight(features.height + (windowRect.height() - viewportSize.height()));

    // Ensure non-NaN values, minimum size as well as being within valid screen area.
    FloatRect newWindowRect = DOMWindow::adjustWindowRect(page, windowRect);

    page->chrome().setWindowRect(newWindowRect);
    page->chrome().show();

    created = true;
    return frame;
}

Frame* createWindow(const String& urlString, const AtomicString& frameName, const WindowFeatures& windowFeatures,
    DOMWindow* activeWindow, Frame* firstFrame, Frame* openerFrame, DOMWindow::PrepareDialogFunction function, void* functionContext)
{
    Frame* activeFrame = activeWindow->frame();

    KURL completedURL = urlString.isEmpty() ? KURL(ParsedURLString, emptyString()) : firstFrame->document()->completeURL(urlString);
    if (!completedURL.isEmpty() && !completedURL.isValid()) {
        // Don't expose client code to invalid URLs.
        activeWindow->printErrorMessage("Unable to open a window with invalid URL '" + completedURL.string() + "'.\n");
        return 0;
    }

    // For whatever reason, Firefox uses the first frame to determine the outgoingReferrer. We replicate that behavior here.
    String referrer = SecurityPolicy::generateReferrerHeader(firstFrame->document()->referrerPolicy(), completedURL, firstFrame->loader()->outgoingReferrer());

    ResourceRequest request(completedURL, referrer);
    FrameLoader::addHTTPOriginIfNeeded(request, firstFrame->loader()->outgoingOrigin());
    FrameLoadRequest frameRequest(activeWindow->document()->securityOrigin(), request, frameName);

    // We pass the opener frame for the lookupFrame in case the active frame is different from
    // the opener frame, and the name references a frame relative to the opener frame.
    bool created;
    Frame* newFrame = createWindow(activeFrame, openerFrame, frameRequest, windowFeatures, created);
    if (!newFrame)
        return 0;

    newFrame->loader()->setOpener(openerFrame);
    newFrame->page()->setOpenedByDOM();

    if (newFrame->domWindow()->isInsecureScriptAccess(activeWindow, completedURL))
        return newFrame;

    if (function)
        function(newFrame->domWindow(), functionContext);

    if (created) {
        FrameLoadRequest request(activeWindow->document()->securityOrigin(), ResourceRequest(completedURL, referrer));
        newFrame->loader()->load(request);
    } else if (!urlString.isEmpty()) {
        newFrame->navigationScheduler()->scheduleLocationChange(activeWindow->document()->securityOrigin(), completedURL.string(), referrer, false);
    }
    return newFrame;
}

} // namespace WebCore
