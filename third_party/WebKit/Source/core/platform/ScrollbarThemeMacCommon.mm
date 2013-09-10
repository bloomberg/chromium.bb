/*
 * Copyright (C) 2008, 2011 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "core/platform/ScrollbarThemeMacCommon.h"

#include <Carbon/Carbon.h>
#include "core/page/FrameView.h"
#include "core/platform/PlatformMouseEvent.h"
#include "core/platform/ScrollView.h"
#include "core/platform/ScrollbarThemeMacNonOverlayAPI.h"
#include "core/platform/ScrollbarThemeMacOverlayAPI.h"
#include "core/platform/graphics/Gradient.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/GraphicsContextStateSaver.h"
#include "core/platform/graphics/GraphicsLayer.h"
#include "core/platform/graphics/ImageBuffer.h"
#include "core/platform/graphics/mac/ColorMac.h"
#include "core/platform/mac/LocalCurrentGraphicsContext.h"
#include "core/platform/mac/NSScrollerImpDetails.h"
#include "core/platform/mac/ScrollAnimatorMac.h"
#include "public/platform/mac/WebThemeEngine.h"
#include "public/platform/Platform.h"
#include "public/platform/WebRect.h"
#include "skia/ext/skia_utils_mac.h"
#include "wtf/HashSet.h"
#include "wtf/StdLibExtras.h"
#include "wtf/TemporaryChange.h"
#include "wtf/UnusedParam.h"

// FIXME: There are repainting problems due to Aqua scroll bar buttons' visual overflow.

using namespace std;
using namespace WebCore;

@interface NSColor (WebNSColorDetails)
+ (NSImage *)_linenPatternImage;
@end

namespace WebCore {

typedef HashSet<ScrollbarThemeClient*> ScrollbarSet;

static ScrollbarSet& scrollbarSet()
{
    DEFINE_STATIC_LOCAL(ScrollbarSet, set, ());
    return set;
}

}

@interface WebScrollbarPrefsObserver : NSObject
{
}

+ (void)registerAsObserver;
+ (void)appearancePrefsChanged:(NSNotification*)theNotification;
+ (void)behaviorPrefsChanged:(NSNotification*)theNotification;

@end

@implementation WebScrollbarPrefsObserver

+ (void)appearancePrefsChanged:(NSNotification*)unusedNotification
{
    UNUSED_PARAM(unusedNotification);

    ScrollbarTheme* theme = ScrollbarTheme::theme();
    if (theme->isMockTheme())
        return;

    static_cast<ScrollbarThemeMacCommon*>(ScrollbarTheme::theme())->preferencesChanged();
    if (scrollbarSet().isEmpty())
        return;
    ScrollbarSet::iterator end = scrollbarSet().end();
    for (ScrollbarSet::iterator it = scrollbarSet().begin(); it != end; ++it) {
        (*it)->styleChanged();
        (*it)->invalidate();
    }
}

+ (void)behaviorPrefsChanged:(NSNotification*)unusedNotification
{
    UNUSED_PARAM(unusedNotification);

    ScrollbarTheme* theme = ScrollbarTheme::theme();
    if (theme->isMockTheme())
        return;

    static_cast<ScrollbarThemeMacCommon*>(ScrollbarTheme::theme())->preferencesChanged();
}

+ (void)registerAsObserver
{
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(appearancePrefsChanged:) name:@"AppleAquaScrollBarVariantChanged" object:nil suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(behaviorPrefsChanged:) name:@"AppleNoRedisplayAppearancePreferenceChanged" object:nil suspensionBehavior:NSNotificationSuspensionBehaviorCoalesce];
}

@end

namespace WebCore {

static float gInitialButtonDelay = 0.5f;
static float gAutoscrollButtonDelay = 0.05f;
static bool gJumpOnTrackClick = false;

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    if (isScrollbarOverlayAPIAvailable()) {
        DEFINE_STATIC_LOCAL(ScrollbarThemeMacOverlayAPI, theme, ());
        return &theme;
    } else {
        DEFINE_STATIC_LOCAL(ScrollbarThemeMacNonOverlayAPI, theme, ());
        return &theme;
    }
    return NULL;
}

void ScrollbarThemeMacCommon::registerScrollbar(ScrollbarThemeClient* scrollbar)
{
    scrollbarSet().add(scrollbar);
}

void ScrollbarThemeMacCommon::unregisterScrollbar(ScrollbarThemeClient* scrollbar)
{
    scrollbarSet().remove(scrollbar);
}

void ScrollbarThemeMacCommon::paintGivenTickmarks(GraphicsContext* context, ScrollbarThemeClient* scrollbar, const IntRect& rect, const Vector<IntRect>& tickmarks)
{
    if (scrollbar->orientation() != VerticalScrollbar)
        return;

    if (rect.height() <= 0 || rect.width() <= 0)
        return;  // nothing to draw on.

    if (!tickmarks.size())
        return;

    GraphicsContextStateSaver stateSaver(*context);
    context->setShouldAntialias(false);
    context->setStrokeColor(Color(0xCC, 0xAA, 0x00, 0xFF));
    context->setFillColor(Color(0xFF, 0xDD, 0x00, 0xFF));

    for (Vector<IntRect>::const_iterator i = tickmarks.begin(); i != tickmarks.end(); ++i) {
        // Calculate how far down (in %) the tick-mark should appear.
        const float percent = static_cast<float>(i->y()) / scrollbar->totalSize();
        if (percent < 0.0 || percent > 1.0)
            continue;

        // Calculate how far down (in pixels) the tick-mark should appear.
        const int yPos = rect.y() + (rect.height() * percent);

        // Paint.
        FloatRect tickRect(rect.x(), yPos, rect.width(), 2);
        context->fillRect(tickRect);
        context->strokeRect(tickRect, 1);
    }
}

void ScrollbarThemeMacCommon::paintOverhangBackground(ScrollView* view, GraphicsContext* context, const IntRect& horizontalOverhangRect, const IntRect& verticalOverhangRect, const IntRect& dirtyRect)
{
    const bool hasHorizontalOverhang = !horizontalOverhangRect.isEmpty();
    const bool hasVerticalOverhang = !verticalOverhangRect.isEmpty();

    GraphicsContextStateSaver stateSaver(*context);
    context->setFillPattern(m_overhangPattern);
    if (hasHorizontalOverhang)
        context->fillRect(intersection(horizontalOverhangRect, dirtyRect));
    if (hasVerticalOverhang)
        context->fillRect(intersection(verticalOverhangRect, dirtyRect));
}

void ScrollbarThemeMacCommon::paintOverhangShadows(ScrollView* view, GraphicsContext* context, const IntRect& horizontalOverhangRect, const IntRect& verticalOverhangRect, const IntRect& dirtyRect)
{
    // The extent of each shadow in pixels.
    const int kShadowSize = 4;
    // Offset of negative one pixel to make the gradient blend with the toolbar's bottom border.
    const int kToolbarShadowOffset = -1;
    const struct {
        float stop;
        Color color;
    } kShadowColors[] = {
        { 0.000, Color(0, 0, 0, 255) },
        { 0.125, Color(0, 0, 0, 57) },
        { 0.375, Color(0, 0, 0, 41) },
        { 0.625, Color(0, 0, 0, 18) },
        { 0.875, Color(0, 0, 0, 6) },
        { 1.000, Color(0, 0, 0, 0) }
    };
    const unsigned kNumShadowColors = sizeof(kShadowColors)/sizeof(kShadowColors[0]);

    const bool hasHorizontalOverhang = !horizontalOverhangRect.isEmpty();
    const bool hasVerticalOverhang = !verticalOverhangRect.isEmpty();
    // Prefer non-additive shadows, but degrade to additive shadows if there is vertical overhang.
    const bool useAdditiveShadows = hasVerticalOverhang;

    GraphicsContextStateSaver stateSaver(*context);

    IntSize scrollOffset = view->scrollOffset();
    FloatPoint shadowCornerOrigin;
    FloatPoint shadowCornerOffset;

    // Draw the shadow for the horizontal overhang.
    if (hasHorizontalOverhang) {
        int toolbarShadowHeight = kShadowSize;
        RefPtr<Gradient> gradient;
        IntRect shadowRect = horizontalOverhangRect;
        shadowRect.setHeight(kShadowSize);
        if (scrollOffset.height() < 0) {
            if (useAdditiveShadows) {
                toolbarShadowHeight = std::min(horizontalOverhangRect.height(), kShadowSize);
            } else if (horizontalOverhangRect.height() < 2 * kShadowSize + kToolbarShadowOffset) {
                // Split the overhang area between the web content shadow and toolbar shadow if it's too small.
                shadowRect.setHeight((horizontalOverhangRect.height() + 1) / 2);
                toolbarShadowHeight = horizontalOverhangRect.height() - shadowRect.height() - kToolbarShadowOffset;
            }
            shadowRect.setY(horizontalOverhangRect.maxY() - shadowRect.height());
            gradient = Gradient::create(FloatPoint(0, shadowRect.maxY()), FloatPoint(0, shadowRect.maxY() - kShadowSize));
            shadowCornerOrigin.setY(shadowRect.maxY());
            shadowCornerOffset.setY(-kShadowSize);
        } else {
            gradient = Gradient::create(FloatPoint(0, shadowRect.y()), FloatPoint(0, shadowRect.maxY()));
            shadowCornerOrigin.setY(shadowRect.y());
        }
        if (hasVerticalOverhang) {
            shadowRect.setWidth(shadowRect.width() - verticalOverhangRect.width());
            if (scrollOffset.width() < 0) {
                shadowRect.setX(shadowRect.x() + verticalOverhangRect.width());
                shadowCornerOrigin.setX(shadowRect.x());
                shadowCornerOffset.setX(-kShadowSize);
            } else {
                shadowCornerOrigin.setX(shadowRect.maxX());
            }
        }
        for (unsigned i = 0; i < kNumShadowColors; i++)
            gradient->addColorStop(kShadowColors[i].stop, kShadowColors[i].color);
        context->setFillGradient(gradient);
        context->fillRect(intersection(shadowRect, dirtyRect));

        // Draw a drop-shadow from the toolbar.
        if (scrollOffset.height() < 0) {
            shadowRect.setY(kToolbarShadowOffset);
            shadowRect.setHeight(toolbarShadowHeight);
            gradient = Gradient::create(FloatPoint(0, shadowRect.y()), FloatPoint(0, shadowRect.y() + kShadowSize));
            for (unsigned i = 0; i < kNumShadowColors; i++)
                gradient->addColorStop(kShadowColors[i].stop, kShadowColors[i].color);
            context->setFillGradient(gradient);
            context->fillRect(intersection(shadowRect, dirtyRect));
        }
    }

    // Draw the shadow for the vertical overhang.
    if (hasVerticalOverhang) {
        RefPtr<Gradient> gradient;
        IntRect shadowRect = verticalOverhangRect;
        shadowRect.setWidth(kShadowSize);
        if (scrollOffset.width() < 0) {
            shadowRect.setX(verticalOverhangRect.maxX() - shadowRect.width());
            gradient = Gradient::create(FloatPoint(shadowRect.maxX(), 0), FloatPoint(shadowRect.x(), 0));
        } else {
            gradient = Gradient::create(FloatPoint(shadowRect.x(), 0), FloatPoint(shadowRect.maxX(), 0));
        }
        for (unsigned i = 0; i < kNumShadowColors; i++)
            gradient->addColorStop(kShadowColors[i].stop, kShadowColors[i].color);
        context->setFillGradient(gradient);
        context->fillRect(intersection(shadowRect, dirtyRect));

        // Draw a drop-shadow from the toolbar.
        shadowRect = verticalOverhangRect;
        shadowRect.setY(kToolbarShadowOffset);
        shadowRect.setHeight(kShadowSize);
        gradient = Gradient::create(FloatPoint(0, shadowRect.y()), FloatPoint(0, shadowRect.maxY()));
        for (unsigned i = 0; i < kNumShadowColors; i++)
            gradient->addColorStop(kShadowColors[i].stop, kShadowColors[i].color);
        context->setFillGradient(gradient);
        context->fillRect(intersection(shadowRect, dirtyRect));
    }

    // If both rectangles present, draw a radial gradient for the corner.
    if (hasHorizontalOverhang && hasVerticalOverhang) {
        RefPtr<Gradient> gradient = Gradient::create(shadowCornerOrigin, 0, shadowCornerOrigin, kShadowSize);
        for (unsigned i = 0; i < kNumShadowColors; i++)
            gradient->addColorStop(kShadowColors[i].stop, kShadowColors[i].color);
        context->setFillGradient(gradient);
        context->fillRect(FloatRect(shadowCornerOrigin.x() + shadowCornerOffset.x(), shadowCornerOrigin.y() + shadowCornerOffset.y(), kShadowSize, kShadowSize));
    }
}

void ScrollbarThemeMacCommon::paintTickmarks(GraphicsContext* context, ScrollbarThemeClient* scrollbar, const IntRect& rect)
{
    // Note: This is only used for css-styled scrollbars on mac.
    if (scrollbar->orientation() != VerticalScrollbar)
        return;

    if (rect.height() <= 0 || rect.width() <= 0)
        return;

    Vector<IntRect> tickmarks;
    scrollbar->getTickmarks(tickmarks);
    if (!tickmarks.size())
        return;

    // Inset a bit.
    IntRect tickmarkTrackRect = rect;
    tickmarkTrackRect.setX(tickmarkTrackRect.x() + 1);
    tickmarkTrackRect.setWidth(tickmarkTrackRect.width() - 2);
    paintGivenTickmarks(context, scrollbar, tickmarkTrackRect, tickmarks);
}

ScrollbarThemeMacCommon::ScrollbarThemeMacCommon()
{
    static bool initialized;
    if (!initialized) {
        initialized = true;
        [WebScrollbarPrefsObserver registerAsObserver];
        preferencesChanged();
    }

    // Load the shadow for the overhang.
    m_overhangShadow = Image::loadPlatformResource("overhangShadow");

    // Load the linen pattern image used for overhang drawing.
    RefPtr<Image> patternImage = Image::loadPlatformResource("overhangPattern");
    m_overhangPattern = Pattern::create(patternImage, true, true);
}

ScrollbarThemeMacCommon::~ScrollbarThemeMacCommon()
{
}

void ScrollbarThemeMacCommon::setUpOverhangShadowLayer(GraphicsLayer* overhangShadowLayer)
{
  // The shadow texture is has a 1-pixel aperture in the center, so the division by
  // two is doing an intentional round-down.
  overhangShadowLayer->setContentsToNinePatch(
      m_overhangShadow.get(),
      IntRect(m_overhangShadow->width() / 2, m_overhangShadow->height() / 2, 1, 1));
}

void ScrollbarThemeMacCommon::updateOverhangShadowLayer(GraphicsLayer* shadowLayer, GraphicsLayer* rootContentLayer)
{
  // Note that for the position, the division m_overhangShadow->width() / 2 is an intentional
  // round-down, and that for the width and height, the 1-pixel aperture is being replaced
  // by the root contents layer, hence subtracting 1 and adding the rootContentsLayer size.
  IntRect shadowRect (
    static_cast<int>(rootContentLayer->position().x()) - m_overhangShadow->width() / 2,
    static_cast<int>(rootContentLayer->position().y()) -  m_overhangShadow->height() / 2,
    static_cast<int>(rootContentLayer->size().width()) + m_overhangShadow->width() - 1,
    static_cast<int>(rootContentLayer->size().height()) + m_overhangShadow->height() - 1);
  shadowLayer->setContentsRect(shadowRect);
}

void ScrollbarThemeMacCommon::preferencesChanged()
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults synchronize];
    updateButtonPlacement();
    gInitialButtonDelay = [defaults floatForKey:@"NSScrollerButtonDelay"];
    gAutoscrollButtonDelay = [defaults floatForKey:@"NSScrollerButtonPeriod"];
    gJumpOnTrackClick = [defaults boolForKey:@"AppleScrollerPagingBehavior"];
}

double ScrollbarThemeMacCommon::initialAutoscrollTimerDelay()
{
    return gInitialButtonDelay;
}

double ScrollbarThemeMacCommon::autoscrollTimerDelay()
{
    return gAutoscrollButtonDelay;
}

bool ScrollbarThemeMacCommon::shouldCenterOnThumb(ScrollbarThemeClient*, const PlatformMouseEvent& evt)
{
    if (evt.button() != LeftButton)
        return false;
    if (gJumpOnTrackClick)
        return !evt.altKey();
    return evt.altKey();
}

bool ScrollbarThemeMacCommon::shouldDragDocumentInsteadOfThumb(ScrollbarThemeClient*, const PlatformMouseEvent& event)
{
    return event.altKey();
}

int ScrollbarThemeMacCommon::scrollbarPartToHIPressedState(ScrollbarPart part)
{
    switch (part) {
        case BackButtonStartPart:
            return kThemeTopOutsideArrowPressed;
        case BackButtonEndPart:
            return kThemeTopOutsideArrowPressed; // This does not make much sense.  For some reason the outside constant is required.
        case ForwardButtonStartPart:
            return kThemeTopInsideArrowPressed;
        case ForwardButtonEndPart:
            return kThemeBottomOutsideArrowPressed;
        case ThumbPart:
            return kThemeThumbPressed;
        default:
            return 0;
    }
}

} // namespace WebCore
