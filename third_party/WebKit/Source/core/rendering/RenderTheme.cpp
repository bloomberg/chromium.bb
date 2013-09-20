/**
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Computer, Inc.
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
 */

#include "config.h"
#include "core/rendering/RenderTheme.h"

#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "core/dom/Document.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/editing/FrameSelection.h"
#include "core/fileapi/FileList.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLDataListElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLMeterElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/InputTypeNames.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/shadow/MediaControlElements.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html/shadow/SpinButtonElement.h"
#include "core/html/shadow/TextControlInnerElements.h"
#include "core/page/FocusController.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/FileSystem.h"
#include "core/platform/FloatConversion.h"
#include "core/platform/LocalizedStrings.h"
#include "core/platform/graphics/FontSelector.h"
#include "core/platform/graphics/GraphicsContextStateSaver.h"
#include "core/platform/graphics/StringTruncator.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderMeter.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/style/RenderStyle.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFallbackThemeEngine.h"
#include "public/platform/WebRect.h"
#include "wtf/text/StringBuilder.h"

#if ENABLE(INPUT_SPEECH)
#include "core/rendering/RenderInputSpeech.h"
#endif

// The methods in this file are shared by all themes on every platform.

namespace WebCore {

using namespace HTMLNames;

static Color& customFocusRingColor()
{
    DEFINE_STATIC_LOCAL(Color, color, ());
    return color;
}

static WebKit::WebFallbackThemeEngine::State getWebFallbackThemeState(const RenderTheme* theme, const RenderObject* o)
{
    if (!theme->isEnabled(o))
        return WebKit::WebFallbackThemeEngine::StateDisabled;
    if (theme->isPressed(o))
        return WebKit::WebFallbackThemeEngine::StatePressed;
    if (theme->isHovered(o))
        return WebKit::WebFallbackThemeEngine::StateHover;

    return WebKit::WebFallbackThemeEngine::StateNormal;
}

RenderTheme::RenderTheme()
#if USE(NEW_THEME)
    : m_platformTheme(platformTheme())
#endif
{
}

void RenderTheme::adjustStyle(RenderStyle* style, Element* e, const CachedUAStyle& uaStyle)
{
    // Force inline and table display styles to be inline-block (except for table- which is block)
    ControlPart part = style->appearance();
    if (style->display() == INLINE || style->display() == INLINE_TABLE || style->display() == TABLE_ROW_GROUP
        || style->display() == TABLE_HEADER_GROUP || style->display() == TABLE_FOOTER_GROUP
        || style->display() == TABLE_ROW || style->display() == TABLE_COLUMN_GROUP || style->display() == TABLE_COLUMN
        || style->display() == TABLE_CELL || style->display() == TABLE_CAPTION)
        style->setDisplay(INLINE_BLOCK);
    else if (style->display() == COMPACT || style->display() == RUN_IN || style->display() == LIST_ITEM || style->display() == TABLE)
        style->setDisplay(BLOCK);

    if (uaStyle.hasAppearance && isControlStyled(style, uaStyle)) {
        if (part == MenulistPart) {
            style->setAppearance(MenulistButtonPart);
            part = MenulistButtonPart;
        } else
            style->setAppearance(NoControlPart);
    }

    if (!style->hasAppearance())
        return;

    if (shouldUseFallbackTheme(style)) {
        adjustStyleUsingFallbackTheme(style, e);
        return;
    }

#if USE(NEW_THEME)
    switch (part) {
    case CheckboxPart:
    case InnerSpinButtonPart:
    case RadioPart:
    case PushButtonPart:
    case SquareButtonPart:
    case ButtonPart: {
        // Border
        LengthBox borderBox(style->borderTopWidth(), style->borderRightWidth(), style->borderBottomWidth(), style->borderLeftWidth());
        borderBox = m_platformTheme->controlBorder(part, style->font(), borderBox, style->effectiveZoom());
        if (borderBox.top().value() != static_cast<int>(style->borderTopWidth())) {
            if (borderBox.top().value())
                style->setBorderTopWidth(borderBox.top().value());
            else
                style->resetBorderTop();
        }
        if (borderBox.right().value() != static_cast<int>(style->borderRightWidth())) {
            if (borderBox.right().value())
                style->setBorderRightWidth(borderBox.right().value());
            else
                style->resetBorderRight();
        }
        if (borderBox.bottom().value() != static_cast<int>(style->borderBottomWidth())) {
            style->setBorderBottomWidth(borderBox.bottom().value());
            if (borderBox.bottom().value())
                style->setBorderBottomWidth(borderBox.bottom().value());
            else
                style->resetBorderBottom();
        }
        if (borderBox.left().value() != static_cast<int>(style->borderLeftWidth())) {
            style->setBorderLeftWidth(borderBox.left().value());
            if (borderBox.left().value())
                style->setBorderLeftWidth(borderBox.left().value());
            else
                style->resetBorderLeft();
        }

        // Padding
        LengthBox paddingBox = m_platformTheme->controlPadding(part, style->font(), style->paddingBox(), style->effectiveZoom());
        if (paddingBox != style->paddingBox())
            style->setPaddingBox(paddingBox);

        // Whitespace
        if (m_platformTheme->controlRequiresPreWhiteSpace(part))
            style->setWhiteSpace(PRE);

        // Width / Height
        // The width and height here are affected by the zoom.
        // FIXME: Check is flawed, since it doesn't take min-width/max-width into account.
        LengthSize controlSize = m_platformTheme->controlSize(part, style->font(), LengthSize(style->width(), style->height()), style->effectiveZoom());
        if (controlSize.width() != style->width())
            style->setWidth(controlSize.width());
        if (controlSize.height() != style->height())
            style->setHeight(controlSize.height());

        // Min-Width / Min-Height
        LengthSize minControlSize = m_platformTheme->minimumControlSize(part, style->font(), style->effectiveZoom());
        if (minControlSize.width() != style->minWidth())
            style->setMinWidth(minControlSize.width());
        if (minControlSize.height() != style->minHeight())
            style->setMinHeight(minControlSize.height());

        // Font
        FontDescription controlFont = m_platformTheme->controlFont(part, style->font(), style->effectiveZoom());
        if (controlFont != style->font().fontDescription()) {
            // Reset our line-height
            style->setLineHeight(RenderStyle::initialLineHeight());

            // Now update our font.
            if (style->setFontDescription(controlFont))
                style->font().update(0);
        }
    }
    default:
        break;
    }
#endif

    // Call the appropriate style adjustment method based off the appearance value.
    switch (style->appearance()) {
#if !USE(NEW_THEME)
    case CheckboxPart:
        return adjustCheckboxStyle(style, e);
    case RadioPart:
        return adjustRadioStyle(style, e);
    case PushButtonPart:
    case SquareButtonPart:
    case ButtonPart:
        return adjustButtonStyle(style, e);
    case InnerSpinButtonPart:
        return adjustInnerSpinButtonStyle(style, e);
#endif
    case TextFieldPart:
        return adjustTextFieldStyle(style, e);
    case TextAreaPart:
        return adjustTextAreaStyle(style, e);
    case MenulistPart:
        return adjustMenuListStyle(style, e);
    case MenulistButtonPart:
        return adjustMenuListButtonStyle(style, e);
    case MediaPlayButtonPart:
    case MediaCurrentTimePart:
    case MediaTimeRemainingPart:
    case MediaEnterFullscreenButtonPart:
    case MediaExitFullscreenButtonPart:
    case MediaMuteButtonPart:
    case MediaVolumeSliderContainerPart:
        return adjustMediaControlStyle(style, e);
    case MediaSliderPart:
    case MediaVolumeSliderPart:
    case MediaFullScreenVolumeSliderPart:
    case SliderHorizontalPart:
    case SliderVerticalPart:
        return adjustSliderTrackStyle(style, e);
    case SliderThumbHorizontalPart:
    case SliderThumbVerticalPart:
        return adjustSliderThumbStyle(style, e);
    case SearchFieldPart:
        return adjustSearchFieldStyle(style, e);
    case SearchFieldCancelButtonPart:
        return adjustSearchFieldCancelButtonStyle(style, e);
    case SearchFieldDecorationPart:
        return adjustSearchFieldDecorationStyle(style, e);
    case SearchFieldResultsDecorationPart:
        return adjustSearchFieldResultsDecorationStyle(style, e);
    case ProgressBarPart:
        return adjustProgressBarStyle(style, e);
    case MeterPart:
    case RelevancyLevelIndicatorPart:
    case ContinuousCapacityLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
        return adjustMeterStyle(style, e);
#if ENABLE(INPUT_SPEECH)
    case InputSpeechButtonPart:
        return adjustInputFieldSpeechButtonStyle(style, e);
#endif
    default:
        break;
    }
}

bool RenderTheme::paint(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    // If painting is disabled, but we aren't updating control tints, then just bail.
    // If we are updating control tints, just schedule a repaint if the theme supports tinting
    // for that control.
    if (paintInfo.context->updatingControlTints()) {
        if (controlSupportsTints(o))
            o->repaint();
        return false;
    }
    if (paintInfo.context->paintingDisabled())
        return false;

    ControlPart part = o->style()->appearance();

    if (shouldUseFallbackTheme(o->style()))
        return paintUsingFallbackTheme(o, paintInfo, r);

#if USE(NEW_THEME)
    switch (part) {
    case CheckboxPart:
    case RadioPart:
    case PushButtonPart:
    case SquareButtonPart:
    case ButtonPart:
    case InnerSpinButtonPart:
        m_platformTheme->paint(part, controlStatesForRenderer(o), const_cast<GraphicsContext*>(paintInfo.context), r, o->style()->effectiveZoom(), o->view()->frameView());
        return false;
    default:
        break;
    }
#endif

    // Call the appropriate paint method based off the appearance value.
    switch (part) {
#if !USE(NEW_THEME)
    case CheckboxPart:
        return paintCheckbox(o, paintInfo, r);
    case RadioPart:
        return paintRadio(o, paintInfo, r);
    case PushButtonPart:
    case SquareButtonPart:
    case ButtonPart:
        return paintButton(o, paintInfo, r);
    case InnerSpinButtonPart:
        return paintInnerSpinButton(o, paintInfo, r);
#endif
    case MenulistPart:
        return paintMenuList(o, paintInfo, r);
    case MeterPart:
    case RelevancyLevelIndicatorPart:
    case ContinuousCapacityLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
        return paintMeter(o, paintInfo, r);
    case ProgressBarPart:
        return paintProgressBar(o, paintInfo, r);
    case SliderHorizontalPart:
    case SliderVerticalPart:
        return paintSliderTrack(o, paintInfo, r);
    case SliderThumbHorizontalPart:
    case SliderThumbVerticalPart:
        return paintSliderThumb(o, paintInfo, r);
    case MediaEnterFullscreenButtonPart:
    case MediaExitFullscreenButtonPart:
        return paintMediaFullscreenButton(o, paintInfo, r);
    case MediaPlayButtonPart:
        return paintMediaPlayButton(o, paintInfo, r);
    case MediaOverlayPlayButtonPart:
        return paintMediaOverlayPlayButton(o, paintInfo, r);
    case MediaMuteButtonPart:
        return paintMediaMuteButton(o, paintInfo, r);
    case MediaSeekBackButtonPart:
        return paintMediaSeekBackButton(o, paintInfo, r);
    case MediaSeekForwardButtonPart:
        return paintMediaSeekForwardButton(o, paintInfo, r);
    case MediaRewindButtonPart:
        return paintMediaRewindButton(o, paintInfo, r);
    case MediaReturnToRealtimeButtonPart:
        return paintMediaReturnToRealtimeButton(o, paintInfo, r);
    case MediaToggleClosedCaptionsButtonPart:
        return paintMediaToggleClosedCaptionsButton(o, paintInfo, r);
    case MediaSliderPart:
        return paintMediaSliderTrack(o, paintInfo, r);
    case MediaSliderThumbPart:
        return paintMediaSliderThumb(o, paintInfo, r);
    case MediaVolumeSliderMuteButtonPart:
        return paintMediaMuteButton(o, paintInfo, r);
    case MediaVolumeSliderContainerPart:
        return paintMediaVolumeSliderContainer(o, paintInfo, r);
    case MediaVolumeSliderPart:
        return paintMediaVolumeSliderTrack(o, paintInfo, r);
    case MediaVolumeSliderThumbPart:
        return paintMediaVolumeSliderThumb(o, paintInfo, r);
    case MediaFullScreenVolumeSliderPart:
        return paintMediaFullScreenVolumeSliderTrack(o, paintInfo, r);
    case MediaFullScreenVolumeSliderThumbPart:
        return paintMediaFullScreenVolumeSliderThumb(o, paintInfo, r);
    case MediaTimeRemainingPart:
        return paintMediaTimeRemaining(o, paintInfo, r);
    case MediaCurrentTimePart:
        return paintMediaCurrentTime(o, paintInfo, r);
    case MediaControlsBackgroundPart:
        return paintMediaControlsBackground(o, paintInfo, r);
    case MenulistButtonPart:
    case TextFieldPart:
    case TextAreaPart:
    case ListboxPart:
        return true;
    case SearchFieldPart:
        return paintSearchField(o, paintInfo, r);
    case SearchFieldCancelButtonPart:
        return paintSearchFieldCancelButton(o, paintInfo, r);
    case SearchFieldDecorationPart:
        return paintSearchFieldDecoration(o, paintInfo, r);
    case SearchFieldResultsDecorationPart:
        return paintSearchFieldResultsDecoration(o, paintInfo, r);
#if ENABLE(INPUT_SPEECH)
    case InputSpeechButtonPart:
        return paintInputFieldSpeechButton(o, paintInfo, r);
#endif
    default:
        break;
    }

    return true; // We don't support the appearance, so let the normal background/border paint.
}

bool RenderTheme::paintBorderOnly(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    if (paintInfo.context->paintingDisabled())
        return false;

    // Call the appropriate paint method based off the appearance value.
    switch (o->style()->appearance()) {
    case TextFieldPart:
        return paintTextField(o, paintInfo, r);
    case ListboxPart:
    case TextAreaPart:
        return paintTextArea(o, paintInfo, r);
    case MenulistButtonPart:
    case SearchFieldPart:
        return true;
    case CheckboxPart:
    case RadioPart:
    case PushButtonPart:
    case SquareButtonPart:
    case ButtonPart:
    case MenulistPart:
    case MeterPart:
    case RelevancyLevelIndicatorPart:
    case ContinuousCapacityLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
    case ProgressBarPart:
    case SliderHorizontalPart:
    case SliderVerticalPart:
    case SliderThumbHorizontalPart:
    case SliderThumbVerticalPart:
    case SearchFieldCancelButtonPart:
    case SearchFieldDecorationPart:
    case SearchFieldResultsDecorationPart:
#if ENABLE(INPUT_SPEECH)
    case InputSpeechButtonPart:
#endif
    default:
        break;
    }

    return false;
}

bool RenderTheme::paintDecorations(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    if (paintInfo.context->paintingDisabled())
        return false;

    // Call the appropriate paint method based off the appearance value.
    switch (o->style()->appearance()) {
    case MenulistButtonPart:
        return paintMenuListButton(o, paintInfo, r);
    case TextFieldPart:
    case TextAreaPart:
    case ListboxPart:
    case CheckboxPart:
    case RadioPart:
    case PushButtonPart:
    case SquareButtonPart:
    case ButtonPart:
    case MenulistPart:
    case MeterPart:
    case RelevancyLevelIndicatorPart:
    case ContinuousCapacityLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
    case ProgressBarPart:
    case SliderHorizontalPart:
    case SliderVerticalPart:
    case SliderThumbHorizontalPart:
    case SliderThumbVerticalPart:
    case SearchFieldPart:
    case SearchFieldCancelButtonPart:
    case SearchFieldDecorationPart:
    case SearchFieldResultsDecorationPart:
#if ENABLE(INPUT_SPEECH)
    case InputSpeechButtonPart:
#endif
    default:
        break;
    }

    return false;
}

String RenderTheme::extraDefaultStyleSheet()
{
    if (!RuntimeEnabledFeatures::dataListElementEnabled() && !RuntimeEnabledFeatures::dialogElementEnabled())
        return String();
    StringBuilder runtimeCSS;

    if (RuntimeEnabledFeatures::dataListElementEnabled()) {
        runtimeCSS.appendLiteral("datalist {display: none ;}");

        if (RuntimeEnabledFeatures::inputTypeColorEnabled()) {
            runtimeCSS.appendLiteral("input[type=\"color\"][list] { -webkit-appearance: menulist; width: 88px; height: 23px;}");
            runtimeCSS.appendLiteral("input[type=\"color\"][list]::-webkit-color-swatch-wrapper { padding-left: 8px; padding-right: 24px;}");
            runtimeCSS.appendLiteral("input[type=\"color\"][list]::-webkit-color-swatch { border-color: #000000;}");
        }
    }
    if (RuntimeEnabledFeatures::dialogElementEnabled()) {
        runtimeCSS.appendLiteral("dialog:not([open]) { display: none; }");
        runtimeCSS.appendLiteral("dialog { position: absolute; left: 0; right: 0; width: -webkit-fit-content; height: -webkit-fit-content; margin: auto; border: solid; padding: 1em; background: white; color: black;}");
        runtimeCSS.appendLiteral("dialog::backdrop { background: rgba(0,0,0,0.1); }");
    }

    return runtimeCSS.toString();
}

String RenderTheme::formatMediaControlsTime(float time) const
{
    if (!std::isfinite(time))
        time = 0;
    int seconds = (int)fabsf(time);
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;
    if (hours) {
        if (hours > 9)
            return String::format("%s%02d:%02d:%02d", (time < 0 ? "-" : ""), hours, minutes, seconds);

        return String::format("%s%01d:%02d:%02d", (time < 0 ? "-" : ""), hours, minutes, seconds);
    }

    return String::format("%s%02d:%02d", (time < 0 ? "-" : ""), minutes, seconds);
}

String RenderTheme::formatMediaControlsCurrentTime(float currentTime, float /*duration*/) const
{
    return formatMediaControlsTime(currentTime);
}

Color RenderTheme::activeSelectionBackgroundColor() const
{
    if (!m_activeSelectionBackgroundColor.isValid())
        m_activeSelectionBackgroundColor = platformActiveSelectionBackgroundColor().blendWithWhite();
    return m_activeSelectionBackgroundColor;
}

Color RenderTheme::inactiveSelectionBackgroundColor() const
{
    if (!m_inactiveSelectionBackgroundColor.isValid())
        m_inactiveSelectionBackgroundColor = platformInactiveSelectionBackgroundColor().blendWithWhite();
    return m_inactiveSelectionBackgroundColor;
}

Color RenderTheme::activeSelectionForegroundColor() const
{
    if (!m_activeSelectionForegroundColor.isValid() && supportsSelectionForegroundColors())
        m_activeSelectionForegroundColor = platformActiveSelectionForegroundColor();
    return m_activeSelectionForegroundColor;
}

Color RenderTheme::inactiveSelectionForegroundColor() const
{
    if (!m_inactiveSelectionForegroundColor.isValid() && supportsSelectionForegroundColors())
        m_inactiveSelectionForegroundColor = platformInactiveSelectionForegroundColor();
    return m_inactiveSelectionForegroundColor;
}

Color RenderTheme::activeListBoxSelectionBackgroundColor() const
{
    if (!m_activeListBoxSelectionBackgroundColor.isValid())
        m_activeListBoxSelectionBackgroundColor = platformActiveListBoxSelectionBackgroundColor();
    return m_activeListBoxSelectionBackgroundColor;
}

Color RenderTheme::inactiveListBoxSelectionBackgroundColor() const
{
    if (!m_inactiveListBoxSelectionBackgroundColor.isValid())
        m_inactiveListBoxSelectionBackgroundColor = platformInactiveListBoxSelectionBackgroundColor();
    return m_inactiveListBoxSelectionBackgroundColor;
}

Color RenderTheme::activeListBoxSelectionForegroundColor() const
{
    if (!m_activeListBoxSelectionForegroundColor.isValid() && supportsListBoxSelectionForegroundColors())
        m_activeListBoxSelectionForegroundColor = platformActiveListBoxSelectionForegroundColor();
    return m_activeListBoxSelectionForegroundColor;
}

Color RenderTheme::inactiveListBoxSelectionForegroundColor() const
{
    if (!m_inactiveListBoxSelectionForegroundColor.isValid() && supportsListBoxSelectionForegroundColors())
        m_inactiveListBoxSelectionForegroundColor = platformInactiveListBoxSelectionForegroundColor();
    return m_inactiveListBoxSelectionForegroundColor;
}

Color RenderTheme::platformActiveSelectionBackgroundColor() const
{
    // Use a blue color by default if the platform theme doesn't define anything.
    return Color(0, 0, 255);
}

Color RenderTheme::platformActiveSelectionForegroundColor() const
{
    // Use a white color by default if the platform theme doesn't define anything.
    return Color::white;
}

Color RenderTheme::platformInactiveSelectionBackgroundColor() const
{
    // Use a grey color by default if the platform theme doesn't define anything.
    // This color matches Firefox's inactive color.
    return Color(176, 176, 176);
}

Color RenderTheme::platformInactiveSelectionForegroundColor() const
{
    // Use a black color by default.
    return Color::black;
}

Color RenderTheme::platformActiveListBoxSelectionBackgroundColor() const
{
    return platformActiveSelectionBackgroundColor();
}

Color RenderTheme::platformActiveListBoxSelectionForegroundColor() const
{
    return platformActiveSelectionForegroundColor();
}

Color RenderTheme::platformInactiveListBoxSelectionBackgroundColor() const
{
    return platformInactiveSelectionBackgroundColor();
}

Color RenderTheme::platformInactiveListBoxSelectionForegroundColor() const
{
    return platformInactiveSelectionForegroundColor();
}

int RenderTheme::baselinePosition(const RenderObject* o) const
{
    if (!o->isBox())
        return 0;

    const RenderBox* box = toRenderBox(o);

#if USE(NEW_THEME)
    return box->height() + box->marginTop() + m_platformTheme->baselinePositionAdjustment(o->style()->appearance()) * o->style()->effectiveZoom();
#else
    return box->height() + box->marginTop();
#endif
}

bool RenderTheme::isControlContainer(ControlPart appearance) const
{
    // There are more leaves than this, but we'll patch this function as we add support for
    // more controls.
    return appearance != CheckboxPart && appearance != RadioPart;
}

static bool isBackgroundOrBorderStyled(const RenderStyle& style, const CachedUAStyle& uaStyle)
{
    // Code below excludes the background-repeat from comparison by resetting it
    FillLayer backgroundCopy = uaStyle.backgroundLayers;
    FillLayer backgroundLayersCopy = *style.backgroundLayers();
    backgroundCopy.setRepeatX(NoRepeatFill);
    backgroundCopy.setRepeatY(NoRepeatFill);
    backgroundLayersCopy.setRepeatX(NoRepeatFill);
    backgroundLayersCopy.setRepeatY(NoRepeatFill);
    // Test the style to see if the UA border and background match.
    return style.border() != uaStyle.border
        || backgroundLayersCopy != backgroundCopy
        || style.visitedDependentColor(CSSPropertyBackgroundColor) != uaStyle.backgroundColor;
}

bool RenderTheme::isControlStyled(const RenderStyle* style, const CachedUAStyle& uaStyle) const
{
    switch (style->appearance()) {
    case PushButtonPart:
    case SquareButtonPart:
    case ButtonPart:
    case ProgressBarPart:
    case MeterPart:
    case RelevancyLevelIndicatorPart:
    case ContinuousCapacityLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
        return isBackgroundOrBorderStyled(*style, uaStyle);

    case ListboxPart:
    case MenulistPart:
    case SearchFieldPart:
    case TextAreaPart:
    case TextFieldPart:
        return isBackgroundOrBorderStyled(*style, uaStyle) || style->boxShadow();

    case SliderHorizontalPart:
    case SliderVerticalPart:
        return style->boxShadow();

    default:
        return false;
    }
}

void RenderTheme::adjustRepaintRect(const RenderObject* o, IntRect& r)
{
#if USE(NEW_THEME)
    m_platformTheme->inflateControlPaintRect(o->style()->appearance(), controlStatesForRenderer(o), r, o->style()->effectiveZoom());
#else
    UNUSED_PARAM(o);
    UNUSED_PARAM(r);
#endif
}

bool RenderTheme::shouldDrawDefaultFocusRing(RenderObject* renderer) const
{
    if (supportsFocusRing(renderer->style()))
        return false;
    if (!renderer->style()->hasAppearance())
        return true;
    Node* node = renderer->node();
    if (!node)
        return true;
    // We can't use RenderTheme::isFocused because outline:auto might be
    // specified to non-:focus rulesets.
    if (node->focused() && !node->shouldHaveFocusAppearance())
        return false;
    return true;
}

bool RenderTheme::supportsFocusRing(const RenderStyle* style) const
{
    return (style->hasAppearance() && style->appearance() != TextFieldPart && style->appearance() != TextAreaPart && style->appearance() != MenulistButtonPart && style->appearance() != ListboxPart);
}

bool RenderTheme::stateChanged(RenderObject* o, ControlState state) const
{
    // Default implementation assumes the controls don't respond to changes in :hover state
    if (state == HoverState && !supportsHover(o->style()))
        return false;

    // Assume pressed state is only responded to if the control is enabled.
    if (state == PressedState && !isEnabled(o))
        return false;

    // Repaint the control.
    o->repaint();
    return true;
}

ControlStates RenderTheme::controlStatesForRenderer(const RenderObject* o) const
{
    ControlStates result = 0;
    if (isHovered(o)) {
        result |= HoverState;
        if (isSpinUpButtonPartHovered(o))
            result |= SpinUpState;
    }
    if (isPressed(o)) {
        result |= PressedState;
        if (isSpinUpButtonPartPressed(o))
            result |= SpinUpState;
    }
    if (isFocused(o) && o->style()->outlineStyleIsAuto())
        result |= FocusState;
    if (isEnabled(o))
        result |= EnabledState;
    if (isChecked(o))
        result |= CheckedState;
    if (isReadOnlyControl(o))
        result |= ReadOnlyState;
    if (!isActive(o))
        result |= WindowInactiveState;
    if (isIndeterminate(o))
        result |= IndeterminateState;
    return result;
}

bool RenderTheme::isActive(const RenderObject* o) const
{
    Node* node = o->node();
    if (!node)
        return false;

    Page* page = node->document().page();
    if (!page)
        return false;

    return page->focusController().isActive();
}

bool RenderTheme::isChecked(const RenderObject* o) const
{
    if (!o->node() || !o->node()->hasTagName(inputTag))
        return false;
    return toHTMLInputElement(o->node())->shouldAppearChecked();
}

bool RenderTheme::isIndeterminate(const RenderObject* o) const
{
    if (!o->node() || !o->node()->hasTagName(inputTag))
        return false;
    return toHTMLInputElement(o->node())->shouldAppearIndeterminate();
}

bool RenderTheme::isEnabled(const RenderObject* o) const
{
    Node* node = o->node();
    if (!node || !node->isElementNode())
        return true;
    return !toElement(node)->isDisabledFormControl();
}

bool RenderTheme::isFocused(const RenderObject* o) const
{
    Node* node = o->node();
    if (!node)
        return false;

    node = node->focusDelegate();
    Document& document = node->document();
    Frame* frame = document.frame();
    return node == document.focusedElement() && node->shouldHaveFocusAppearance() && frame && frame->selection().isFocusedAndActive();
}

bool RenderTheme::isPressed(const RenderObject* o) const
{
    if (!o->node())
        return false;
    return o->node()->active();
}

bool RenderTheme::isSpinUpButtonPartPressed(const RenderObject* o) const
{
    Node* node = o->node();
    if (!node || !node->active() || !node->isElementNode()
        || !toElement(node)->isSpinButtonElement())
        return false;
    SpinButtonElement* element = toSpinButtonElement(node);
    return element->upDownState() == SpinButtonElement::Up;
}

bool RenderTheme::isReadOnlyControl(const RenderObject* o) const
{
    Node* node = o->node();
    if (!node || !node->isElementNode())
        return false;
    return toElement(node)->matchesReadOnlyPseudoClass();
}

bool RenderTheme::isHovered(const RenderObject* o) const
{
    Node* node = o->node();
    if (!node)
        return false;
    if (!node->isElementNode() || !toElement(node)->isSpinButtonElement())
        return node->hovered();
    SpinButtonElement* element = toSpinButtonElement(node);
    return element->hovered() && element->upDownState() != SpinButtonElement::Indeterminate;
}

bool RenderTheme::isSpinUpButtonPartHovered(const RenderObject* o) const
{
    Node* node = o->node();
    if (!node || !node->isElementNode() || !toElement(node)->isSpinButtonElement())
        return false;
    SpinButtonElement* element = toSpinButtonElement(node);
    return element->upDownState() == SpinButtonElement::Up;
}

#if !USE(NEW_THEME)

void RenderTheme::adjustCheckboxStyle(RenderStyle* style, Element*) const
{
    // A summary of the rules for checkbox designed to match WinIE:
    // width/height - honored (WinIE actually scales its control for small widths, but lets it overflow for small heights.)
    // font-size - not honored (control has no text), but we use it to decide which control size to use.
    setCheckboxSize(style);

    // padding - not honored by WinIE, needs to be removed.
    style->resetPadding();

    // border - honored by WinIE, but looks terrible (just paints in the control box and turns off the Windows XP theme)
    // for now, we will not honor it.
    style->resetBorder();
}

void RenderTheme::adjustRadioStyle(RenderStyle* style, Element*) const
{
    // A summary of the rules for checkbox designed to match WinIE:
    // width/height - honored (WinIE actually scales its control for small widths, but lets it overflow for small heights.)
    // font-size - not honored (control has no text), but we use it to decide which control size to use.
    setRadioSize(style);

    // padding - not honored by WinIE, needs to be removed.
    style->resetPadding();

    // border - honored by WinIE, but looks terrible (just paints in the control box and turns off the Windows XP theme)
    // for now, we will not honor it.
    style->resetBorder();
}

void RenderTheme::adjustButtonStyle(RenderStyle* style, Element*) const
{
    // Most platforms will completely honor all CSS, and so we have no need to
    // adjust the style at all by default. We will still allow the theme a crack
    // at setting up a desired vertical size.
    setButtonSize(style);
}

void RenderTheme::adjustInnerSpinButtonStyle(RenderStyle*, Element*) const
{
}
#endif

void RenderTheme::adjustTextFieldStyle(RenderStyle*, Element*) const
{
}

void RenderTheme::adjustTextAreaStyle(RenderStyle*, Element*) const
{
}

void RenderTheme::adjustMenuListStyle(RenderStyle*, Element*) const
{
}

#if ENABLE(INPUT_SPEECH)
void RenderTheme::adjustInputFieldSpeechButtonStyle(RenderStyle* style, Element* element) const
{
    RenderInputSpeech::adjustInputFieldSpeechButtonStyle(style, element);
}

bool RenderTheme::paintInputFieldSpeechButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderInputSpeech::paintInputFieldSpeechButton(object, paintInfo, rect);
}
#endif

