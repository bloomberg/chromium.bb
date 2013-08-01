/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollbarThemeMac_h
#define ScrollbarThemeMac_h

#include "core/platform/ScrollbarThemeComposite.h"

typedef id ScrollbarPainter;

namespace WebCore {

class ScrollbarThemeMacCommon : public ScrollbarThemeComposite {
public:
    ScrollbarThemeMacCommon();
    virtual ~ScrollbarThemeMacCommon();

    virtual void registerScrollbar(ScrollbarThemeClient*);
    virtual void unregisterScrollbar(ScrollbarThemeClient*);
    void preferencesChanged();

    virtual bool supportsControlTints() const { return true; }

    virtual double initialAutoscrollTimerDelay();
    virtual double autoscrollTimerDelay();

    virtual void paintOverhangAreas(ScrollView*, GraphicsContext*, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect);
    virtual void paintTickmarks(GraphicsContext*, ScrollbarThemeClient*, const IntRect&) OVERRIDE;

protected:
    virtual int maxOverlapBetweenPages() { return 40; }

    virtual bool shouldCenterOnThumb(ScrollbarThemeClient*, const PlatformMouseEvent&);
    virtual bool shouldDragDocumentInsteadOfThumb(ScrollbarThemeClient*, const PlatformMouseEvent&);
    int scrollbarPartToHIPressedState(ScrollbarPart);

    virtual void updateButtonPlacement() { }

    void paintGivenTickmarks(GraphicsContext*, ScrollbarThemeClient*, const IntRect&, const Vector<IntRect>&);

    RefPtr<Pattern> m_overhangPattern;
};

class ScrollbarThemeMacOverlayAPI : public ScrollbarThemeMacCommon {
public:
    virtual void updateEnabledState(ScrollbarThemeClient*);
    virtual int scrollbarThickness(ScrollbarControlSize = RegularScrollbar);
    virtual bool usesOverlayScrollbars() const;
    virtual void updateScrollbarOverlayStyle(ScrollbarThemeClient*);
    virtual ScrollbarButtonsPlacement buttonsPlacement() const;

    virtual void registerScrollbar(ScrollbarThemeClient*);
    virtual void unregisterScrollbar(ScrollbarThemeClient*);

    void setNewPainterForScrollbar(ScrollbarThemeClient*, ScrollbarPainter);
    ScrollbarPainter painterForScrollbar(ScrollbarThemeClient*);

    virtual void paintTrackBackground(GraphicsContext*, ScrollbarThemeClient*, const IntRect&);
    virtual void paintThumb(GraphicsContext*, ScrollbarThemeClient*, const IntRect&);

protected:
    virtual IntRect trackRect(ScrollbarThemeClient*, bool painting = false);
    virtual IntRect backButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool painting = false);
    virtual IntRect forwardButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool painting = false);

    virtual bool hasButtons(ScrollbarThemeClient*) { return false; }
    virtual bool hasThumb(ScrollbarThemeClient*);

    virtual int minimumThumbLength(ScrollbarThemeClient*);
};

class ScrollbarThemeMacNonOverlayAPI : public ScrollbarThemeMacCommon {
public:
    virtual int scrollbarThickness(ScrollbarControlSize = RegularScrollbar);
    virtual bool usesOverlayScrollbars() const { return false; }
    virtual ScrollbarButtonsPlacement buttonsPlacement() const;

    virtual bool paint(ScrollbarThemeClient*, GraphicsContext*, const IntRect& damageRect);

protected:
    virtual IntRect trackRect(ScrollbarThemeClient*, bool painting = false);
    virtual IntRect backButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool painting = false);
    virtual IntRect forwardButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool painting = false);

    virtual void updateButtonPlacement();

    virtual bool hasButtons(ScrollbarThemeClient*);
    virtual bool hasThumb(ScrollbarThemeClient*);

    virtual int minimumThumbLength(ScrollbarThemeClient*);
};

}

#endif
