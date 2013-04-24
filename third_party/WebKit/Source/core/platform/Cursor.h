/*
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef Cursor_h
#define Cursor_h

#include "core/platform/graphics/Image.h"
#include "core/platform/graphics/IntPoint.h"
#include <wtf/Assertions.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class Image;

    class Cursor {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        enum Type {
            Pointer = 0,
            Cross,
            Hand,
            IBeam,
            Wait,
            Help,
            EastResize,
            NorthResize,
            NorthEastResize,
            NorthWestResize,
            SouthResize,
            SouthEastResize,
            SouthWestResize,
            WestResize,
            NorthSouthResize,
            EastWestResize,
            NorthEastSouthWestResize,
            NorthWestSouthEastResize,
            ColumnResize,
            RowResize,
            MiddlePanning,
            EastPanning,
            NorthPanning,
            NorthEastPanning,
            NorthWestPanning,
            SouthPanning,
            SouthEastPanning,
            SouthWestPanning,
            WestPanning,
            Move,
            VerticalText,
            Cell,
            ContextMenu,
            Alias,
            Progress,
            NoDrop,
            Copy,
            None,
            NotAllowed,
            ZoomIn,
            ZoomOut,
            Grab,
            Grabbing,
            Custom
        };

        static const Cursor& fromType(Cursor::Type);

        Cursor()
            // This is an invalid Cursor and should never actually get used.
            : m_type(static_cast<Type>(-1))
        {
        }

        Cursor(Image*, const IntPoint& hotSpot);
        Cursor(const Cursor&);

        // Hot spot is in image pixels.
        Cursor(Image*, const IntPoint& hotSpot, float imageScaleFactor);

        ~Cursor();
        Cursor& operator=(const Cursor&);

        explicit Cursor(Type);
        Type type() const
        {
            ASSERT(m_type >= 0 && m_type <= Custom);
            return m_type;
        }
        Image* image() const { return m_image.get(); }
        const IntPoint& hotSpot() const { return m_hotSpot; }
        // Image scale in image pixels per logical (UI) pixel.
        float imageScaleFactor() const { return m_imageScaleFactor; }

     private:
        Type m_type;
        RefPtr<Image> m_image;
        IntPoint m_hotSpot;
        float m_imageScaleFactor;
    };

    IntPoint determineHotSpot(Image*, const IntPoint& specifiedHotSpot);

    const Cursor& pointerCursor();
    const Cursor& crossCursor();
    const Cursor& handCursor();
    const Cursor& moveCursor();
    const Cursor& iBeamCursor();
    const Cursor& waitCursor();
    const Cursor& helpCursor();
    const Cursor& eastResizeCursor();
    const Cursor& northResizeCursor();
    const Cursor& northEastResizeCursor();
    const Cursor& northWestResizeCursor();
    const Cursor& southResizeCursor();
    const Cursor& southEastResizeCursor();
    const Cursor& southWestResizeCursor();
    const Cursor& westResizeCursor();
    const Cursor& northSouthResizeCursor();
    const Cursor& eastWestResizeCursor();
    const Cursor& northEastSouthWestResizeCursor();
    const Cursor& northWestSouthEastResizeCursor();
    const Cursor& columnResizeCursor();
    const Cursor& rowResizeCursor();
    const Cursor& middlePanningCursor();
    const Cursor& eastPanningCursor();
    const Cursor& northPanningCursor();
    const Cursor& northEastPanningCursor();
    const Cursor& northWestPanningCursor();
    const Cursor& southPanningCursor();
    const Cursor& southEastPanningCursor();
    const Cursor& southWestPanningCursor();
    const Cursor& westPanningCursor();
    const Cursor& verticalTextCursor();
    const Cursor& cellCursor();
    const Cursor& contextMenuCursor();
    const Cursor& noDropCursor();
    const Cursor& notAllowedCursor();
    const Cursor& progressCursor();
    const Cursor& aliasCursor();
    const Cursor& zoomInCursor();
    const Cursor& zoomOutCursor();
    const Cursor& copyCursor();
    const Cursor& noneCursor();
    const Cursor& grabCursor();
    const Cursor& grabbingCursor();

} // namespace WebCore

#endif // Cursor_h