void RenderTheme::adjustMeterStyle(RenderStyle* style, Element*) const
{
}

IntSize RenderTheme::meterSizeForBounds(const RenderMeter*, const IntRect& bounds) const
{
    return bounds.size();
}

bool RenderTheme::supportsMeter(ControlPart) const
{
    return false;
}

bool RenderTheme::paintMeter(RenderObject*, const PaintInfo&, const IntRect&)
{
    return true;
}

LayoutUnit RenderTheme::sliderTickSnappingThreshold() const
{
    return 5;
}

void RenderTheme::paintSliderTicks(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
    Node* node = o->node();
    if (!node || !node->hasTagName(inputTag))
        return;

    HTMLInputElement* input = toHTMLInputElement(node);
    HTMLDataListElement* dataList = input->dataList();
    if (!dataList)
        return;

    double min = input->minimum();
    double max = input->maximum();
    ControlPart part = o->style()->appearance();
    // We don't support ticks on alternate sliders like MediaVolumeSliders.
    if (part !=  SliderHorizontalPart && part != SliderVerticalPart)
        return;
    bool isHorizontal = part ==  SliderHorizontalPart;

    IntSize thumbSize;
    RenderObject* thumbRenderer = input->userAgentShadowRoot()->getElementById(ShadowElementNames::sliderThumb())->renderer();
    if (thumbRenderer) {
        RenderStyle* thumbStyle = thumbRenderer->style();
        int thumbWidth = thumbStyle->width().intValue();
        int thumbHeight = thumbStyle->height().intValue();
        thumbSize.setWidth(isHorizontal ? thumbWidth : thumbHeight);
        thumbSize.setHeight(isHorizontal ? thumbHeight : thumbWidth);
    }

    IntSize tickSize = sliderTickSize();
    float zoomFactor = o->style()->effectiveZoom();
    FloatRect tickRect;
    int tickRegionSideMargin = 0;
    int tickRegionWidth = 0;
    IntRect trackBounds;
    RenderObject* trackRenderer = input->userAgentShadowRoot()->getElementById(ShadowElementNames::sliderTrack())->renderer();
    // We can ignoring transforms because transform is handled by the graphics context.
    if (trackRenderer)
        trackBounds = trackRenderer->absoluteBoundingBoxRectIgnoringTransforms();
    IntRect sliderBounds = o->absoluteBoundingBoxRectIgnoringTransforms();

    // Make position relative to the transformed ancestor element.
    trackBounds.setX(trackBounds.x() - sliderBounds.x() + rect.x());
    trackBounds.setY(trackBounds.y() - sliderBounds.y() + rect.y());

    if (isHorizontal) {
        tickRect.setWidth(floor(tickSize.width() * zoomFactor));
        tickRect.setHeight(floor(tickSize.height() * zoomFactor));
        tickRect.setY(floor(rect.y() + rect.height() / 2.0 + sliderTickOffsetFromTrackCenter() * zoomFactor));
        tickRegionSideMargin = trackBounds.x() + (thumbSize.width() - tickSize.width() * zoomFactor) / 2.0;
        tickRegionWidth = trackBounds.width() - thumbSize.width();
    } else {
        tickRect.setWidth(floor(tickSize.height() * zoomFactor));
        tickRect.setHeight(floor(tickSize.width() * zoomFactor));
        tickRect.setX(floor(rect.x() + rect.width() / 2.0 + sliderTickOffsetFromTrackCenter() * zoomFactor));
        tickRegionSideMargin = trackBounds.y() + (thumbSize.width() - tickSize.width() * zoomFactor) / 2.0;
        tickRegionWidth = trackBounds.height() - thumbSize.width();
    }
    RefPtr<HTMLCollection> options = dataList->options();
    GraphicsContextStateSaver stateSaver(*paintInfo.context);
    paintInfo.context->setFillColor(o->resolveColor(CSSPropertyColor));
    for (unsigned i = 0; Node* node = options->item(i); i++) {
        ASSERT(node->hasTagName(optionTag));
        HTMLOptionElement* optionElement = toHTMLOptionElement(node);
        String value = optionElement->value();
        if (!input->isValidValue(value))
            continue;
        double parsedValue = parseToDoubleForNumberType(input->sanitizeValue(value));
        double tickFraction = (parsedValue - min) / (max - min);
        double tickRatio = isHorizontal && o->style()->isLeftToRightDirection() ? tickFraction : 1.0 - tickFraction;
        double tickPosition = round(tickRegionSideMargin + tickRegionWidth * tickRatio);
        if (isHorizontal)
            tickRect.setX(tickPosition);
        else
            tickRect.setY(tickPosition);
        paintInfo.context->fillRect(tickRect);
    }
}

