/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008, 2009 Google Inc.
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

#include "config.h"
#include "core/layout/LayoutThemeDefault.h"

#include "core/CSSValueKeywords.h"
#include "core/layout/LayoutMediaControls.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutProgress.h"
#include "core/layout/LayoutThemeFontProvider.h"
#include "core/layout/PaintInfo.h"
#include "platform/LayoutTestSupport.h"
#include "platform/PlatformResourceLoader.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/Platform.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebThemeEngine.h"
#include "wtf/StdLibExtras.h"

namespace blink {

enum PaddingType {
    TopPadding,
    RightPadding,
    BottomPadding,
    LeftPadding
};

static const int styledMenuListInternalPadding[4] = { 1, 4, 1, 4 };

// These values all match Safari/Win.
static const float defaultControlFontPixelSize = 13;
static const float defaultCancelButtonSize = 9;
static const float minCancelButtonSize = 5;
static const float maxCancelButtonSize = 21;
static const float defaultSearchFieldResultsDecorationSize = 13;
static const float minSearchFieldResultsDecorationSize = 9;
static const float maxSearchFieldResultsDecorationSize = 30;

static bool useMockTheme()
{
    return LayoutTestSupport::isRunningLayoutTest();
}

unsigned LayoutThemeDefault::m_activeSelectionBackgroundColor = 0xff1e90ff;
unsigned LayoutThemeDefault::m_activeSelectionForegroundColor = Color::black;
unsigned LayoutThemeDefault::m_inactiveSelectionBackgroundColor = 0xffc8c8c8;
unsigned LayoutThemeDefault::m_inactiveSelectionForegroundColor = 0xff323232;

double LayoutThemeDefault::m_caretBlinkInterval;

static const unsigned defaultButtonBackgroundColor = 0xffdddddd;

static WebThemeEngine::State getWebThemeState(const LayoutTheme* theme, const LayoutObject* o)
{
    if (!theme->isEnabled(o))
        return WebThemeEngine::StateDisabled;
    if (useMockTheme() && theme->isReadOnlyControl(o))
        return WebThemeEngine::StateReadonly;
    if (theme->isPressed(o))
        return WebThemeEngine::StatePressed;
    if (useMockTheme() && theme->isFocused(o))
        return WebThemeEngine::StateFocused;
    if (theme->isHovered(o))
        return WebThemeEngine::StateHover;

    return WebThemeEngine::StateNormal;
}

LayoutThemeDefault::LayoutThemeDefault()
{
    m_caretBlinkInterval = LayoutTheme::caretBlinkInterval();
}

LayoutThemeDefault::~LayoutThemeDefault()
{
}

bool LayoutThemeDefault::supportsFocusRing(const LayoutStyle& style) const
{
    if (useMockTheme()) {
        // Don't use focus rings for buttons when mocking controls.
        return style.appearance() == ButtonPart
            || style.appearance() == PushButtonPart
            || style.appearance() == SquareButtonPart;
    }

    // This causes Blink to draw the focus rings for us.
    return false;
}

Color LayoutThemeDefault::systemColor(CSSValueID cssValueId) const
{
    static const Color defaultButtonGrayColor(0xffdddddd);
    static const Color defaultMenuColor(0xfff7f7f7);

    if (cssValueId == CSSValueButtonface) {
        if (useMockTheme())
            return Color(0xc0, 0xc0, 0xc0);
        return defaultButtonGrayColor;
    }
    if (cssValueId == CSSValueMenu)
        return defaultMenuColor;
    return LayoutTheme::systemColor(cssValueId);
}

// Use the Windows style sheets to match their metrics.
String LayoutThemeDefault::extraDefaultStyleSheet()
{
    String legacyOptionStyle;
    if (!RuntimeEnabledFeatures::htmlPopupMenuEnabled()) {
        // Option font must be inherited because we depend on computing the size
        // of the <select> based on the size of the options, and they must use
        // the same font for that computation to be correct.
        legacyOptionStyle = "option { font: inherit !important; }";
    }
    return LayoutTheme::extraDefaultStyleSheet()
        + loadResourceAsASCIIString("themeWin.css")
        + loadResourceAsASCIIString("themeChromiumSkia.css")
        + loadResourceAsASCIIString("themeChromium.css")
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
        + loadResourceAsASCIIString("themeInputMultipleFields.css")
#endif
        + legacyOptionStyle;
}

String LayoutThemeDefault::extraQuirksStyleSheet()
{
    return loadResourceAsASCIIString("themeWinQuirks.css");
}

Color LayoutThemeDefault::activeListBoxSelectionBackgroundColor() const
{
    return Color(0x28, 0x28, 0x28);
}

Color LayoutThemeDefault::activeListBoxSelectionForegroundColor() const
{
    return Color::black;
}

Color LayoutThemeDefault::inactiveListBoxSelectionBackgroundColor() const
{
    return Color(0xc8, 0xc8, 0xc8);
}

Color LayoutThemeDefault::inactiveListBoxSelectionForegroundColor() const
{
    return Color(0x32, 0x32, 0x32);
}

Color LayoutThemeDefault::platformActiveSelectionBackgroundColor() const
{
    if (useMockTheme())
        return Color(0x00, 0x00, 0xff); // Royal blue.
    return m_activeSelectionBackgroundColor;
}

Color LayoutThemeDefault::platformInactiveSelectionBackgroundColor() const
{
    if (useMockTheme())
        return Color(0x99, 0x99, 0x99); // Medium gray.
    return m_inactiveSelectionBackgroundColor;
}

Color LayoutThemeDefault::platformActiveSelectionForegroundColor() const
{
    if (useMockTheme())
        return Color(0xff, 0xff, 0xcc); // Pale yellow.
    return m_activeSelectionForegroundColor;
}

Color LayoutThemeDefault::platformInactiveSelectionForegroundColor() const
{
    if (useMockTheme())
        return Color::white;
    return m_inactiveSelectionForegroundColor;
}

IntSize LayoutThemeDefault::sliderTickSize() const
{
    if (useMockTheme())
        return IntSize(1, 3);
    return IntSize(1, 6);
}

int LayoutThemeDefault::sliderTickOffsetFromTrackCenter() const
{
    if (useMockTheme())
        return 11;
    return -16;
}

void LayoutThemeDefault::adjustSliderThumbSize(LayoutStyle& style, Element* element) const
{
    IntSize size = Platform::current()->themeEngine()->getSize(WebThemeEngine::PartSliderThumb);

    // FIXME: Mock theme doesn't handle zoomed sliders.
    float zoomLevel = useMockTheme() ? 1 : style.effectiveZoom();
    if (style.appearance() == SliderThumbHorizontalPart) {
        style.setWidth(Length(size.width() * zoomLevel, Fixed));
        style.setHeight(Length(size.height() * zoomLevel, Fixed));
    } else if (style.appearance() == SliderThumbVerticalPart) {
        style.setWidth(Length(size.height() * zoomLevel, Fixed));
        style.setHeight(Length(size.width() * zoomLevel, Fixed));
    } else {
        LayoutMediaControls::adjustMediaSliderThumbSize(style);
    }
}

void LayoutThemeDefault::setCaretBlinkInterval(double interval)
{
    m_caretBlinkInterval = interval;
}

void LayoutThemeDefault::setSelectionColors(
    unsigned activeBackgroundColor,
    unsigned activeForegroundColor,
    unsigned inactiveBackgroundColor,
    unsigned inactiveForegroundColor)
{
    m_activeSelectionBackgroundColor = activeBackgroundColor;
    m_activeSelectionForegroundColor = activeForegroundColor;
    m_inactiveSelectionBackgroundColor = inactiveBackgroundColor;
    m_inactiveSelectionForegroundColor = inactiveForegroundColor;
}

bool LayoutThemeDefault::paintCheckbox(LayoutObject* o, const PaintInfo& i, const IntRect& rect)
{
    WebThemeEngine::ExtraParams extraParams;
    WebCanvas* canvas = i.context->canvas();
    extraParams.button.checked = isChecked(o);
    extraParams.button.indeterminate = isIndeterminate(o);

    float zoomLevel = o->style()->effectiveZoom();
    GraphicsContextStateSaver stateSaver(*i.context, false);
    IntRect unzoomedRect = rect;
    if (zoomLevel != 1) {
        stateSaver.save();
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        i.context->translate(unzoomedRect.x(), unzoomedRect.y());
        i.context->scale(zoomLevel, zoomLevel);
        i.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    Platform::current()->themeEngine()->paint(canvas, WebThemeEngine::PartCheckbox, getWebThemeState(this, o), WebRect(unzoomedRect), &extraParams);
    return false;
}

void LayoutThemeDefault::setCheckboxSize(LayoutStyle& style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style.width().isIntrinsicOrAuto() && !style.height().isAuto())
        return;

    IntSize size = Platform::current()->themeEngine()->getSize(WebThemeEngine::PartCheckbox);
    float zoomLevel = style.effectiveZoom();
    size.setWidth(size.width() * zoomLevel);
    size.setHeight(size.height() * zoomLevel);
    setSizeIfAuto(style, size);
}

bool LayoutThemeDefault::paintRadio(LayoutObject* o, const PaintInfo& i, const IntRect& rect)
{
    WebThemeEngine::ExtraParams extraParams;
    WebCanvas* canvas = i.context->canvas();
    extraParams.button.checked = isChecked(o);

    Platform::current()->themeEngine()->paint(canvas, WebThemeEngine::PartRadio, getWebThemeState(this, o), WebRect(rect), &extraParams);
    return false;
}

void LayoutThemeDefault::setRadioSize(LayoutStyle& style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style.width().isIntrinsicOrAuto() && !style.height().isAuto())
        return;

