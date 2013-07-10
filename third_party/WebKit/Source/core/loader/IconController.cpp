/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
 * Copyright (C) 2011 Kris Jordan <krisjordan@gmail.com>
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "core/loader/IconController.h"

#include "core/dom/Document.h"
#include "core/dom/IconURL.h"
#include "core/page/Frame.h"

namespace WebCore {

IconController::IconController(Frame* frame)
    : m_frame(frame)
{
}

IconController::~IconController()
{
}

KURL IconController::url()
{
    IconURLs iconURLs = urlsForTypes(Favicon);
    return iconURLs.isEmpty() ? KURL() : iconURLs[0].m_iconURL;
}

IconURL IconController::iconURL(IconType iconType) const
{
    IconURL result;
    const Vector<IconURL>& iconURLs = m_frame->document()->iconURLs(iconType);
    Vector<IconURL>::const_iterator iter(iconURLs.begin());
    for (; iter != iconURLs.end(); ++iter) {
        if (result.m_iconURL.isEmpty() || !iter->m_mimeType.isEmpty())
            result = *iter;
    }

    return result;
}

IconURLs IconController::urlsForTypes(int iconTypesMask)
{
    IconURLs iconURLs;
    if (m_frame->tree() && m_frame->tree()->parent())
        return iconURLs;

    if (iconTypesMask & Favicon && !appendToIconURLs(Favicon, &iconURLs))
        iconURLs.append(defaultURL(Favicon));

#if ENABLE(TOUCH_ICON_LOADING)
    int missedIcons = 0;
    if (iconTypesMask & TouchPrecomposedIcon)
        missedIcons += appendToIconURLs(TouchPrecomposedIcon, &iconURLs) ? 0:1;

    if (iconTypesMask & TouchIcon)
      missedIcons += appendToIconURLs(TouchIcon, &iconURLs) ? 0:1;

    // Only return the default touch icons when the both were required and neither was gotten.
    if (missedIcons == 2) {
        iconURLs.append(defaultURL(TouchPrecomposedIcon));
        iconURLs.append(defaultURL(TouchIcon));
    }
#endif

    // Finally, append all remaining icons of this type.
    const Vector<IconURL>& allIconURLs = m_frame->document()->iconURLs(iconTypesMask);
    for (Vector<IconURL>::const_iterator iter = allIconURLs.begin(); iter != allIconURLs.end(); ++iter) {
        int i;
        int iconCount = iconURLs.size();
        for (i = 0; i < iconCount; ++i) {
            if (*iter == iconURLs.at(i))
                break;
        }
        if (i == iconCount)
            iconURLs.append(*iter);
    }

    return iconURLs;
}

bool IconController::appendToIconURLs(IconType iconType, IconURLs* iconURLs)
{
    IconURL faviconURL = iconURL(iconType);
    if (faviconURL.m_iconURL.isEmpty())
        return false;

    iconURLs->append(faviconURL);
    return true;
}

IconURL IconController::defaultURL(IconType iconType)
{
    // Don't return a favicon iconURL unless we're http or https
    KURL documentURL = m_frame->document()->url();
    if (!documentURL.protocolIsInHTTPFamily())
        return IconURL();

    KURL url;
    bool couldSetProtocol = url.setProtocol(documentURL.protocol());
    ASSERT_UNUSED(couldSetProtocol, couldSetProtocol);
    url.setHost(documentURL.host());
    if (documentURL.hasPort())
        url.setPort(documentURL.port());

    if (iconType == Favicon) {
        url.setPath("/favicon.ico");
        return IconURL::defaultIconURL(url, Favicon);
    }
#if ENABLE(TOUCH_ICON_LOADING)
    if (iconType == TouchPrecomposedIcon) {
        url.setPath("/apple-touch-icon-precomposed.png");
        return IconURL::defaultIconURL(url, TouchPrecomposedIcon);
    }
    if (iconType == TouchIcon) {
        url.setPath("/apple-touch-icon.png");
        return IconURL::defaultIconURL(url, TouchIcon);
    }
#endif
    return IconURL();
}

}