double RenderTheme::animationRepeatIntervalForProgressBar(RenderProgress*) const
{
    return 0;
}

double RenderTheme::animationDurationForProgressBar(RenderProgress*) const
{
    return 0;
}

void RenderTheme::adjustProgressBarStyle(RenderStyle*, Element*) const
{
}

bool RenderTheme::shouldHaveSpinButton(HTMLInputElement* inputElement) const
{
    return inputElement->isSteppable() && !inputElement->isRangeControl();
}

void RenderTheme::adjustMenuListButtonStyle(RenderStyle*, Element*) const
{
}

void RenderTheme::adjustMediaControlStyle(RenderStyle*, Element*) const
{
}

void RenderTheme::adjustSliderTrackStyle(RenderStyle*, Element*) const
{
}

void RenderTheme::adjustSliderThumbStyle(RenderStyle* style, Element* element) const
{
    adjustSliderThumbSize(style, element);
}

void RenderTheme::adjustSliderThumbSize(RenderStyle*, Element*) const
{
}

void RenderTheme::adjustSearchFieldStyle(RenderStyle*, Element*) const
{
}

void RenderTheme::adjustSearchFieldCancelButtonStyle(RenderStyle*, Element*) const
{
}

void RenderTheme::adjustSearchFieldDecorationStyle(RenderStyle*, Element*) const
{
}