    IntSize size = Platform::current()->themeEngine()->getSize(WebThemeEngine::PartRadio);
    float zoomLevel = style.effectiveZoom();
    size.setWidth(size.width() * zoomLevel);
    size.setHeight(size.height() * zoomLevel);
    setSizeIfAuto(style, size);
}

bool LayoutThemeDefault::paintButton(LayoutObject* o, const PaintInfo& i, const IntRect& rect)
{
    WebThemeEngine::ExtraParams extraParams;
    WebCanvas* canvas = i.context->canvas();
    extraParams.button.hasBorder = true;
    extraParams.button.backgroundColor = useMockTheme() ? 0xffc0c0c0 : defaultButtonBackgroundColor;
    if (o->hasBackground())
        extraParams.button.backgroundColor = o->resolveColor(CSSPropertyBackgroundColor).rgb();

    Platform::current()->themeEngine()->paint(canvas, WebThemeEngine::PartButton, getWebThemeState(this, o), WebRect(rect), &extraParams);
    return false;
}

bool LayoutThemeDefault::paintTextField(LayoutObject* o, const PaintInfo& i, const IntRect& rect)
{
    // WebThemeEngine does not handle border rounded corner and background image
    // so return true to draw CSS border and background.
    if (o->style()->hasBorderRadius() || o->style()->hasBackgroundImage())
        return true;

    ControlPart part = o->style()->appearance();

    WebThemeEngine::ExtraParams extraParams;
    extraParams.textField.isTextArea = part == TextAreaPart;
    extraParams.textField.isListbox = part == ListboxPart;

    WebCanvas* canvas = i.context->canvas();

    Color backgroundColor = o->resolveColor(CSSPropertyBackgroundColor);
    extraParams.textField.backgroundColor = backgroundColor.rgb();

    Platform::current()->themeEngine()->paint(canvas, WebThemeEngine::PartTextField, getWebThemeState(this, o), WebRect(rect), &extraParams);
    return false;
}

