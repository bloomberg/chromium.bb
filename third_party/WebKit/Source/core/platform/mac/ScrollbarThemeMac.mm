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
#include "core/platform/mac/ScrollbarThemeMac.h"

#include <Carbon/Carbon.h>
#include "core/page/FrameView.h"
#include "core/platform/PlatformMouseEvent.h"
#include "core/platform/ScrollView.h"
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
#include "wtf/HashMap.h"
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

typedef HashMap<ScrollbarThemeClient*, ScrollbarThemeClient*> ScrollbarMap;

static ScrollbarMap* scrollbarMap()
{
    static ScrollbarMap* map = new ScrollbarMap;
    return map;
}

typedef HashMap<ScrollbarThemeClient*, RetainPtr<ScrollbarPainter> > ScrollbarPainterMap;

static ScrollbarPainterMap* scrollbarPainterMap()
{
    static ScrollbarPainterMap* map = new ScrollbarPainterMap;
    return map;
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
    if (scrollbarMap()->isEmpty())
        return;
    ScrollbarMap::iterator end = scrollbarMap()->end();
    for (ScrollbarMap::iterator it = scrollbarMap()->begin(); it != end; ++it) {
        it->key->styleChanged();
        it->key->invalidate();
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

// FIXME: Get these numbers from CoreUI.
static int cRealButtonLength[] = { 28, 21 };
static int cButtonHitInset[] = { 3, 2 };
// cRealButtonLength - cButtonInset
static int cButtonLength[] = { 14, 10 };
static int cScrollbarThickness[] = { 15, 11 };
static int cButtonInset[] = { 14, 11 };
static int cThumbMinLength[] = { 26, 20 };

static int cOuterButtonLength[] = { 16, 14 }; // The outer button in a double button pair is a bit bigger.
static int cOuterButtonOverlap = 2;

static float gInitialButtonDelay = 0.5f;
static float gAutoscrollButtonDelay = 0.05f;
static bool gJumpOnTrackClick = false;

static ScrollbarButtonsPlacement gButtonPlacement = ScrollbarButtonsDoubleEnd;

static bool supportsExpandedScrollbars()
{
    // FIXME: This is temporary until all platforms that support ScrollbarPainter support this part of the API.
    static bool globalSupportsExpandedScrollbars = [NSClassFromString(@"NSScrollerImp") instancesRespondToSelector:@selector(setExpanded:)];
    return globalSupportsExpandedScrollbars;
}

void ScrollbarThemeMacNonOverlayAPI::updateButtonPlacement()
{
    NSString *buttonPlacement = [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleScrollBarVariant"];
    if ([buttonPlacement isEqualToString:@"Single"])
        gButtonPlacement = ScrollbarButtonsSingle;
    else if ([buttonPlacement isEqualToString:@"DoubleMin"])
        gButtonPlacement = ScrollbarButtonsDoubleStart;
    else if ([buttonPlacement isEqualToString:@"DoubleBoth"])
        gButtonPlacement = ScrollbarButtonsDoubleBoth;
    else {
        gButtonPlacement = ScrollbarButtonsDoubleEnd;
    }
}

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

static WebKit::WebThemeEngine::State scrollbarStateToThemeState(ScrollbarThemeClient* scrollbar)
{
    if (!scrollbar->enabled())
        return WebKit::WebThemeEngine::StateDisabled;
    if (!scrollbar->isScrollableAreaActive())
        return WebKit::WebThemeEngine::StateInactive;
    if (scrollbar->pressedPart() == ThumbPart)
        return WebKit::WebThemeEngine::StatePressed;

    return WebKit::WebThemeEngine::StateActive;
}

void ScrollbarThemeMacOverlayAPI::registerScrollbar(ScrollbarThemeClient* scrollbar)
{
    ScrollbarThemeMacCommon::registerScrollbar(scrollbar);

    bool isHorizontal = scrollbar->orientation() == HorizontalScrollbar;
    ScrollbarPainter scrollbarPainter = [NSClassFromString(@"NSScrollerImp") scrollerImpWithStyle:recommendedScrollerStyle() controlSize:(NSControlSize)scrollbar->controlSize() horizontal:isHorizontal replacingScrollerImp:nil];
    scrollbarPainterMap()->add(scrollbar, scrollbarPainter);
    updateEnabledState(scrollbar);
    updateScrollbarOverlayStyle(scrollbar);
}

void ScrollbarThemeMacCommon::registerScrollbar(ScrollbarThemeClient* scrollbar)
{
    scrollbarMap()->add(scrollbar, scrollbar);
}

void ScrollbarThemeMacOverlayAPI::unregisterScrollbar(ScrollbarThemeClient* scrollbar)
{
    scrollbarPainterMap()->remove(scrollbar);

    ScrollbarThemeMacCommon::unregisterScrollbar(scrollbar);
}

void ScrollbarThemeMacCommon::unregisterScrollbar(ScrollbarThemeClient* scrollbar)
{
    scrollbarMap()->remove(scrollbar);
}

void ScrollbarThemeMacOverlayAPI::setNewPainterForScrollbar(ScrollbarThemeClient* scrollbar, ScrollbarPainter newPainter)
{
    scrollbarPainterMap()->set(scrollbar, newPainter);
    updateEnabledState(scrollbar);
    updateScrollbarOverlayStyle(scrollbar);
}

ScrollbarPainter ScrollbarThemeMacOverlayAPI::painterForScrollbar(ScrollbarThemeClient* scrollbar)
{
    return scrollbarPainterMap()->get(scrollbar).get();
}

// Override ScrollbarThemeMacCommon::paint() to add support for the following:
//     - drawing using WebThemeEngine functions
//     - drawing tickmarks
//     - Skia specific changes
bool ScrollbarThemeMacNonOverlayAPI::paint(ScrollbarThemeClient* scrollbar, GraphicsContext* context, const IntRect& damageRect)
{
    // Get the tickmarks for the frameview.
    Vector<IntRect> tickmarks;
    scrollbar->getTickmarks(tickmarks);

    HIThemeTrackDrawInfo trackInfo;
    trackInfo.version = 0;
    trackInfo.kind = scrollbar->controlSize() == RegularScrollbar ? kThemeMediumScrollBar : kThemeSmallScrollBar;
    trackInfo.bounds = scrollbar->frameRect();
    trackInfo.min = 0;
    trackInfo.max = scrollbar->maximum();
    trackInfo.value = scrollbar->currentPos();
    trackInfo.trackInfo.scrollbar.viewsize = scrollbar->visibleSize();
    trackInfo.attributes = 0;
    if (scrollbar->orientation() == HorizontalScrollbar)
        trackInfo.attributes |= kThemeTrackHorizontal;

    if (!scrollbar->enabled())
        trackInfo.enableState = kThemeTrackDisabled;
    else
        trackInfo.enableState = scrollbar->isScrollableAreaActive() ? kThemeTrackActive : kThemeTrackInactive;

    if (!hasButtons(scrollbar))
        trackInfo.enableState = kThemeTrackNothingToScroll;
    trackInfo.trackInfo.scrollbar.pressState = scrollbarPartToHIPressedState(scrollbar->pressedPart());

    SkCanvas* canvas = context->canvas();
    CGAffineTransform currentCTM = gfx::SkMatrixToCGAffineTransform(canvas->getTotalMatrix());

    // The Aqua scrollbar is buggy when rotated and scaled.  We will just draw into a bitmap if we detect a scale or rotation.
    bool canDrawDirectly = currentCTM.a == 1.0f && currentCTM.b == 0.0f && currentCTM.c == 0.0f && (currentCTM.d == 1.0f || currentCTM.d == -1.0f);
    GraphicsContext* drawingContext = context;
    OwnPtr<ImageBuffer> imageBuffer;
    if (!canDrawDirectly) {
        trackInfo.bounds = IntRect(IntPoint(), scrollbar->frameRect().size());

        IntRect bufferRect(scrollbar->frameRect());
        bufferRect.intersect(damageRect);
        bufferRect.move(-scrollbar->frameRect().x(), -scrollbar->frameRect().y());

        imageBuffer = ImageBuffer::create(bufferRect.size());
        if (!imageBuffer)
            return true;

        drawingContext = imageBuffer->context();
    }

    // Draw thumbless.
    gfx::SkiaBitLocker bitLocker(drawingContext->canvas());
    CGContextRef cgContext = bitLocker.cgContext();
    HIThemeDrawTrack(&trackInfo, 0, cgContext, kHIThemeOrientationNormal);

    IntRect tickmarkTrackRect = trackRect(scrollbar, false);
    if (!canDrawDirectly) {
        tickmarkTrackRect.setX(0);
        tickmarkTrackRect.setY(0);
    }
    // The ends are rounded and the thumb doesn't go there.
    tickmarkTrackRect.inflateY(-tickmarkTrackRect.width());
    // Inset a bit.
    tickmarkTrackRect.setX(tickmarkTrackRect.x() + 2);
    tickmarkTrackRect.setWidth(tickmarkTrackRect.width() - 5);
    paintGivenTickmarks(drawingContext, scrollbar, tickmarkTrackRect, tickmarks);

    if (hasThumb(scrollbar)) {
        WebKit::WebThemeEngine::ScrollbarInfo scrollbarInfo;
        scrollbarInfo.orientation = scrollbar->orientation() == HorizontalScrollbar ? WebKit::WebThemeEngine::ScrollbarOrientationHorizontal : WebKit::WebThemeEngine::ScrollbarOrientationVertical;
        scrollbarInfo.parent = scrollbar->isScrollViewScrollbar() ? WebKit::WebThemeEngine::ScrollbarParentScrollView : WebKit::WebThemeEngine::ScrollbarParentRenderLayer;
        scrollbarInfo.maxValue = scrollbar->maximum();
        scrollbarInfo.currentValue = scrollbar->currentPos();
        scrollbarInfo.visibleSize = scrollbar->visibleSize();
        scrollbarInfo.totalSize = scrollbar->totalSize();

        WebKit::WebCanvas* webCanvas = drawingContext->canvas();
        WebKit::Platform::current()->themeEngine()->paintScrollbarThumb(
            webCanvas,
            scrollbarStateToThemeState(scrollbar),
            scrollbar->controlSize() == RegularScrollbar ? WebKit::WebThemeEngine::SizeRegular : WebKit::WebThemeEngine::SizeSmall,
            WebKit::WebRect(scrollbar->frameRect()),
            scrollbarInfo);
    }

    if (!canDrawDirectly)
        context->drawImageBuffer(imageBuffer.get(), scrollbar->frameRect().location());

    return true;
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

void ScrollbarThemeMacCommon::paintOverhangAreas(ScrollView* view, GraphicsContext* context, const IntRect& horizontalOverhangRect, const IntRect& verticalOverhangRect, const IntRect& dirtyRect)
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

    context->setFillPattern(m_overhangPattern);
    if (hasHorizontalOverhang)
        context->fillRect(intersection(horizontalOverhangRect, dirtyRect));
    if (hasVerticalOverhang)
        context->fillRect(intersection(verticalOverhangRect, dirtyRect));

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

void ScrollbarThemeMacOverlayAPI::paintTrackBackground(GraphicsContext* context, ScrollbarThemeClient* scrollbar, const IntRect& rect) {
    ASSERT(isScrollbarOverlayAPIAvailable());

    GraphicsContextStateSaver stateSaver(*context);
    context->translate(rect.x(), rect.y());
    LocalCurrentGraphicsContext localContext(context);

    CGRect frameRect = scrollbar->frameRect();
    ScrollbarPainter scrollbarPainter = painterForScrollbar(scrollbar);
    [scrollbarPainter setEnabled:scrollbar->enabled()];
    [scrollbarPainter setBoundsSize: NSSizeFromCGSize(frameRect.size)];

    NSRect trackRect = NSMakeRect(0, 0, frameRect.size.width, frameRect.size.height);
    [scrollbarPainter drawKnobSlotInRect:trackRect highlight:NO];
}

void ScrollbarThemeMacOverlayAPI::paintThumb(GraphicsContext* context, ScrollbarThemeClient* scrollbar, const IntRect& rect) {
    ASSERT(isScrollbarOverlayAPIAvailable());

    GraphicsContextStateSaver stateSaver(*context);
    context->translate(rect.x(), rect.y());
    LocalCurrentGraphicsContext localContext(context);

    ScrollbarPainter scrollbarPainter = painterForScrollbar(scrollbar);
    CGRect frameRect = scrollbar->frameRect();
    [scrollbarPainter setEnabled:scrollbar->enabled()];
    [scrollbarPainter setBoundsSize:NSSizeFromCGSize(rect.size())];
    [scrollbarPainter setDoubleValue:0];
    [scrollbarPainter setKnobProportion:1];
    if (scrollbar->enabled())
        [scrollbarPainter drawKnob];

    // If this state is not set, then moving the cursor over the scrollbar area will only cause the
    // scrollbar to engorge when moved over the top of the scrollbar area.
    [scrollbarPainter setBoundsSize: NSSizeFromCGSize(scrollbar->frameRect().size())];
}

ScrollbarThemeMacCommon::ScrollbarThemeMacCommon()
{
    static bool initialized;
    if (!initialized) {
        initialized = true;
        [WebScrollbarPrefsObserver registerAsObserver];
        preferencesChanged();
    }

    // Load the linen pattern image used for overhang drawing.
    RefPtr<Image> patternImage = Image::loadPlatformResource("overhangPattern");
    m_overhangPattern = Pattern::create(patternImage, true, true);
}

ScrollbarThemeMacCommon::~ScrollbarThemeMacCommon()
{
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

int ScrollbarThemeMacOverlayAPI::scrollbarThickness(ScrollbarControlSize controlSize)
{
    ScrollbarPainter scrollbarPainter = [NSClassFromString(@"NSScrollerImp") scrollerImpWithStyle:recommendedScrollerStyle() controlSize:controlSize horizontal:NO replacingScrollerImp:nil];
    if (supportsExpandedScrollbars())
        [scrollbarPainter setExpanded:YES];
    return [scrollbarPainter trackBoxWidth];
}

int ScrollbarThemeMacNonOverlayAPI::scrollbarThickness(ScrollbarControlSize controlSize)
{
    return cScrollbarThickness[controlSize];
}

bool ScrollbarThemeMacOverlayAPI::usesOverlayScrollbars() const
{
    return recommendedScrollerStyle() == NSScrollerStyleOverlay;
}

void ScrollbarThemeMacOverlayAPI::updateScrollbarOverlayStyle(ScrollbarThemeClient* scrollbar)
{
    ScrollbarPainter painter = painterForScrollbar(scrollbar);
    switch (scrollbar->scrollbarOverlayStyle()) {
    case ScrollbarOverlayStyleDefault:
        [painter setKnobStyle:NSScrollerKnobStyleDefault];
        break;
    case ScrollbarOverlayStyleDark:
        [painter setKnobStyle:NSScrollerKnobStyleDark];
        break;
    case ScrollbarOverlayStyleLight:
        [painter setKnobStyle:NSScrollerKnobStyleLight];
        break;
    }
}

double ScrollbarThemeMacCommon::initialAutoscrollTimerDelay()
{
    return gInitialButtonDelay;
}

double ScrollbarThemeMacCommon::autoscrollTimerDelay()
{
    return gAutoscrollButtonDelay;
}

ScrollbarButtonsPlacement ScrollbarThemeMacOverlayAPI::buttonsPlacement() const
{
    return ScrollbarButtonsNone;
}
    
ScrollbarButtonsPlacement ScrollbarThemeMacNonOverlayAPI::buttonsPlacement() const
{
    return gButtonPlacement;
}

bool ScrollbarThemeMacNonOverlayAPI::hasButtons(ScrollbarThemeClient* scrollbar)
{
    return scrollbar->enabled() && buttonsPlacement() != ScrollbarButtonsNone
             && (scrollbar->orientation() == HorizontalScrollbar
             ? scrollbar->width()
             : scrollbar->height()) >= 2 * (cRealButtonLength[scrollbar->controlSize()] - cButtonHitInset[scrollbar->controlSize()]);
}

bool ScrollbarThemeMacOverlayAPI::hasThumb(ScrollbarThemeClient* scrollbar)
{
    ScrollbarPainter painter = painterForScrollbar(scrollbar);
    int minLengthForThumb = [painter knobMinLength] + [painter trackOverlapEndInset] + [painter knobOverlapEndInset]
        + 2 * ([painter trackEndInset] + [painter knobEndInset]);
    return scrollbar->enabled() && (scrollbar->orientation() == HorizontalScrollbar ?
             scrollbar->width() :
             scrollbar->height()) >= minLengthForThumb;
}

bool ScrollbarThemeMacNonOverlayAPI::hasThumb(ScrollbarThemeClient* scrollbar)
{
    int minLengthForThumb = 2 * cButtonInset[scrollbar->controlSize()] + cThumbMinLength[scrollbar->controlSize()] + 1;
    return scrollbar->enabled() && (scrollbar->orientation() == HorizontalScrollbar ?
             scrollbar->width() :
             scrollbar->height()) >= minLengthForThumb;
}

static IntRect buttonRepaintRect(const IntRect& buttonRect, ScrollbarOrientation orientation, ScrollbarControlSize controlSize, bool start)
{
    ASSERT(gButtonPlacement != ScrollbarButtonsNone);

    IntRect paintRect(buttonRect);
    if (orientation == HorizontalScrollbar) {
        paintRect.setWidth(cRealButtonLength[controlSize]);
        if (!start)
            paintRect.setX(buttonRect.x() - (cRealButtonLength[controlSize] - buttonRect.width()));
    } else {
        paintRect.setHeight(cRealButtonLength[controlSize]);
        if (!start)
            paintRect.setY(buttonRect.y() - (cRealButtonLength[controlSize] - buttonRect.height()));
    }

    return paintRect;
}

IntRect ScrollbarThemeMacOverlayAPI::backButtonRect(ScrollbarThemeClient* scrollbar, ScrollbarPart part, bool painting)
{
    ASSERT(buttonsPlacement() == ScrollbarButtonsNone);
    return IntRect();
}

IntRect ScrollbarThemeMacNonOverlayAPI::backButtonRect(ScrollbarThemeClient* scrollbar, ScrollbarPart part, bool painting)
{
    IntRect result;
    
    if (part == BackButtonStartPart && (buttonsPlacement() == ScrollbarButtonsNone || buttonsPlacement() == ScrollbarButtonsDoubleEnd))
        return result;
    
    if (part == BackButtonEndPart && (buttonsPlacement() == ScrollbarButtonsNone || buttonsPlacement() == ScrollbarButtonsDoubleStart || buttonsPlacement() == ScrollbarButtonsSingle))
        return result;
        
    int thickness = scrollbarThickness(scrollbar->controlSize());
    bool outerButton = part == BackButtonStartPart && (buttonsPlacement() == ScrollbarButtonsDoubleStart || buttonsPlacement() == ScrollbarButtonsDoubleBoth);
    if (outerButton) {
        if (scrollbar->orientation() == HorizontalScrollbar)
            result = IntRect(scrollbar->x(), scrollbar->y(), cOuterButtonLength[scrollbar->controlSize()] + (painting ? cOuterButtonOverlap : 0), thickness);
        else
            result = IntRect(scrollbar->x(), scrollbar->y(), thickness, cOuterButtonLength[scrollbar->controlSize()] + (painting ? cOuterButtonOverlap : 0));
        return result;
    }
    
    // Our repaint rect is slightly larger, since we are a button that is adjacent to the track.
    if (scrollbar->orientation() == HorizontalScrollbar) {
        int start = part == BackButtonStartPart ? scrollbar->x() : scrollbar->x() + scrollbar->width() - cOuterButtonLength[scrollbar->controlSize()] - cButtonLength[scrollbar->controlSize()];
        result = IntRect(start, scrollbar->y(), cButtonLength[scrollbar->controlSize()], thickness);
    } else {
        int start = part == BackButtonStartPart ? scrollbar->y() : scrollbar->y() + scrollbar->height() - cOuterButtonLength[scrollbar->controlSize()] - cButtonLength[scrollbar->controlSize()];
        result = IntRect(scrollbar->x(), start, thickness, cButtonLength[scrollbar->controlSize()]);
    }
    
    if (painting)
        return buttonRepaintRect(result, scrollbar->orientation(), scrollbar->controlSize(), part == BackButtonStartPart);
    return result;
}

IntRect ScrollbarThemeMacOverlayAPI::forwardButtonRect(ScrollbarThemeClient* scrollbar, ScrollbarPart part, bool painting)
{
    ASSERT(buttonsPlacement() == ScrollbarButtonsNone);
    return IntRect();
}

IntRect ScrollbarThemeMacNonOverlayAPI::forwardButtonRect(ScrollbarThemeClient* scrollbar, ScrollbarPart part, bool painting)
{
    IntRect result;
    
    if (part == ForwardButtonEndPart && (buttonsPlacement() == ScrollbarButtonsNone || buttonsPlacement() == ScrollbarButtonsDoubleStart))
        return result;
    
    if (part == ForwardButtonStartPart && (buttonsPlacement() == ScrollbarButtonsNone || buttonsPlacement() == ScrollbarButtonsDoubleEnd || buttonsPlacement() == ScrollbarButtonsSingle))
        return result;
        
    int thickness = scrollbarThickness(scrollbar->controlSize());
    int outerButtonLength = cOuterButtonLength[scrollbar->controlSize()];
    int buttonLength = cButtonLength[scrollbar->controlSize()];
    
    bool outerButton = part == ForwardButtonEndPart && (buttonsPlacement() == ScrollbarButtonsDoubleEnd || buttonsPlacement() == ScrollbarButtonsDoubleBoth);
    if (outerButton) {
        if (scrollbar->orientation() == HorizontalScrollbar) {
            result = IntRect(scrollbar->x() + scrollbar->width() - outerButtonLength, scrollbar->y(), outerButtonLength, thickness);
            if (painting)
                result.inflateX(cOuterButtonOverlap);
        } else {
            result = IntRect(scrollbar->x(), scrollbar->y() + scrollbar->height() - outerButtonLength, thickness, outerButtonLength);
            if (painting)
                result.inflateY(cOuterButtonOverlap);
        }
        return result;
    }
    
    if (scrollbar->orientation() == HorizontalScrollbar) {
        int start = part == ForwardButtonEndPart ? scrollbar->x() + scrollbar->width() - buttonLength : scrollbar->x() + outerButtonLength;
        result = IntRect(start, scrollbar->y(), buttonLength, thickness);
    } else {
        int start = part == ForwardButtonEndPart ? scrollbar->y() + scrollbar->height() - buttonLength : scrollbar->y() + outerButtonLength;
        result = IntRect(scrollbar->x(), start, thickness, buttonLength);
    }
    if (painting)
        return buttonRepaintRect(result, scrollbar->orientation(), scrollbar->controlSize(), part == ForwardButtonStartPart);
    return result;
}

IntRect ScrollbarThemeMacOverlayAPI::trackRect(ScrollbarThemeClient* scrollbar, bool painting)
{
    ASSERT(!hasButtons(scrollbar));
    return scrollbar->frameRect();
}

IntRect ScrollbarThemeMacNonOverlayAPI::trackRect(ScrollbarThemeClient* scrollbar, bool painting)
{
    if (painting || !hasButtons(scrollbar))
        return scrollbar->frameRect();
    
    IntRect result;
    int thickness = scrollbarThickness(scrollbar->controlSize());
    int startWidth = 0;
    int endWidth = 0;
    int outerButtonLength = cOuterButtonLength[scrollbar->controlSize()];
    int buttonLength = cButtonLength[scrollbar->controlSize()];
    int doubleButtonLength = outerButtonLength + buttonLength;
    switch (buttonsPlacement()) {
        case ScrollbarButtonsSingle:
            startWidth = buttonLength;
            endWidth = buttonLength;
            break;
        case ScrollbarButtonsDoubleStart:
            startWidth = doubleButtonLength;
            break;
        case ScrollbarButtonsDoubleEnd:
            endWidth = doubleButtonLength;
            break;
        case ScrollbarButtonsDoubleBoth:
            startWidth = doubleButtonLength;
            endWidth = doubleButtonLength;
            break;
        default:
            break;
    }
    
    int totalWidth = startWidth + endWidth;
    if (scrollbar->orientation() == HorizontalScrollbar)
        return IntRect(scrollbar->x() + startWidth, scrollbar->y(), scrollbar->width() - totalWidth, thickness);
    return IntRect(scrollbar->x(), scrollbar->y() + startWidth, thickness, scrollbar->height() - totalWidth);
}

int ScrollbarThemeMacOverlayAPI::minimumThumbLength(ScrollbarThemeClient* scrollbar)
{
    return [painterForScrollbar(scrollbar) knobMinLength];
}

int ScrollbarThemeMacNonOverlayAPI::minimumThumbLength(ScrollbarThemeClient* scrollbar)
{
    return cThumbMinLength[scrollbar->controlSize()];
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

void ScrollbarThemeMacOverlayAPI::updateEnabledState(ScrollbarThemeClient* scrollbar)
{
    [painterForScrollbar(scrollbar) setEnabled:scrollbar->enabled()];
}

} // namespace WebCore