void RenderTheme::adjustSearchFieldResultsDecorationStyle(RenderStyle*, Element*) const
{
}

void RenderTheme::platformColorsDidChange()
{
    m_activeSelectionForegroundColor = Color();
    m_inactiveSelectionForegroundColor = Color();
    m_activeSelectionBackgroundColor = Color();
    m_inactiveSelectionBackgroundColor = Color();

    m_activeListBoxSelectionForegroundColor = Color();
    m_inactiveListBoxSelectionForegroundColor = Color();
    m_activeListBoxSelectionBackgroundColor = Color();
    m_inactiveListBoxSelectionForegroundColor = Color();

    Page::scheduleForcedStyleRecalcForAllPages();
}

Color RenderTheme::systemColor(CSSValueID cssValueId) const
{
    switch (cssValueId) {
    case CSSValueActiveborder:
        return 0xFFFFFFFF;
    case CSSValueActivecaption:
        return 0xFFCCCCCC;
    case CSSValueAppworkspace:
        return 0xFFFFFFFF;
    case CSSValueBackground:
        return 0xFF6363CE;
    case CSSValueButtonface:
        return 0xFFC0C0C0;
    case CSSValueButtonhighlight:
        return 0xFFDDDDDD;
    case CSSValueButtonshadow:
        return 0xFF888888;
    case CSSValueButtontext:
        return 0xFF000000;
    case CSSValueCaptiontext:
        return 0xFF000000;
    case CSSValueGraytext:
        return 0xFF808080;
    case CSSValueHighlight:
        return 0xFFB5D5FF;
    case CSSValueHighlighttext:
        return 0xFF000000;
    case CSSValueInactiveborder:
        return 0xFFFFFFFF;
    case CSSValueInactivecaption:
        return 0xFFFFFFFF;
    case CSSValueInactivecaptiontext:
        return 0xFF7F7F7F;
    case CSSValueInfobackground:
        return 0xFFFBFCC5;
    case CSSValueInfotext:
        return 0xFF000000;
    case CSSValueMenu:
        return 0xFFC0C0C0;
    case CSSValueMenutext:
        return 0xFF000000;
    case CSSValueScrollbar:
        return 0xFFFFFFFF;
    case CSSValueText:
        return 0xFF000000;
    case CSSValueThreeddarkshadow:
        return 0xFF666666;
    case CSSValueThreedface:
        return 0xFFC0C0C0;
    case CSSValueThreedhighlight:
        return 0xFFDDDDDD;
    case CSSValueThreedlightshadow:
        return 0xFFC0C0C0;
    case CSSValueThreedshadow:
        return 0xFF888888;
    case CSSValueWindow:
        return 0xFFFFFFFF;
    case CSSValueWindowframe:
        return 0xFFCCCCCC;
    case CSSValueWindowtext:
        return 0xFF000000;
    case CSSValueInternalActiveListBoxSelection:
        return activeListBoxSelectionBackgroundColor();
        break;
    case CSSValueInternalActiveListBoxSelectionText:
        return activeListBoxSelectionForegroundColor();
        break;
    case CSSValueInternalInactiveListBoxSelection:
        return inactiveListBoxSelectionBackgroundColor();
        break;
    case CSSValueInternalInactiveListBoxSelectionText:
        return inactiveListBoxSelectionForegroundColor();
        break;
    default:
        break;
    }
    return Color();
}