bool LayoutThemeDefault::paintMenuList(LayoutObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (!o->isBox())
        return false;

    const int right = rect.x() + rect.width();
    const int middle = rect.y() + rect.height() / 2;

    WebThemeEngine::ExtraParams extraParams;
    extraParams.menuList.arrowY = middle;
    const LayoutBox* box = toLayoutBox(o);
    // Match Chromium Win behaviour of showing all borders if any are shown.
    extraParams.menuList.hasBorder = box->borderRight() || box->borderLeft() || box->borderTop() || box->borderBottom();
    extraParams.menuList.hasBorderRadius = o->style()->hasBorderRadius();
    // Fallback to transparent if the specified color object is invalid.
    Color backgroundColor(Color::transparent);
    if (o->hasBackground())
        backgroundColor = o->resolveColor(CSSPropertyBackgroundColor);
    extraParams.menuList.backgroundColor = backgroundColor.rgb();

    // If we have a background image, don't fill the content area to expose the
    // parent's background. Also, we shouldn't fill the content area if the
    // alpha of the color is 0. The API of Windows GDI ignores the alpha.
    // FIXME: the normal Aura theme doesn't care about this, so we should
    // investigate if we really need fillContentArea.
    extraParams.menuList.fillContentArea = !o->style()->hasBackgroundImage() && backgroundColor.alpha();

    if (useMockTheme()) {
        // The size and position of the drop-down button is different between
        // the mock theme and the regular aura theme.
        int spacingTop = box->borderTop() + box->paddingTop();
        int spacingBottom = box->borderBottom() + box->paddingBottom();
        int spacingRight = box->borderRight() + box->paddingRight();
        extraParams.menuList.arrowX = (o->style()->direction() == RTL) ? rect.x() + 4 + spacingRight: right - 13 - spacingRight;
        extraParams.menuList.arrowHeight = rect.height() - spacingBottom - spacingTop;
    } else {
        extraParams.menuList.arrowX = (o->style()->direction() == RTL) ? rect.x() + 7 : right - 13;
    }

    WebCanvas* canvas = i.context->canvas();

    Platform::current()->themeEngine()->paint(canvas, WebThemeEngine::PartMenuList, getWebThemeState(this, o), WebRect(rect), &extraParams);
    return false;
}

