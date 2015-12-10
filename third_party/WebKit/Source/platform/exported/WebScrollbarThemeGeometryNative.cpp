/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "platform/exported/WebScrollbarThemeGeometryNative.h"

#include "platform/exported/WebScrollbarThemeClientImpl.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/WebScrollbar.h"

namespace blink {

PassOwnPtr<WebScrollbarThemeGeometryNative> WebScrollbarThemeGeometryNative::create(ScrollbarTheme& theme)
{
    return adoptPtr(new WebScrollbarThemeGeometryNative(theme));
}

WebScrollbarThemeGeometryNative::WebScrollbarThemeGeometryNative(ScrollbarTheme& theme)
    : m_theme(theme)
{
}

WebScrollbarThemeGeometryNative* WebScrollbarThemeGeometryNative::clone() const
{
    return new WebScrollbarThemeGeometryNative(m_theme);
}

int WebScrollbarThemeGeometryNative::thumbPosition(WebScrollbar* scrollbar)
{
    return m_theme.thumbPosition(WebScrollbarThemeClientImpl(*scrollbar));
}

int WebScrollbarThemeGeometryNative::thumbLength(WebScrollbar* scrollbar)
{
    return m_theme.thumbLength(WebScrollbarThemeClientImpl(*scrollbar));
}

int WebScrollbarThemeGeometryNative::trackPosition(WebScrollbar* scrollbar)
{
    return m_theme.trackPosition(WebScrollbarThemeClientImpl(*scrollbar));
}

int WebScrollbarThemeGeometryNative::trackLength(WebScrollbar* scrollbar)
{
    return m_theme.trackLength(WebScrollbarThemeClientImpl(*scrollbar));
}

bool WebScrollbarThemeGeometryNative::hasButtons(WebScrollbar* scrollbar)
{
    return m_theme.hasButtons(WebScrollbarThemeClientImpl(*scrollbar));
}

bool WebScrollbarThemeGeometryNative::hasThumb(WebScrollbar* scrollbar)
{
    return m_theme.hasThumb(WebScrollbarThemeClientImpl(*scrollbar));
}

WebRect WebScrollbarThemeGeometryNative::trackRect(WebScrollbar* scrollbar)
{
    return m_theme.trackRect(WebScrollbarThemeClientImpl(*scrollbar));
}

WebRect WebScrollbarThemeGeometryNative::thumbRect(WebScrollbar* scrollbar)
{
    return m_theme.thumbRect(WebScrollbarThemeClientImpl(*scrollbar));
}

int WebScrollbarThemeGeometryNative::minimumThumbLength(WebScrollbar* scrollbar)
{
    return m_theme.minimumThumbLength(WebScrollbarThemeClientImpl(*scrollbar));
}

int WebScrollbarThemeGeometryNative::scrollbarThickness(WebScrollbar* scrollbar)
{
    return m_theme.scrollbarThickness(WebScrollbarThemeClientImpl(*scrollbar).controlSize());
}

WebRect WebScrollbarThemeGeometryNative::backButtonStartRect(WebScrollbar* scrollbar)
{
    return m_theme.backButtonRect(WebScrollbarThemeClientImpl(*scrollbar), BackButtonStartPart, false);
}

WebRect WebScrollbarThemeGeometryNative::backButtonEndRect(WebScrollbar* scrollbar)
{
    return m_theme.backButtonRect(WebScrollbarThemeClientImpl(*scrollbar), BackButtonEndPart, false);
}

WebRect WebScrollbarThemeGeometryNative::forwardButtonStartRect(WebScrollbar* scrollbar)
{
    return m_theme.forwardButtonRect(WebScrollbarThemeClientImpl(*scrollbar), ForwardButtonStartPart, false);
}

WebRect WebScrollbarThemeGeometryNative::forwardButtonEndRect(WebScrollbar* scrollbar)
{
    return m_theme.forwardButtonRect(WebScrollbarThemeClientImpl(*scrollbar), ForwardButtonEndPart, false);
}

WebRect WebScrollbarThemeGeometryNative::constrainTrackRectToTrackPieces(WebScrollbar* scrollbar, const WebRect& rect)
{
    return m_theme.constrainTrackRectToTrackPieces(WebScrollbarThemeClientImpl(*scrollbar), IntRect(rect));
}

void WebScrollbarThemeGeometryNative::splitTrack(WebScrollbar* scrollbar, const WebRect& webTrack, WebRect& webStartTrack, WebRect& webThumb, WebRect& webEndTrack)
{
    IntRect track(webTrack);
    IntRect startTrack;
    IntRect thumb;
    IntRect endTrack;
    m_theme.splitTrack(WebScrollbarThemeClientImpl(*scrollbar), track, startTrack, thumb, endTrack);

    webStartTrack = startTrack;
    webThumb = thumb;
    webEndTrack = endTrack;
}

} // namespace blink
