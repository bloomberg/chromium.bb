/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008, 2009 Google, Inc.
 * All rights reserved.
 * Copyright (C) 2009 Kenneth Rohde Christiansen
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

#ifndef LayoutThemeChromiumDefault_h
#define LayoutThemeChromiumDefault_h

#include "core/layout/LayoutThemeChromiumSkia.h"

namespace blink {

class LayoutThemeChromiumDefault : public LayoutThemeChromiumSkia {
public:
    virtual String extraDefaultStyleSheet() override;

    virtual Color systemColor(CSSValueID) const override;

    virtual bool supportsFocusRing(const RenderStyle&) const override;

    // List Box selection color
    virtual Color activeListBoxSelectionBackgroundColor() const;
    virtual Color activeListBoxSelectionForegroundColor() const;
    virtual Color inactiveListBoxSelectionBackgroundColor() const;
    virtual Color inactiveListBoxSelectionForegroundColor() const;

    virtual Color platformActiveSelectionBackgroundColor() const override;
    virtual Color platformInactiveSelectionBackgroundColor() const override;
    virtual Color platformActiveSelectionForegroundColor() const override;
    virtual Color platformInactiveSelectionForegroundColor() const override;

    virtual IntSize sliderTickSize() const override;
    virtual int sliderTickOffsetFromTrackCenter() const override;
    virtual void adjustSliderThumbSize(RenderStyle&, Element*) const override;

    static void setCaretBlinkInterval(double);
    virtual double caretBlinkIntervalInternal() const override;

    virtual bool paintCheckbox(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual void setCheckboxSize(RenderStyle&) const override;

    virtual bool paintRadio(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual void setRadioSize(RenderStyle&) const override;

    virtual bool paintButton(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintTextField(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMenuList(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMenuListButton(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintSliderTrack(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintSliderThumb(LayoutObject*, const PaintInfo&, const IntRect&) override;

    virtual void adjustInnerSpinButtonStyle(RenderStyle&, Element*) const override;
    virtual bool paintInnerSpinButton(LayoutObject*, const PaintInfo&, const IntRect&) override;

    virtual bool popsMenuBySpaceKey() const override final { return true; }
    virtual bool popsMenuByReturnKey() const override final { return true; }
    virtual bool popsMenuByAltDownUpOrF4Key() const override { return true; }

    virtual bool paintProgressBar(LayoutObject*, const PaintInfo&, const IntRect&) override;

    virtual bool shouldOpenPickerWithF4Key() const override;

    static void setSelectionColors(unsigned activeBackgroundColor, unsigned activeForegroundColor, unsigned inactiveBackgroundColor, unsigned inactiveForegroundColor);

protected:
    LayoutThemeChromiumDefault();
    virtual ~LayoutThemeChromiumDefault();
    virtual bool shouldUseFallbackTheme(const RenderStyle&) const override;

private:
    static double m_caretBlinkInterval;

    static unsigned m_activeSelectionBackgroundColor;
    static unsigned m_activeSelectionForegroundColor;
    static unsigned m_inactiveSelectionBackgroundColor;
    static unsigned m_inactiveSelectionForegroundColor;
};

} // namespace blink

#endif // LayoutThemeChromiumDefault_h