bool LayoutThemeDefault::paintMenuListButton(LayoutObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (!o->isBox())
        return false;

    const int right = rect.x() + rect.width();
    const int middle = rect.y() + rect.height() / 2;

    WebThemeEngine::ExtraParams extraParams;
    extraParams.menuList.arrowY = middle;
    extraParams.menuList.hasBorder = false;
    extraParams.menuList.hasBorderRadius = o->style()->hasBorderRadius();
    extraParams.menuList.backgroundColor = Color::transparent;
    extraParams.menuList.fillContentArea = false;

    if (useMockTheme()) {
        const LayoutBox* box = toLayoutBox(o);
        // The size and position of the drop-down button is different between
        // the mock theme and the regular aura theme.
        int spacingTop = box->borderTop() + box->paddingTop();
        int spacingBottom = box->borderBottom() + box->paddingBottom();
        int spacingRight = box->borderRight() + box->paddingRight();
        extraParams.menuList.arrowX = (o->style()->direction() == RTL) ? rect.x() + 4 + spacingRight: right - 13 - spacingRight;
        extraParams.menuList.arrowHeight = rect.height() - spacingBottom - spacingTop;
    } else {
        extraParams.menuList.arrowX = (o->style()->direction() == RTL) ? rect.x() + 7 : right - 13;
    }

    WebCanvas* canvas = i.context->canvas();

    Platform::current()->themeEngine()->paint(canvas, WebThemeEngine::PartMenuList, getWebThemeState(this, o), WebRect(rect), &extraParams);
    return false;
}

