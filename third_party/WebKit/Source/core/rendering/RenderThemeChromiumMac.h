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

#ifndef RenderThemeChromiumMac_h
#define RenderThemeChromiumMac_h

#import "core/rendering/RenderTheme.h"
#import "wtf/HashMap.h"
#import "wtf/RetainPtr.h"

OBJC_CLASS WebCoreRenderThemeNotificationObserver;

namespace blink {

class RenderThemeChromiumMac final : public RenderTheme {
public:
    static PassRefPtr<RenderTheme> create();

    virtual void adjustPaintInvalidationRect(const RenderObject*, IntRect&) override;

    virtual bool isControlStyled(const RenderStyle*, const CachedUAStyle*) const override;

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
    virtual void systemFont(CSSValueID, FontDescription&) const override;

    virtual int minimumMenuListSize(RenderStyle*) const override;

    virtual void adjustSliderThumbSize(RenderStyle*, Element*) const override;

    virtual IntSize sliderTickSize() const override;
    virtual int sliderTickOffsetFromTrackCenter() const override;

    virtual int popupInternalPaddingLeft(RenderStyle*) const override;
    virtual int popupInternalPaddingRight(RenderStyle*) const override;
    virtual int popupInternalPaddingTop(RenderStyle*) const override;
    virtual int popupInternalPaddingBottom(RenderStyle*) const override;

    virtual bool paintCapsLockIndicator(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual bool popsMenuByArrowKeys() const override { return true; }
    virtual bool popsMenuBySpaceKey() const override final { return true; }

    virtual IntSize meterSizeForBounds(const RenderMeter*, const IntRect&) const override;
    virtual bool paintMeter(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool supportsMeter(ControlPart) const override;

    // Returns the repeat interval of the animation for the progress bar.
    virtual double animationRepeatIntervalForProgressBar(RenderProgress*) const override;
    // Returns the duration of the animation for the progress bar.
    virtual double animationDurationForProgressBar(RenderProgress*) const override;

    virtual Color systemColor(CSSValueID) const override;

    virtual bool supportsSelectionForegroundColors() const override { return false; }

    virtual bool isModalColorChooser() const { return false; }

protected:
    RenderThemeChromiumMac();
    virtual ~RenderThemeChromiumMac();

    virtual bool paintTextField(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual bool paintTextArea(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual bool paintMenuList(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual void adjustMenuListStyle(RenderStyle*, Element*) const override;

    virtual bool paintMenuListButton(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual void adjustMenuListButtonStyle(RenderStyle*, Element*) const override;

    virtual bool paintProgressBar(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual bool paintSliderTrack(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual bool paintSliderThumb(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual bool paintSearchField(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual void adjustSearchFieldStyle(RenderStyle*, Element*) const override;

    virtual void adjustSearchFieldCancelButtonStyle(RenderStyle*, Element*) const override;
    virtual bool paintSearchFieldCancelButton(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual void adjustSearchFieldDecorationStyle(RenderStyle*, Element*) const override;
    virtual bool paintSearchFieldDecoration(RenderObject*, const PaintInfo&, const IntRect&) override;

    virtual void adjustSearchFieldResultsDecorationStyle(RenderStyle*, Element*) const override;
    virtual bool paintSearchFieldResultsDecoration(RenderObject*, const PaintInfo&, const IntRect&) override;

private:
    virtual String fileListNameForWidth(Locale&, const FileList*, const Font&, int width) const override;

    FloatRect convertToPaintingRect(const RenderObject* inputRenderer, const RenderObject* partRenderer, const FloatRect& inputRect, const IntRect&) const;

    // Get the control size based off the font. Used by some of the controls (like buttons).
    NSControlSize controlSizeForFont(RenderStyle*) const;
    NSControlSize controlSizeForSystemFont(RenderStyle*) const;
    void setControlSize(NSCell*, const IntSize* sizes, const IntSize& minSize, float zoomLevel = 1.0f);
    void setSizeFromFont(RenderStyle*, const IntSize* sizes) const;
    IntSize sizeForFont(RenderStyle*, const IntSize* sizes) const;
    IntSize sizeForSystemFont(RenderStyle*, const IntSize* sizes) const;
    void setFontFromControlSize(RenderStyle*, NSControlSize) const;

    void updateCheckedState(NSCell*, const RenderObject*);
    void updateEnabledState(NSCell*, const RenderObject*);
    void updateFocusedState(NSCell*, const RenderObject*);
    void updatePressedState(NSCell*, const RenderObject*);

    // Helpers for adjusting appearance and for painting

    void setPopupButtonCellState(const RenderObject*, const IntRect&);
    const IntSize* popupButtonSizes() const;
    const int* popupButtonMargins() const;
    const int* popupButtonPadding(NSControlSize) const;
    const IntSize* menuListSizes() const;

    const IntSize* searchFieldSizes() const;
    const IntSize* cancelButtonSizes() const;
    const IntSize* resultsButtonSizes() const;
    void setSearchCellState(RenderObject*, const IntRect&);
    void setSearchFieldSize(RenderStyle*) const;

    NSPopUpButtonCell* popupButton() const;
    NSSearchFieldCell* search() const;
    NSTextFieldCell* textField() const;

    NSLevelIndicatorStyle levelIndicatorStyleFor(ControlPart) const;
    NSLevelIndicatorCell* levelIndicatorFor(const RenderMeter*) const;

    int minimumProgressBarHeight(RenderStyle*) const;
    const IntSize* progressBarSizes() const;
    const int* progressBarMargins(NSControlSize) const;

protected:
    virtual void adjustMediaSliderThumbSize(RenderStyle*) const;
    virtual bool paintMediaPlayButton(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaOverlayPlayButton(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaMuteButton(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaSliderTrack(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual String extraFullScreenStyleSheet() override;

    virtual bool paintMediaSliderThumb(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaVolumeSliderContainer(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaVolumeSliderTrack(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaVolumeSliderThumb(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual String formatMediaControlsTime(float time) const override;
    virtual String formatMediaControlsCurrentTime(float currentTime, float duration) const override;
    virtual bool paintMediaFullscreenButton(RenderObject*, const PaintInfo&, const IntRect&) override;
    virtual bool paintMediaToggleClosedCaptionsButton(RenderObject*, const PaintInfo&, const IntRect&) override;

    // Controls color values returned from platformFocusRingColor(). systemColor() will be used when false.
    bool usesTestModeFocusRingColor() const;
    // A view associated to the contained document. Subclasses may not have such a view and return a fake.
    NSView* documentViewFor(RenderObject*) const;

    virtual bool shouldUseFallbackTheme(RenderStyle*) const override;

private:
    virtual void updateActiveState(NSCell*, const RenderObject*);
    virtual String extraDefaultStyleSheet() override;
    virtual bool shouldShowPlaceholderWhenFocused() const override;

    mutable RetainPtr<NSPopUpButtonCell> m_popupButton;
    mutable RetainPtr<NSSearchFieldCell> m_search;
    mutable RetainPtr<NSMenu> m_searchMenuTemplate;
    mutable RetainPtr<NSLevelIndicatorCell> m_levelIndicator;
    mutable RetainPtr<NSTextFieldCell> m_textField;

    mutable HashMap<int, RGBA32> m_systemColorCache;

    RetainPtr<WebCoreRenderThemeNotificationObserver> m_notificationObserver;
};

} // namespace blink

#endif // RenderThemeChromiumMac_h
