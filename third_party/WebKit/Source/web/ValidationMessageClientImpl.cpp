/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "ValidationMessageClientImpl.h"

#include "WebTextDirection.h"
#include "WebValidationMessageClient.h"
#include "WebViewImpl.h"
#include "core/dom/Element.h"
#include "core/page/FrameView.h"
#include "core/platform/HostWindow.h"
#include "core/rendering/RenderObject.h"
#include "wtf/CurrentTime.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebString.h"
#include "public/web/WebViewClient.h"

using namespace WebCore;

namespace WebKit {

ValidationMessageClientImpl::ValidationMessageClientImpl(WebViewImpl& webView, WebValidationMessageClient& client)
    : m_webView(webView)
    , m_client(client)
    , m_currentAnchor(0)
    , m_lastPageScaleFactor(1)
    , m_finishTime(0)
    , m_timer(this, &ValidationMessageClientImpl::checkAnchorStatus)
{
}

PassOwnPtr<ValidationMessageClientImpl> ValidationMessageClientImpl::create(WebViewImpl& webView, WebValidationMessageClient& client)
{
    return adoptPtr(new ValidationMessageClientImpl(webView, client));
}

ValidationMessageClientImpl::~ValidationMessageClientImpl()
{
    if (m_currentAnchor)
        hideValidationMessage(*m_currentAnchor);
}

FrameView* ValidationMessageClientImpl::currentView()
{
    return m_currentAnchor->document().view();
}

void ValidationMessageClientImpl::showValidationMessage(const Element& anchor, const String& message)
{
    if (message.isEmpty()) {
        hideValidationMessage(anchor);
        return;
    }
    if (!anchor.renderBox())
        return;
    if (m_currentAnchor)
        hideValidationMessage(*m_currentAnchor);
    m_currentAnchor = &anchor;
    IntRect anchorInRootView = currentView()->contentsToRootView(anchor.pixelSnappedBoundingBox());
    m_lastAnchorRectInScreen = currentView()->hostWindow()->rootViewToScreen(anchorInRootView);
    m_lastPageScaleFactor = m_webView.pageScaleFactor();
    m_message = message;

    WebTextDirection dir = m_currentAnchor->renderer()->style()->direction() == RTL ? WebTextDirectionRightToLeft : WebTextDirectionLeftToRight;
    AtomicString title = m_currentAnchor->fastGetAttribute(HTMLNames::titleAttr);
    m_client.showValidationMessage(anchorInRootView, m_message, title, dir);
    m_webView.client()->showValidationMessage(anchorInRootView, m_message, title, dir);

    const double minimumSecondToShowValidationMessage = 5.0;
    const double secondPerCharacter = 0.05;
    const double statusCheckInterval = 0.1;
    m_finishTime = monotonicallyIncreasingTime() + std::max(minimumSecondToShowValidationMessage, (message.length() + title.length()) * secondPerCharacter);
    // FIXME: We should invoke checkAnchorStatus actively when layout, scroll,
    // or page scale change happen.
    m_timer.startRepeating(statusCheckInterval);
}

void ValidationMessageClientImpl::hideValidationMessage(const Element& anchor)
{
    if (!m_currentAnchor || !isValidationMessageVisible(anchor))
        return;
    m_timer.stop();
    m_currentAnchor = 0;
    m_message = String();
    m_finishTime = 0;
    m_client.hideValidationMessage();
    m_webView.client()->hideValidationMessage();
}

bool ValidationMessageClientImpl::isValidationMessageVisible(const Element& anchor)
{
    return m_currentAnchor == &anchor;
}

void ValidationMessageClientImpl::documentDetached(const Document& document)
{
    if (m_currentAnchor && &m_currentAnchor->document() == &document)
        hideValidationMessage(*m_currentAnchor);
}

void ValidationMessageClientImpl::checkAnchorStatus(Timer<ValidationMessageClientImpl>*)
{
    ASSERT(m_currentAnchor);
    if (monotonicallyIncreasingTime() >= m_finishTime || !currentView()) {
        hideValidationMessage(*m_currentAnchor);
        return;
    }

    // Check the visibility of the element.
    // FIXME: Can we check invisibility by scrollable non-frame elements?
    IntRect newAnchorRect = currentView()->contentsToRootView(m_currentAnchor->pixelSnappedBoundingBox());
    newAnchorRect = intersection(currentView()->convertToRootView(currentView()->boundsRect()), newAnchorRect);
    if (newAnchorRect.isEmpty()) {
        hideValidationMessage(*m_currentAnchor);
        return;
    }

    IntRect newAnchorRectInScreen = currentView()->hostWindow()->rootViewToScreen(newAnchorRect);
    if (newAnchorRectInScreen == m_lastAnchorRectInScreen && m_webView.pageScaleFactor() == m_lastPageScaleFactor)
        return;
    m_lastAnchorRectInScreen = newAnchorRectInScreen;
    m_lastPageScaleFactor = m_webView.pageScaleFactor();
    m_client.moveValidationMessage(newAnchorRect);
    m_webView.client()->moveValidationMessage(newAnchorRect);
}

}