Color RenderTheme::platformActiveTextSearchHighlightColor() const
{
    return Color(255, 150, 50); // Orange.
}

Color RenderTheme::platformInactiveTextSearchHighlightColor() const
{
    return Color(255, 255, 0); // Yellow.
}

Color RenderTheme::tapHighlightColor()
{
    return theme().platformTapHighlightColor();
}

void RenderTheme::setCustomFocusRingColor(const Color& c)
{
    customFocusRingColor() = c;
}

Color RenderTheme::focusRingColor()
{
    return customFocusRingColor().isValid() ? customFocusRingColor() : theme().platformFocusRingColor();
}

String RenderTheme::fileListDefaultLabel(bool multipleFilesAllowed) const
{
    if (multipleFilesAllowed)
        return fileButtonNoFilesSelectedLabel();
    return fileButtonNoFileSelectedLabel();
}

String RenderTheme::fileListNameForWidth(const FileList* fileList, const Font& font, int width, bool multipleFilesAllowed) const
{
    if (width <= 0)
        return String();

    String string;
    if (fileList->isEmpty())
        string = fileListDefaultLabel(multipleFilesAllowed);
    else if (fileList->length() == 1)
        string = fileList->item(0)->name();
    else
        return StringTruncator::rightTruncate(multipleFileUploadText(fileList->length()), width, font, StringTruncator::EnableRoundingHacks);

    return StringTruncator::centerTruncate(string, width, font, StringTruncator::EnableRoundingHacks);
}

