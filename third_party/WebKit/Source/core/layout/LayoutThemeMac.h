/*
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
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

#ifndef LayoutThemeMac_h
#define LayoutThemeMac_h

#import "core/layout/LayoutTheme.h"
#import "wtf/HashMap.h"
#import "wtf/RetainPtr.h"

OBJC_CLASS LayoutThemeNotificationObserver;

namespace blink {

class LayoutThemeMac final : public LayoutTheme {
public:
    static PassRefPtr<LayoutTheme> create();

    virtual void adjustPaintInvalidationRect(const LayoutObject*, IntRect&) override;

    virtual bool isControlStyled(const LayoutStyle&, const AuthorStyleInfo&) const override;

    virtual Color platformActiveSelectionBackgroundColor() const override;
    virtual Color platformInactiveSelectionBackgroundColor() const override;
    virtual Color platformActiveSelectionForegroundColor() const override;
    virtual Color platformActiveListBoxSelectionBackgroundColor() const override;
    virtual Color platformActiveListBoxSelectionForegroundColor() const override;
    virtual Color platformInactiveListBoxSelectionBackgroundColor() const override;
    virtual Color platformInactiveListBoxSelectionForegroundColor() const override;
    virtual Color platformFocusRingColor() const override;

    virtual ScrollbarControlSize scrollbarControlSizeForPart(ControlPart part) override { return part == ListboxPart ? SmallScrollbar : RegularScrollbar; }

    virtual void platformColorsDidChange() override;

    // System fonts.
    virtual void systemFont(CSSValueID systemFontID, FontStyle&, FontWeight&, float& fontSize, AtomicString& fontFamily) const override;

    virtual int minimumMenuListSize(const LayoutStyle&) const override;

    virtual void adjustSliderThumbSize(LayoutStyle&, Element*) const override;

    virtual IntSize sliderTickSize() const override;
    virtual int sliderTickOffsetFromTrackCenter() const override;

    virtual int popupInternalPaddingLeft(const LayoutStyle&) const override;
    virtual int popupInternalPaddingRight(const LayoutStyle&) const override;
    virtual int popupInternalPaddingTop(const LayoutStyle&) const override;
    virtual int popupInternalPaddingBottom(const LayoutStyle&) const override;

    virtual bool paintCapsLockIndicator(LayoutObject*, const PaintInfo&, const IntRect&) override;

    virtual bool popsMenuByArrowKeys() const override { return true; }
    virtual bool popsMenuBySpaceKey() const override final { return true; }

    virtual IntSize meterSizeForBounds(const LayoutMeter*, const IntRect&) const override;
    virtual bool paintMeter(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual bool supportsMeter(ControlPart) const override;

    // Returns the repeat interval of the animation for the progress bar.
    virtual double animationRepeatIntervalForProgressBar() const override;
    // Returns the duration of the animation for the progress bar.
    virtual double animationDurationForProgressBar() const override;

    virtual Color systemColor(CSSValueID) const override;

    virtual bool supportsSelectionForegroundColors() const override { return false; }

    virtual bool isModalColorChooser() const { return false; }

protected:
    LayoutThemeMac();
    virtual ~LayoutThemeMac();

    virtual bool paintTextField(LayoutObject*, const PaintInfo&, const IntRect&) override;

    virtual bool paintTextArea(LayoutObject*, const PaintInfo&, const IntRect&) override;

    virtual bool paintMenuList(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual void adjustMenuListStyle(LayoutStyle&, Element*) const override;

    virtual bool paintMenuListButton(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual void adjustMenuListButtonStyle(LayoutStyle&, Element*) const override;

    virtual bool paintProgressBar(LayoutObject*, const PaintInfo&, const IntRect&) override;

    virtual bool paintSliderTrack(LayoutObject*, const PaintInfo&, const IntRect&) override;

    virtual bool paintSliderThumb(LayoutObject*, const PaintInfo&, const IntRect&) override;

    virtual bool paintSearchField(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual void adjustSearchFieldStyle(LayoutStyle&, Element*) const override;

    virtual void adjustSearchFieldCancelButtonStyle(LayoutStyle&, Element*) const override;
    virtual bool paintSearchFieldCancelButton(LayoutObject*, const PaintInfo&, const IntRect&) override;

    virtual void adjustSearchFieldDecorationStyle(LayoutStyle&, Element*) const override;
    virtual bool paintSearchFieldDecoration(LayoutObject*, const PaintInfo&, const IntRect&) override;

    virtual void adjustSearchFieldResultsDecorationStyle(LayoutStyle&, Element*) const override;
    virtual bool paintSearchFieldResultsDecoration(LayoutObject*, const PaintInfo&, const IntRect&) override;

private:
    virtual String fileListNameForWidth(Locale&, const FileList*, const Font&, int width) const override;

    FloatRect convertToPaintingRect(const LayoutObject* inputRenderer, const LayoutObject* partRenderer, const FloatRect& inputRect, const IntRect&) const;

    // Get the control size based off the font. Used by some of the controls (like buttons).
    NSControlSize controlSizeForFont(const LayoutStyle&) const;
    NSControlSize controlSizeForSystemFont(const LayoutStyle&) const;
    void setControlSize(NSCell*, const IntSize* sizes, const IntSize& minSize, float zoomLevel = 1.0f);
    void setSizeFromFont(LayoutStyle&, const IntSize* sizes) const;
    IntSize sizeForFont(const LayoutStyle&, const IntSize* sizes) const;
    IntSize sizeForSystemFont(const LayoutStyle&, const IntSize* sizes) const;
    void setFontFromControlSize(LayoutStyle&, NSControlSize) const;

    void updateCheckedState(NSCell*, const LayoutObject*);
    void updateEnabledState(NSCell*, const LayoutObject*);
    void updateFocusedState(NSCell*, const LayoutObject*);
    void updatePressedState(NSCell*, const LayoutObject*);

    // Helpers for adjusting appearance and for painting

    void setPopupButtonCellState(const LayoutObject*, const IntRect&);
    const IntSize* popupButtonSizes() const;
    const int* popupButtonMargins() const;
    const int* popupButtonPadding(NSControlSize) const;
    const IntSize* menuListSizes() const;

    const IntSize* searchFieldSizes() const;
    const IntSize* cancelButtonSizes() const;
    const IntSize* resultsButtonSizes() const;
    void setSearchCellState(LayoutObject*, const IntRect&);
    void setSearchFieldSize(LayoutStyle&) const;

    NSPopUpButtonCell* popupButton() const;
    NSSearchFieldCell* search() const;
    NSTextFieldCell* textField() const;

    NSLevelIndicatorStyle levelIndicatorStyleFor(ControlPart) const;
    NSLevelIndicatorCell* levelIndicatorFor(const LayoutMeter*) const;

    int minimumProgressBarHeight(const LayoutStyle&) const;
    const IntSize* progressBarSizes() const;
    const int* progressBarMargins(NSControlSize) const;

protected:
    virtual void adjustMediaSliderThumbSize(LayoutStyle&) const;
    virtual bool paintMediaPlayButton(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaOverlayPlayButton(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaMuteButton(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaSliderTrack(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual String extraFullScreenStyleSheet() override;

    virtual bool paintMediaSliderThumb(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaVolumeSliderContainer(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaVolumeSliderTrack(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaVolumeSliderThumb(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual String formatMediaControlsTime(float time) const override;
    virtual String formatMediaControlsCurrentTime(float currentTime, float duration) const override;
    virtual bool paintMediaFullscreenButton(LayoutObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaToggleClosedCaptionsButton(LayoutObject*, const PaintInfo&, const IntRect&) override;

    // Controls color values returned from platformFocusRingColor(). systemColor() will be used when false.
    bool usesTestModeFocusRingColor() const;
    // A view associated to the contained document. Subclasses may not have such a view and return a fake.
    NSView* documentViewFor(LayoutObject*) const;

    virtual bool shouldUseFallbackTheme(const LayoutStyle&) const override;

private:
    virtual void updateActiveState(NSCell*, const LayoutObject*);
    virtual String extraDefaultStyleSheet() override;
    virtual bool shouldShowPlaceholderWhenFocused() const override;

    mutable RetainPtr<NSPopUpButtonCell> m_popupButton;
    mutable RetainPtr<NSSearchFieldCell> m_search;
    mutable RetainPtr<NSMenu> m_searchMenuTemplate;
    mutable RetainPtr<NSLevelIndicatorCell> m_levelIndicator;
    mutable RetainPtr<NSTextFieldCell> m_textField;

    mutable HashMap<int, RGBA32> m_systemColorCache;

    RetainPtr<LayoutThemeNotificationObserver> m_notificationObserver;
};

} // namespace blink

#endif // LayoutThemeMac_h
