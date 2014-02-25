/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "core/svg/graphics/SVGImageChromeClient.h"

#include "core/frame/FrameView.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/graphics/ImageObserver.h"

namespace WebCore {

SVGImageChromeClient::SVGImageChromeClient(SVGImage* image)
    : m_image(image)
    , m_animationTimer(this, &SVGImageChromeClient::animationTimerFired)
{
}

bool SVGImageChromeClient::isSVGImageChromeClient() const
{
    return true;
}

void SVGImageChromeClient::chromeDestroyed()
{
    m_image = 0;
}

void SVGImageChromeClient::invalidateContentsAndRootView(const IntRect& r)
{
    // If m_image->m_page is null, we're being destructed, don't fire changedInRect() in that case.
    if (m_image && m_image->imageObserver() && m_image->m_page)
        m_image->imageObserver()->changedInRect(m_image, r);
}

void SVGImageChromeClient::scheduleAnimation()
{
    // Because a single SVGImage can be shared by multiple pages, we can't key
    // our svg image layout on the page's real animation frame. Therefore, we
    // run this fake animation timer to trigger layout in SVGImages. The name,
    // "animationTimer", is to match the new requestAnimationFrame-based layout
    // approach.
    if (m_animationTimer.isActive())
        return;
    m_animationTimer.startOneShot(0);
}

void SVGImageChromeClient::animationTimerFired(Timer<SVGImageChromeClient>*)
{
    // In principle, we should call requestAnimationFrame callbacks here, but
    // we know there aren't any because script is forbidden inside SVGImages.
    if (m_image) {
        m_image->frameView()->page()->animator().serviceScriptedAnimations(0);
        m_image->frameView()->updateLayoutAndStyleIfNeededRecursive();
    }
}

}