bool RenderTheme::shouldOpenPickerWithF4Key() const
{
    return false;
}

bool RenderTheme::supportsDataListUI(const AtomicString& type) const
{
    return type == InputTypeNames::text() || type == InputTypeNames::search() || type == InputTypeNames::url()
        || type == InputTypeNames::telephone() || type == InputTypeNames::email() || type == InputTypeNames::number()
        || type == InputTypeNames::color()
        || type == InputTypeNames::date()
        || type == InputTypeNames::datetime()
        || type == InputTypeNames::datetimelocal()
        || type == InputTypeNames::month()
        || type == InputTypeNames::week()
        || type == InputTypeNames::time()
        || type == InputTypeNames::range();
}

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
bool RenderTheme::supportsCalendarPicker(const AtomicString& type) const
{
    return type == InputTypeNames::date()
        || type == InputTypeNames::datetime()
        || type == InputTypeNames::datetimelocal()
        || type == InputTypeNames::month()
        || type == InputTypeNames::week();
}
#endif

bool RenderTheme::shouldUseFallbackTheme(RenderStyle*) const
{
    return false;
}

void RenderTheme::adjustStyleUsingFallbackTheme(RenderStyle* style, Element* e)
{
    ControlPart part = style->appearance();
    switch (part) {
    case CheckboxPart:
        return adjustCheckboxStyleUsingFallbackTheme(style, e);
    case RadioPart:
        return adjustRadioStyleUsingFallbackTheme(style, e);
    default:
        break;
    }
}

