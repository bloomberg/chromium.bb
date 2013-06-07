/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2008, 2009 Google, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RenderThemeChromiumWin_h
#define RenderThemeChromiumWin_h

#include "core/rendering/RenderThemeChromiumSkia.h"

#if WIN32
typedef void* HANDLE;
typedef struct HINSTANCE__* HINSTANCE;
typedef HINSTANCE HMODULE;
#endif

namespace WebCore {

struct ThemeData {
    ThemeData() : m_part(0), m_state(0), m_classicState(0) { }

    unsigned m_part;
    unsigned m_state;
    unsigned m_classicState;
};

class RenderThemeChromiumWin : public RenderThemeChromiumSkia {
public:
    static PassRefPtr<RenderTheme> create();

    // A method asking if the theme is able to draw the focus ring.
    virtual bool supportsFocusRing(const RenderStyle*) const OVERRIDE;

    // The platform selection color.
    virtual Color platformActiveSelectionBackgroundColor() const OVERRIDE;
    virtual Color platformInactiveSelectionBackgroundColor() const OVERRIDE;
    virtual Color platformActiveSelectionForegroundColor() const OVERRIDE;
    virtual Color platformInactiveSelectionForegroundColor() const OVERRIDE;
    virtual Color platformActiveTextSearchHighlightColor() const OVERRIDE;
    virtual Color platformInactiveTextSearchHighlightColor() const OVERRIDE;

    virtual Color systemColor(CSSValueID) const OVERRIDE;

    virtual IntSize sliderTickSize() const OVERRIDE;
    virtual int sliderTickOffsetFromTrackCenter() const OVERRIDE;
    virtual void adjustSliderThumbSize(RenderStyle*, Element*) const OVERRIDE;

    // Various paint functions.
    virtual bool paintCheckbox(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintRadio(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintButton(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintTextField(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintSliderTrack(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintSliderThumb(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    // MenuList refers to an unstyled menulist (meaning a menulist without
    // background-color or border set) and MenuListButton refers to a styled
    // menulist (a menulist with background-color or border set).
    virtual bool paintMenuList(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintMenuListButton(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    virtual void adjustInnerSpinButtonStyle(RenderStyle*, Element*) const OVERRIDE;
    virtual bool paintInnerSpinButton(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    virtual double animationRepeatIntervalForProgressBar(RenderProgress*) const OVERRIDE;
    virtual double animationDurationForProgressBar(RenderProgress*) const OVERRIDE;
    virtual void adjustProgressBarStyle(RenderStyle*, Element*) const OVERRIDE;
    virtual bool paintProgressBar(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    virtual bool shouldOpenPickerWithF4Key() const OVERRIDE;

protected:
    virtual double caretBlinkIntervalInternal() const OVERRIDE;
    virtual bool shouldUseFallbackTheme(RenderStyle*) const OVERRIDE;

private:
    enum ControlSubPart {
        None,
        SpinButtonDown,
        SpinButtonUp,
    };

    RenderThemeChromiumWin() { }
    virtual ~RenderThemeChromiumWin() { }

    unsigned determineState(RenderObject*, ControlSubPart = None);
    unsigned determineSliderThumbState(RenderObject*);
    unsigned determineClassicState(RenderObject*, ControlSubPart = None);

    ThemeData getThemeData(RenderObject*, ControlSubPart = None);

    bool paintTextFieldInternal(RenderObject*, const PaintInfo&, const IntRect&, bool);
};

} // namespace WebCore

#endif
