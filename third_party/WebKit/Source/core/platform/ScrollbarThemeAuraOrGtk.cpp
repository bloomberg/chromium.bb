/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/platform/ScrollbarThemeAuraOrGtk.h"

#include "RuntimeEnabledFeatures.h"
#include "core/platform/PlatformMouseEvent.h"
#include "core/platform/Scrollbar.h"
#include "core/platform/ScrollbarThemeOverlay.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "public/platform/Platform.h"
#include "public/platform/WebRect.h"
#include "public/platform/default/WebThemeEngine.h"

namespace WebCore {

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    if (RuntimeEnabledFeatures::overlayScrollbarsEnabled()) {
        DEFINE_STATIC_LOCAL(ScrollbarThemeOverlay, theme, (10, 0, ScrollbarThemeOverlay::AllowHitTest, Color(128, 128, 128, 192)));
        return &theme;
    }

    DEFINE_STATIC_LOCAL(ScrollbarThemeAuraOrGtk, theme, ());
    return &theme;
}

int ScrollbarThemeAuraOrGtk::scrollbarThickness(ScrollbarControlSize controlSize)
{
    // Horiz and Vert scrollbars are the same thickness.
    IntSize scrollbarSize = WebKit::Platform::current()->themeEngine()->getSize(WebKit::WebThemeEngine::PartScrollbarVerticalTrack);
    return scrollbarSize.width();
}

void ScrollbarThemeAuraOrGtk::paintTrackPiece(GraphicsContext* gc, ScrollbarThemeClient* scrollbar, const IntRect& rect, ScrollbarPart partType)
{
    WebKit::WebThemeEngine::State state = scrollbar->hoveredPart() == partType ? WebKit::WebThemeEngine::StateHover : WebKit::WebThemeEngine::StateNormal;
    IntRect alignRect = trackRect(scrollbar, false);
    WebKit::WebThemeEngine::ExtraParams extraParams;
    WebKit::WebCanvas* canvas = gc->canvas();
    extraParams.scrollbarTrack.trackX = alignRect.x();
    extraParams.scrollbarTrack.trackY = alignRect.y();
    extraParams.scrollbarTrack.trackWidth = alignRect.width();
    extraParams.scrollbarTrack.trackHeight = alignRect.height();
    WebKit::Platform::current()->themeEngine()->paint(canvas, scrollbar->orientation() == HorizontalScrollbar ? WebKit::WebThemeEngine::PartScrollbarHorizontalTrack : WebKit::WebThemeEngine::PartScrollbarVerticalTrack, state, WebKit::WebRect(rect), &extraParams);
}

void ScrollbarThemeAuraOrGtk::paintButton(GraphicsContext* gc, ScrollbarThemeClient* scrollbar, const IntRect& rect, ScrollbarPart part)
{
    WebKit::WebThemeEngine::Part paintPart;
    WebKit::WebThemeEngine::State state = WebKit::WebThemeEngine::StateNormal;
    WebKit::WebCanvas* canvas = gc->canvas();
    bool checkMin = false;
    bool checkMax = false;
    if (scrollbar->orientation() == HorizontalScrollbar) {
        if (part == BackButtonStartPart) {
            paintPart = WebKit::WebThemeEngine::PartScrollbarLeftArrow;
            checkMin = true;
        } else {
            paintPart = WebKit::WebThemeEngine::PartScrollbarRightArrow;
            checkMax = true;
        }
    } else {
        if (part == BackButtonStartPart) {
            paintPart = WebKit::WebThemeEngine::PartScrollbarUpArrow;
            checkMin = true;
        } else {
            paintPart = WebKit::WebThemeEngine::PartScrollbarDownArrow;
            checkMax = true;
        }
    }
    if ((checkMin && (scrollbar->currentPos() <= 0))
        || (checkMax && scrollbar->currentPos() == scrollbar->maximum())) {
        state = WebKit::WebThemeEngine::StateDisabled;
    } else {
        if (part == scrollbar->pressedPart())
            state = WebKit::WebThemeEngine::StatePressed;
        else if (part == scrollbar->hoveredPart())
            state = WebKit::WebThemeEngine::StateHover;
    }
    WebKit::Platform::current()->themeEngine()->paint(canvas, paintPart, state, WebKit::WebRect(rect), 0);
}

void ScrollbarThemeAuraOrGtk::paintThumb(GraphicsContext* gc, ScrollbarThemeClient* scrollbar, const IntRect& rect)
{
    WebKit::WebThemeEngine::State state;
    WebKit::WebCanvas* canvas = gc->canvas();
    if (scrollbar->pressedPart() == ThumbPart)
        state = WebKit::WebThemeEngine::StatePressed;
    else if (scrollbar->hoveredPart() == ThumbPart)
        state = WebKit::WebThemeEngine::StateHover;
    else
        state = WebKit::WebThemeEngine::StateNormal;
    WebKit::Platform::current()->themeEngine()->paint(canvas, scrollbar->orientation() == HorizontalScrollbar ? WebKit::WebThemeEngine::PartScrollbarHorizontalThumb : WebKit::WebThemeEngine::PartScrollbarVerticalThumb, state, WebKit::WebRect(rect), 0);
}

bool ScrollbarThemeAuraOrGtk::shouldCenterOnThumb(ScrollbarThemeClient*, const PlatformMouseEvent& evt)
{
    return (evt.shiftKey() && evt.button() == LeftButton) || (evt.button() == MiddleButton);
}

IntSize ScrollbarThemeAuraOrGtk::buttonSize(ScrollbarThemeClient* scrollbar)
{
    if (scrollbar->orientation() == VerticalScrollbar) {
        IntSize size = WebKit::Platform::current()->themeEngine()->getSize(WebKit::WebThemeEngine::PartScrollbarUpArrow);
        return IntSize(size.width(), scrollbar->height() < 2 * size.height() ? scrollbar->height() / 2 : size.height());
    }

    // HorizontalScrollbar
    IntSize size = WebKit::Platform::current()->themeEngine()->getSize(WebKit::WebThemeEngine::PartScrollbarLeftArrow);
    return IntSize(scrollbar->width() < 2 * size.width() ? scrollbar->width() / 2 : size.width(), size.height());
}

int ScrollbarThemeAuraOrGtk::minimumThumbLength(ScrollbarThemeClient* scrollbar)
{
    if (scrollbar->orientation() == VerticalScrollbar) {
        IntSize size = WebKit::Platform::current()->themeEngine()->getSize(WebKit::WebThemeEngine::PartScrollbarVerticalThumb);
        return size.height();
    }

    IntSize size = WebKit::Platform::current()->themeEngine()->getSize(WebKit::WebThemeEngine::PartScrollbarHorizontalThumb);
    return size.width();
}

} // namespace WebCore