bool LayoutThemeDefault::paintSliderTrack(LayoutObject* o, const PaintInfo& i, const IntRect& rect)
{
    WebThemeEngine::ExtraParams extraParams;
    WebCanvas* canvas = i.context->canvas();
    extraParams.slider.vertical = o->style()->appearance() == SliderVerticalPart;

    paintSliderTicks(o, i, rect);

    // FIXME: Mock theme doesn't handle zoomed sliders.
    float zoomLevel = useMockTheme() ? 1 : o->style()->effectiveZoom();
    GraphicsContextStateSaver stateSaver(*i.context, false);
    IntRect unzoomedRect = rect;
    if (zoomLevel != 1) {
        stateSaver.save();
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        i.context->translate(unzoomedRect.x(), unzoomedRect.y());
        i.context->scale(zoomLevel, zoomLevel);
        i.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    Platform::current()->themeEngine()->paint(canvas, WebThemeEngine::PartSliderTrack, getWebThemeState(this, o), WebRect(unzoomedRect), &extraParams);

    return false;
}

bool LayoutThemeDefault::paintSliderThumb(LayoutObject* o, const PaintInfo& i, const IntRect& rect)
{
    WebThemeEngine::ExtraParams extraParams;
    WebCanvas* canvas = i.context->canvas();
    extraParams.slider.vertical = o->style()->appearance() == SliderThumbVerticalPart;
    extraParams.slider.inDrag = isPressed(o);

    // FIXME: Mock theme doesn't handle zoomed sliders.
    float zoomLevel = useMockTheme() ? 1 : o->style()->effectiveZoom();
    GraphicsContextStateSaver stateSaver(*i.context, false);
    IntRect unzoomedRect = rect;
    if (zoomLevel != 1) {
        stateSaver.save();
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        i.context->translate(unzoomedRect.x(), unzoomedRect.y());
        i.context->scale(zoomLevel, zoomLevel);
        i.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    Platform::current()->themeEngine()->paint(canvas, WebThemeEngine::PartSliderThumb, getWebThemeState(this, o), WebRect(unzoomedRect), &extraParams);
    return false;
}

void LayoutThemeDefault::adjustInnerSpinButtonStyle(LayoutStyle& style, Element*) const
{
    IntSize size = Platform::current()->themeEngine()->getSize(WebThemeEngine::PartInnerSpinButton);

    style.setWidth(Length(size.width(), Fixed));
    style.setMinWidth(Length(size.width(), Fixed));
}

bool LayoutThemeDefault::paintInnerSpinButton(LayoutObject* o, const PaintInfo& i, const IntRect& rect)
{
    WebThemeEngine::ExtraParams extraParams;
    WebCanvas* canvas = i.context->canvas();
    extraParams.innerSpin.spinUp = (controlStatesForRenderer(o) & SpinUpControlState);
    extraParams.innerSpin.readOnly = isReadOnlyControl(o);

    Platform::current()->themeEngine()->paint(canvas, WebThemeEngine::PartInnerSpinButton, getWebThemeState(this, o), WebRect(rect), &extraParams);
    return false;
}

bool LayoutThemeDefault::paintProgressBar(LayoutObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (!o->isProgress())
        return true;

    LayoutProgress* renderProgress = toLayoutProgress(o);
    IntRect valueRect = progressValueRectFor(renderProgress, rect);

    WebThemeEngine::ExtraParams extraParams;
    extraParams.progressBar.determinate = renderProgress->isDeterminate();
    extraParams.progressBar.valueRectX = valueRect.x();
    extraParams.progressBar.valueRectY = valueRect.y();
    extraParams.progressBar.valueRectWidth = valueRect.width();
    extraParams.progressBar.valueRectHeight = valueRect.height();

    DirectionFlippingScope scope(o, i, rect);
    WebCanvas* canvas = i.context->canvas();
    Platform::current()->themeEngine()->paint(canvas, WebThemeEngine::PartProgressBar, getWebThemeState(this, o), WebRect(rect), &extraParams);
    return false;
}

bool LayoutThemeDefault::shouldOpenPickerWithF4Key() const
{
    return true;
}

bool LayoutThemeDefault::shouldUseFallbackTheme(const LayoutStyle& style) const
{
    if (useMockTheme()) {
        // The mock theme can't handle zoomed controls, so we fall back to the "fallback" theme.
        ControlPart part = style.appearance();
        if (part == CheckboxPart || part == RadioPart)
            return style.effectiveZoom() != 1;
    }
    return LayoutTheme::shouldUseFallbackTheme(style);
}

bool LayoutThemeDefault::supportsHover(const LayoutStyle& style) const
{
    return true;
}

Color LayoutThemeDefault::platformFocusRingColor() const
{
    static Color focusRingColor(229, 151, 0, 255);
    return focusRingColor;
}

double LayoutThemeDefault::caretBlinkInterval() const
{
    // Disable the blinking caret in layout test mode, as it introduces
    // a race condition for the pixel tests. http://b/1198440
    if (LayoutTestSupport::isRunningLayoutTest())
        return 0;

    return m_caretBlinkInterval;
}

void LayoutThemeDefault::systemFont(CSSValueID systemFontID, FontStyle& fontStyle, FontWeight& fontWeight, float& fontSize, AtomicString& fontFamily) const
{
    LayoutThemeFontProvider::systemFont(systemFontID, fontStyle, fontWeight, fontSize, fontFamily);
}

int LayoutThemeDefault::minimumMenuListSize(const LayoutStyle& style) const
{
    return 0;
}

// Return a rectangle that has the same center point as |original|, but with a
// size capped at |width| by |height|.
IntRect center(const IntRect& original, int width, int height)
{
    width = std::min(original.width(), width);
    height = std::min(original.height(), height);
    int x = original.x() + (original.width() - width) / 2;
    int y = original.y() + (original.height() - height) / 2;

    return IntRect(x, y, width, height);
}

void LayoutThemeDefault::adjustButtonStyle(LayoutStyle& style, Element*) const
{
    if (style.appearance() == PushButtonPart) {
        // Ignore line-height.
        style.setLineHeight(LayoutStyle::initialLineHeight());
    }
}

bool LayoutThemeDefault::paintTextArea(LayoutObject* o, const PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void LayoutThemeDefault::adjustSearchFieldStyle(LayoutStyle& style, Element*) const
{
    // Ignore line-height.
    style.setLineHeight(LayoutStyle::initialLineHeight());
}

bool LayoutThemeDefault::paintSearchField(LayoutObject* o, const PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void LayoutThemeDefault::adjustSearchFieldCancelButtonStyle(LayoutStyle& style, Element*) const
{
    // Scale the button size based on the font size
    float fontScale = style.fontSize() / defaultControlFontPixelSize;
    int cancelButtonSize = lroundf(std::min(std::max(minCancelButtonSize, defaultCancelButtonSize * fontScale), maxCancelButtonSize));
    style.setWidth(Length(cancelButtonSize, Fixed));
    style.setHeight(Length(cancelButtonSize, Fixed));
}

IntRect LayoutThemeDefault::convertToPaintingRect(LayoutObject* inputRenderer, const LayoutObject* partRenderer, LayoutRect partRect, const IntRect& localOffset) const
{
    // Compute an offset between the part renderer and the input renderer.
    LayoutSize offsetFromInputRenderer = -partRenderer->offsetFromAncestorContainer(inputRenderer);
    // Move the rect into partRenderer's coords.
    partRect.move(offsetFromInputRenderer);
    // Account for the local drawing offset.
    partRect.move(localOffset.x(), localOffset.y());

    return pixelSnappedIntRect(partRect);
}

bool LayoutThemeDefault::paintSearchFieldCancelButton(LayoutObject* cancelButtonObject, const PaintInfo& paintInfo, const IntRect& r)
{
    // Get the renderer of <input> element.
    if (!cancelButtonObject->node())
        return false;
    Node* input = cancelButtonObject->node()->shadowHost();
    LayoutObject* baseRenderer = input ? input->layoutObject() : cancelButtonObject;
    if (!baseRenderer->isBox())
        return false;
    LayoutBox* inputLayoutBox = toLayoutBox(baseRenderer);
    LayoutRect inputContentBox = inputLayoutBox->contentBoxRect();

    // Make sure the scaled button stays square and will fit in its parent's box.
    LayoutUnit cancelButtonSize = std::min(inputContentBox.width(), std::min<LayoutUnit>(inputContentBox.height(), r.height()));
    // Calculate cancel button's coordinates relative to the input element.
    // Center the button vertically.  Round up though, so if it has to be one pixel off-center, it will
    // be one pixel closer to the bottom of the field.  This tends to look better with the text.
    LayoutRect cancelButtonRect(cancelButtonObject->offsetFromAncestorContainer(inputLayoutBox).width(),
        inputContentBox.y() + (inputContentBox.height() - cancelButtonSize + 1) / 2,
        cancelButtonSize, cancelButtonSize);
    IntRect paintingRect = convertToPaintingRect(inputLayoutBox, cancelButtonObject, cancelButtonRect, r);

    DEFINE_STATIC_REF(Image, cancelImage, (Image::loadPlatformResource("searchCancel")));
    DEFINE_STATIC_REF(Image, cancelPressedImage, (Image::loadPlatformResource("searchCancelPressed")));
    paintInfo.context->drawImage(isPressed(cancelButtonObject) ? cancelPressedImage : cancelImage, paintingRect);
    return false;
}

void LayoutThemeDefault::adjustSearchFieldDecorationStyle(LayoutStyle& style, Element*) const
{
    IntSize emptySize(1, 11);
    style.setWidth(Length(emptySize.width(), Fixed));
    style.setHeight(Length(emptySize.height(), Fixed));
}

void LayoutThemeDefault::adjustSearchFieldResultsDecorationStyle(LayoutStyle& style, Element*) const
{
    // Scale the decoration size based on the font size
    float fontScale = style.fontSize() / defaultControlFontPixelSize;
    int magnifierSize = lroundf(std::min(std::max(minSearchFieldResultsDecorationSize, defaultSearchFieldResultsDecorationSize * fontScale),
        maxSearchFieldResultsDecorationSize));
    style.setWidth(Length(magnifierSize, Fixed));
    style.setHeight(Length(magnifierSize, Fixed));
}

bool LayoutThemeDefault::paintSearchFieldResultsDecoration(LayoutObject* magnifierObject, const PaintInfo& paintInfo, const IntRect& r)
{
    // Get the renderer of <input> element.
    if (!magnifierObject->node())
        return false;
    Node* input = magnifierObject->node()->shadowHost();
    LayoutObject* baseRenderer = input ? input->layoutObject() : magnifierObject;
    if (!baseRenderer->isBox())
        return false;
    LayoutBox* inputLayoutBox = toLayoutBox(baseRenderer);
    LayoutRect inputContentBox = inputLayoutBox->contentBoxRect();

    // Make sure the scaled decoration stays square and will fit in its parent's box.
    LayoutUnit magnifierSize = std::min(inputContentBox.width(), std::min<LayoutUnit>(inputContentBox.height(), r.height()));
    // Calculate decoration's coordinates relative to the input element.
    // Center the decoration vertically.  Round up though, so if it has to be one pixel off-center, it will
    // be one pixel closer to the bottom of the field.  This tends to look better with the text.
    LayoutRect magnifierRect(magnifierObject->offsetFromAncestorContainer(inputLayoutBox).width(),
        inputContentBox.y() + (inputContentBox.height() - magnifierSize + 1) / 2,
        magnifierSize, magnifierSize);
    IntRect paintingRect = convertToPaintingRect(inputLayoutBox, magnifierObject, magnifierRect, r);

    DEFINE_STATIC_REF(Image, magnifierImage, (Image::loadPlatformResource("searchMagnifier")));
    paintInfo.context->drawImage(magnifierImage, paintingRect);
    return false;
}

bool LayoutThemeDefault::paintMediaSliderTrack(LayoutObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return LayoutMediaControls::paintMediaControlsPart(MediaSlider, object, paintInfo, rect);
}

bool LayoutThemeDefault::paintMediaVolumeSliderTrack(LayoutObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return LayoutMediaControls::paintMediaControlsPart(MediaVolumeSlider, object, paintInfo, rect);
}

bool LayoutThemeDefault::paintMediaSliderThumb(LayoutObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return LayoutMediaControls::paintMediaControlsPart(MediaSliderThumb, object, paintInfo, rect);
}

bool LayoutThemeDefault::paintMediaToggleClosedCaptionsButton(LayoutObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    return LayoutMediaControls::paintMediaControlsPart(MediaShowClosedCaptionsButton, o, paintInfo, r);
}

bool LayoutThemeDefault::paintMediaCastButton(LayoutObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    return LayoutMediaControls::paintMediaControlsPart(MediaCastOffButton, o, paintInfo, r);
}

bool LayoutThemeDefault::paintMediaVolumeSliderThumb(LayoutObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return LayoutMediaControls::paintMediaControlsPart(MediaVolumeSliderThumb, object, paintInfo, rect);
}

bool LayoutThemeDefault::paintMediaPlayButton(LayoutObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return LayoutMediaControls::paintMediaControlsPart(MediaPlayButton, object, paintInfo, rect);
}

bool LayoutThemeDefault::paintMediaOverlayPlayButton(LayoutObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return LayoutMediaControls::paintMediaControlsPart(MediaOverlayPlayButton, object, paintInfo, rect);
}

bool LayoutThemeDefault::paintMediaMuteButton(LayoutObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return LayoutMediaControls::paintMediaControlsPart(MediaMuteButton, object, paintInfo, rect);
}

String LayoutThemeDefault::formatMediaControlsTime(float time) const
{
    return LayoutMediaControls::formatMediaControlsTime(time);
}

String LayoutThemeDefault::formatMediaControlsCurrentTime(float currentTime, float duration) const
{
    return LayoutMediaControls::formatMediaControlsCurrentTime(currentTime, duration);
}

bool LayoutThemeDefault::paintMediaFullscreenButton(LayoutObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return LayoutMediaControls::paintMediaControlsPart(MediaEnterFullscreenButton, object, paintInfo, rect);
}

void LayoutThemeDefault::adjustMenuListStyle(LayoutStyle& style, Element*) const
{
    // Height is locked to auto on all browsers.
    style.setLineHeight(LayoutStyle::initialLineHeight());
}

void LayoutThemeDefault::adjustMenuListButtonStyle(LayoutStyle& style, Element* e) const
{
    adjustMenuListStyle(style, e);
}

int LayoutThemeDefault::popupInternalPaddingLeft(const LayoutStyle& style) const
{
    return menuListInternalPadding(style, LeftPadding);
}

int LayoutThemeDefault::popupInternalPaddingRight(const LayoutStyle& style) const
{
    return menuListInternalPadding(style, RightPadding);
}

int LayoutThemeDefault::popupInternalPaddingTop(const LayoutStyle& style) const
{
    return menuListInternalPadding(style, TopPadding);
}

int LayoutThemeDefault::popupInternalPaddingBottom(const LayoutStyle& style) const
{
    return menuListInternalPadding(style, BottomPadding);
}

// static
void LayoutThemeDefault::setDefaultFontSize(int fontSize)
{
    LayoutThemeFontProvider::setDefaultFontSize(fontSize);
}

int LayoutThemeDefault::menuListArrowPadding() const
{
    return ScrollbarTheme::theme()->scrollbarThickness();
}

int LayoutThemeDefault::menuListInternalPadding(const LayoutStyle& style, int paddingType) const
{
    // This internal padding is in addition to the user-supplied padding.
    // Matches the FF behavior.
    int padding = styledMenuListInternalPadding[paddingType];

    // Reserve the space for right arrow here. The rest of the padding is
    // set by adjustMenuListStyle, since PopMenuWin.cpp uses the padding from
    // LayoutMenuList to lay out the individual items in the popup.
    // If the MenuList actually has appearance "NoAppearance", then that means
    // we don't draw a button, so don't reserve space for it.
    const int barType = style.direction() == LTR ? RightPadding : LeftPadding;
    if (paddingType == barType && style.appearance() != NoControlPart)
        padding += menuListArrowPadding();

    return padding;
}

bool LayoutThemeDefault::shouldShowPlaceholderWhenFocused() const
{
    return true;
}

//
// Following values are come from default of GTK+
//
static const int progressActivityBlocks = 5;
static const int progressAnimationFrames = 10;
static const double progressAnimationInterval = 0.125;

IntRect LayoutThemeDefault::determinateProgressValueRectFor(LayoutProgress* renderProgress, const IntRect& rect) const
{
    int dx = rect.width() * renderProgress->position();
    return IntRect(rect.x(), rect.y(), dx, rect.height());
}

IntRect LayoutThemeDefault::indeterminateProgressValueRectFor(LayoutProgress* renderProgress, const IntRect& rect) const
{

    int valueWidth = rect.width() / progressActivityBlocks;
    int movableWidth = rect.width() - valueWidth;
    if (movableWidth <= 0)
        return IntRect();

    double progress = renderProgress->animationProgress();
    if (progress < 0.5)
        return IntRect(rect.x() + progress * 2 * movableWidth, rect.y(), valueWidth, rect.height());
    return IntRect(rect.x() + (1.0 - progress) * 2 * movableWidth, rect.y(), valueWidth, rect.height());
}

double LayoutThemeDefault::animationRepeatIntervalForProgressBar() const
{
    return progressAnimationInterval;
}

double LayoutThemeDefault::animationDurationForProgressBar() const
{
    return progressAnimationInterval * progressAnimationFrames * 2; // "2" for back and forth
}

IntRect LayoutThemeDefault::progressValueRectFor(LayoutProgress* renderProgress, const IntRect& rect) const
{
    return renderProgress->isDeterminate() ? determinateProgressValueRectFor(renderProgress, rect) : indeterminateProgressValueRectFor(renderProgress, rect);
}

LayoutThemeDefault::DirectionFlippingScope::DirectionFlippingScope(LayoutObject* renderer, const PaintInfo& paintInfo, const IntRect& rect)
    : m_needsFlipping(!renderer->style()->isLeftToRightDirection())
    , m_paintInfo(paintInfo)
{
    if (!m_needsFlipping)
        return;
    m_paintInfo.context->save();
    m_paintInfo.context->translate(2 * rect.x() + rect.width(), 0);
    m_paintInfo.context->scale(-1, 1);
}

LayoutThemeDefault::DirectionFlippingScope::~DirectionFlippingScope()
{
    if (!m_needsFlipping)
        return;
    m_paintInfo.context->restore();
}

} // namespace blink