bool RenderTheme::paintUsingFallbackTheme(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    ControlPart part = o->style()->appearance();
    switch (part) {
    case CheckboxPart:
        return paintCheckboxUsingFallbackTheme(o, i, r);
    case RadioPart:
        return paintRadioUsingFallbackTheme(o, i, r);
    default:
        break;
    }
    return true;
}

// static
void RenderTheme::setSizeIfAuto(RenderStyle* style, const IntSize& size)
{
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(size.width(), Fixed));
    if (style->height().isAuto())
        style->setHeight(Length(size.height(), Fixed));
}

bool RenderTheme::paintCheckboxUsingFallbackTheme(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    WebKit::WebFallbackThemeEngine::ExtraParams extraParams;
    WebKit::WebCanvas* canvas = i.context->canvas();
    extraParams.button.checked = isChecked(o);
    extraParams.button.indeterminate = isIndeterminate(o);

    float zoomLevel = o->style()->effectiveZoom();
    GraphicsContextStateSaver stateSaver(*i.context);
    IntRect unzoomedRect = r;
    if (zoomLevel != 1) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        i.context->translate(unzoomedRect.x(), unzoomedRect.y());
        i.context->scale(FloatSize(zoomLevel, zoomLevel));
        i.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    WebKit::Platform::current()->fallbackThemeEngine()->paint(canvas, WebKit::WebFallbackThemeEngine::PartCheckbox, getWebFallbackThemeState(this, o), WebKit::WebRect(unzoomedRect), &extraParams);
    return false;
}

void RenderTheme::adjustCheckboxStyleUsingFallbackTheme(RenderStyle* style, Element*) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    IntSize size = WebKit::Platform::current()->fallbackThemeEngine()->getSize(WebKit::WebFallbackThemeEngine::PartCheckbox);
    float zoomLevel = style->effectiveZoom();
    size.setWidth(size.width() * zoomLevel);
    size.setHeight(size.height() * zoomLevel);
    setSizeIfAuto(style, size);

    // padding - not honored by WinIE, needs to be removed.
    style->resetPadding();

    // border - honored by WinIE, but looks terrible (just paints in the control box and turns off the Windows XP theme)
    // for now, we will not honor it.
    style->resetBorder();
}

bool RenderTheme::paintRadioUsingFallbackTheme(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    WebKit::WebFallbackThemeEngine::ExtraParams extraParams;
    WebKit::WebCanvas* canvas = i.context->canvas();
    extraParams.button.checked = isChecked(o);
    extraParams.button.indeterminate = isIndeterminate(o);

    float zoomLevel = o->style()->effectiveZoom();
    GraphicsContextStateSaver stateSaver(*i.context);
    IntRect unzoomedRect = r;
    if (zoomLevel != 1) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        i.context->translate(unzoomedRect.x(), unzoomedRect.y());
        i.context->scale(FloatSize(zoomLevel, zoomLevel));
        i.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    WebKit::Platform::current()->fallbackThemeEngine()->paint(canvas, WebKit::WebFallbackThemeEngine::PartRadio, getWebFallbackThemeState(this, o), WebKit::WebRect(unzoomedRect), &extraParams);
    return false;
}

void RenderTheme::adjustRadioStyleUsingFallbackTheme(RenderStyle* style, Element*) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    IntSize size = WebKit::Platform::current()->fallbackThemeEngine()->getSize(WebKit::WebFallbackThemeEngine::PartRadio);
    float zoomLevel = style->effectiveZoom();
    size.setWidth(size.width() * zoomLevel);
    size.setHeight(size.height() * zoomLevel);
    setSizeIfAuto(style, size);

    // padding - not honored by WinIE, needs to be removed.
    style->resetPadding();

    // border - honored by WinIE, but looks terrible (just paints in the control box and turns off the Windows XP theme)
    // for now, we will not honor it.
    style->resetBorder();
}

} // namespace WebCore
