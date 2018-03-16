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

#include "platform/scroll/ScrollbarThemeMac.h"

#include <Carbon/Carbon.h>
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/mac/ColorMac.h"
#include "platform/mac/LocalCurrentGraphicsContext.h"
#include "platform/mac/NSScrollerImpDetails.h"
#include "platform/mac/ScrollAnimatorMac.h"
#include "platform/runtime_enabled_features.h"
#include "platform/scroll/ScrollbarThemeClient.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/RetainPtr.h"
#include "platform/wtf/StdLibExtras.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMouseEvent.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebThemeEngine.h"
#include "skia/ext/skia_utils_mac.h"

// FIXME: There are repainting problems due to Aqua scroll bar buttons' visual
// overflow.

@interface NSColor (WebNSColorDetails)
+ (NSImage*)_linenPatternImage;
@end

@interface BlinkScrollbarObserver : NSObject {
  blink::ScrollbarThemeClient* _scrollbar;
  RetainPtr<ScrollbarPainter> _scrollbarPainter;
}
- (id)initWithScrollbar:(blink::ScrollbarThemeClient*)scrollbar
                painter:(const RetainPtr<ScrollbarPainter>&)painter;
@end

@implementation BlinkScrollbarObserver

- (id)initWithScrollbar:(blink::ScrollbarThemeClient*)scrollbar
                painter:(const RetainPtr<ScrollbarPainter>&)painter {
  if (!(self = [super init]))
    return nil;
  _scrollbar = scrollbar;
  _scrollbarPainter = painter;
  [_scrollbarPainter.Get() addObserver:self
                            forKeyPath:@"knobAlpha"
                               options:0
                               context:nil];
  return self;
}

- (id)painter {
  return _scrollbarPainter.Get();
}

- (void)dealloc {
  [_scrollbarPainter.Get() removeObserver:self forKeyPath:@"knobAlpha"];
  [super dealloc];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:@"knobAlpha"]) {
    BOOL visible = [_scrollbarPainter.Get() knobAlpha] > 0;
    _scrollbar->SetScrollbarsHiddenIfOverlay(!visible);
  }
}

@end

namespace blink {

typedef HashSet<ScrollbarThemeClient*> ScrollbarSet;

static ScrollbarSet& GetScrollbarSet() {
  DEFINE_STATIC_LOCAL(ScrollbarSet, set, ());
  return set;
}

static float g_initial_button_delay = 0.5f;
static float g_autoscroll_button_delay = 0.05f;
static NSScrollerStyle g_preferred_scroller_style = NSScrollerStyleLegacy;

typedef HashMap<ScrollbarThemeClient*, RetainPtr<BlinkScrollbarObserver>>
    ScrollbarPainterMap;

static ScrollbarPainterMap& GetScrollbarPainterMap() {
  static ScrollbarPainterMap* map = new ScrollbarPainterMap;
  return *map;
}

static bool SupportsExpandedScrollbars() {
  // FIXME: This is temporary until all platforms that support ScrollbarPainter
  // support this part of the API.
  static bool global_supports_expanded_scrollbars =
      [NSClassFromString(@"NSScrollerImp")
          instancesRespondToSelector:@selector(setExpanded:)];
  return global_supports_expanded_scrollbars;
}

ScrollbarTheme& ScrollbarTheme::NativeTheme() {
  DEFINE_STATIC_LOCAL(ScrollbarThemeMac, overlay_theme, ());
  return overlay_theme;
}

void ScrollbarThemeMac::PaintTickmarks(GraphicsContext& context,
                                       const Scrollbar& scrollbar,
                                       const IntRect& rect) {
  IntRect tickmark_track_rect = rect;
  tickmark_track_rect.SetX(tickmark_track_rect.X() + 1);
  tickmark_track_rect.SetWidth(tickmark_track_rect.Width() - 1);
  ScrollbarTheme::PaintTickmarks(context, scrollbar, tickmark_track_rect);
}

ScrollbarThemeMac::~ScrollbarThemeMac() {}

void ScrollbarThemeMac::PreferencesChanged(
    float initial_button_delay,
    float autoscroll_button_delay,
    NSScrollerStyle preferred_scroller_style,
    bool redraw,
    WebScrollbarButtonsPlacement button_placement) {
  UpdateButtonPlacement(button_placement);
  g_initial_button_delay = initial_button_delay;
  g_autoscroll_button_delay = autoscroll_button_delay;
  g_preferred_scroller_style = preferred_scroller_style;
  if (redraw && !GetScrollbarSet().IsEmpty()) {
    ScrollbarSet::iterator end = GetScrollbarSet().end();
    for (ScrollbarSet::iterator it = GetScrollbarSet().begin(); it != end;
         ++it) {
      (*it)->StyleChanged();
      (*it)->Invalidate();
    }
  }
}

double ScrollbarThemeMac::InitialAutoscrollTimerDelay() {
  return g_initial_button_delay;
}

double ScrollbarThemeMac::AutoscrollTimerDelay() {
  return g_autoscroll_button_delay;
}

bool ScrollbarThemeMac::ShouldDragDocumentInsteadOfThumb(
    const ScrollbarThemeClient&,
    const WebMouseEvent& event) {
  return (event.GetModifiers() & WebInputEvent::Modifiers::kAltKey) != 0;
}

int ScrollbarThemeMac::ScrollbarPartToHIPressedState(ScrollbarPart part) {
  switch (part) {
    case kBackButtonStartPart:
      return kThemeTopOutsideArrowPressed;
    case kBackButtonEndPart:
      // This does not make much sense.  For some reason the outside constant
      // is required.
      return kThemeTopOutsideArrowPressed;
    case kForwardButtonStartPart:
      return kThemeTopInsideArrowPressed;
    case kForwardButtonEndPart:
      return kThemeBottomOutsideArrowPressed;
    case kThumbPart:
      return kThemeThumbPressed;
    default:
      return 0;
  }
}

ScrollbarPart ScrollbarThemeMac::InvalidateOnThumbPositionChange(
    const ScrollbarThemeClient& scrollbar,
    float old_position,
    float new_position) const {
  // ScrollAnimatorMac will invalidate scrollbar parts if necessary.
  return kNoPart;
}

void ScrollbarThemeMac::RegisterScrollbar(ScrollbarThemeClient& scrollbar) {
  GetScrollbarSet().insert(&scrollbar);

  bool is_horizontal = scrollbar.Orientation() == kHorizontalScrollbar;
  RetainPtr<ScrollbarPainter> scrollbar_painter(
      kAdoptNS,
      [[NSClassFromString(@"NSScrollerImp")
          scrollerImpWithStyle:RecommendedScrollerStyle()
                   controlSize:(NSControlSize)scrollbar.GetControlSize()
                    horizontal:is_horizontal
          replacingScrollerImp:nil] retain]);
  RetainPtr<BlinkScrollbarObserver> observer(
      kAdoptNS,
      [[BlinkScrollbarObserver alloc] initWithScrollbar:&scrollbar
                                                painter:scrollbar_painter]);

  GetScrollbarPainterMap().insert(&scrollbar, observer);
  UpdateEnabledState(scrollbar);
  UpdateScrollbarOverlayColorTheme(scrollbar);
}

void ScrollbarThemeMac::UnregisterScrollbar(ScrollbarThemeClient& scrollbar) {
  GetScrollbarPainterMap().erase(&scrollbar);
  GetScrollbarSet().erase(&scrollbar);
}

void ScrollbarThemeMac::SetNewPainterForScrollbar(
    ScrollbarThemeClient& scrollbar,
    ScrollbarPainter new_painter) {
  RetainPtr<ScrollbarPainter> scrollbar_painter(kAdoptNS, [new_painter retain]);
  RetainPtr<BlinkScrollbarObserver> observer(
      kAdoptNS,
      [[BlinkScrollbarObserver alloc] initWithScrollbar:&scrollbar
                                                painter:scrollbar_painter]);
  GetScrollbarPainterMap().Set(&scrollbar, observer);
  UpdateEnabledState(scrollbar);
  UpdateScrollbarOverlayColorTheme(scrollbar);
}

ScrollbarPainter ScrollbarThemeMac::PainterForScrollbar(
    const ScrollbarThemeClient& scrollbar) const {
  return [GetScrollbarPainterMap()
              .at(const_cast<ScrollbarThemeClient*>(&scrollbar))
              .Get() painter];
}

void ScrollbarThemeMac::PaintTrackBackground(GraphicsContext& context,
                                             const Scrollbar& scrollbar,
                                             const IntRect& rect) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          context, scrollbar, DisplayItem::kScrollbarTrackBackground))
    return;

  DrawingRecorder recorder(context, scrollbar,
                           DisplayItem::kScrollbarTrackBackground);

  GraphicsContextStateSaver state_saver(context);
  context.Translate(rect.X(), rect.Y());
  LocalCurrentGraphicsContext local_context(context,
                                            IntRect(IntPoint(), rect.Size()));

  CGRect frame_rect = CGRect(scrollbar.FrameRect());
  ScrollbarPainter scrollbar_painter = PainterForScrollbar(scrollbar);
  [scrollbar_painter setEnabled:scrollbar.Enabled()];
  [scrollbar_painter setBoundsSize:NSSizeFromCGSize(frame_rect.size)];
  NSRect track_rect =
      NSMakeRect(0, 0, frame_rect.size.width, frame_rect.size.height);
  [scrollbar_painter drawKnobSlotInRect:track_rect highlight:NO];
}

void ScrollbarThemeMac::PaintThumb(GraphicsContext& context,
                                   const Scrollbar& scrollbar,
                                   const IntRect& rect) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, scrollbar,
                                                  DisplayItem::kScrollbarThumb))
    return;

  // Expand dirty rect to allow for scroll thumb anti-aliasing in minimum thumb
  // size case.
  DrawingRecorder recorder(context, scrollbar, DisplayItem::kScrollbarThumb);

  GraphicsContextStateSaver state_saver(context);
  context.Translate(rect.X(), rect.Y());
  LocalCurrentGraphicsContext local_context(context,
                                            IntRect(IntPoint(), rect.Size()));

  ScrollbarPainter scrollbar_painter = PainterForScrollbar(scrollbar);
  [scrollbar_painter setEnabled:scrollbar.Enabled()];
  // drawKnob aligns the thumb to right side of the draw rect.
  // If the vertical overlay scrollbar is on the left, use trackWidth instead
  // of scrollbar width, to avoid the gap on the left side of the thumb.
  IntRect draw_rect = IntRect(rect);
  if (UsesOverlayScrollbars() && scrollbar.IsLeftSideVerticalScrollbar()) {
    int thumb_width = [scrollbar_painter trackWidth];
    draw_rect.SetWidth(thumb_width);
  }
  [scrollbar_painter setBoundsSize:NSSizeFromCGSize(CGSize(draw_rect.Size()))];

  [scrollbar_painter setDoubleValue:0];
  [scrollbar_painter setKnobProportion:1];

  CGFloat old_knob_alpha = [scrollbar_painter knobAlpha];
  [scrollbar_painter setKnobAlpha:1];

  if (scrollbar.Enabled())
    [scrollbar_painter drawKnob];

  // If this state is not set, then moving the cursor over the scrollbar area
  // will only cause the scrollbar to engorge when moved over the top of the
  // scrollbar area.
  [scrollbar_painter
      setBoundsSize:NSSizeFromCGSize(CGSize(scrollbar.FrameRect().Size()))];
  [scrollbar_painter setKnobAlpha:old_knob_alpha];
}

int ScrollbarThemeMac::ScrollbarThickness(ScrollbarControlSize control_size) {
  NSControlSize ns_control_size = static_cast<NSControlSize>(control_size);
  ScrollbarPainter scrollbar_painter = [NSClassFromString(@"NSScrollerImp")
      scrollerImpWithStyle:RecommendedScrollerStyle()
               controlSize:ns_control_size
                horizontal:NO
      replacingScrollerImp:nil];
  BOOL was_expanded = NO;
  if (SupportsExpandedScrollbars()) {
    was_expanded = [scrollbar_painter isExpanded];
    [scrollbar_painter setExpanded:YES];
  }
  int thickness = [scrollbar_painter trackBoxWidth];
  if (SupportsExpandedScrollbars())
    [scrollbar_painter setExpanded:was_expanded];
  return thickness;
}

bool ScrollbarThemeMac::UsesOverlayScrollbars() const {
  return RecommendedScrollerStyle() == NSScrollerStyleOverlay;
}

void ScrollbarThemeMac::UpdateScrollbarOverlayColorTheme(
    const ScrollbarThemeClient& scrollbar) {
  ScrollbarPainter painter = PainterForScrollbar(scrollbar);
  switch (scrollbar.GetScrollbarOverlayColorTheme()) {
    case kScrollbarOverlayColorThemeDark:
      [painter setKnobStyle:NSScrollerKnobStyleDark];
      break;
    case kScrollbarOverlayColorThemeLight:
      [painter setKnobStyle:NSScrollerKnobStyleLight];
      break;
  }
}

WebScrollbarButtonsPlacement ScrollbarThemeMac::ButtonsPlacement() const {
  return kWebScrollbarButtonsPlacementNone;
}

bool ScrollbarThemeMac::HasThumb(const ScrollbarThemeClient& scrollbar) {
  ScrollbarPainter painter = PainterForScrollbar(scrollbar);
  int min_length_for_thumb =
      [painter knobMinLength] + [painter trackOverlapEndInset] +
      [painter knobOverlapEndInset] +
      2 * ([painter trackEndInset] + [painter knobEndInset]);
  return scrollbar.Enabled() &&
         (scrollbar.Orientation() == kHorizontalScrollbar
              ? scrollbar.Width()
              : scrollbar.Height()) >= min_length_for_thumb;
}

IntRect ScrollbarThemeMac::BackButtonRect(const ScrollbarThemeClient& scrollbar,
                                          ScrollbarPart part,
                                          bool painting) {
  DCHECK_EQ(ButtonsPlacement(), kWebScrollbarButtonsPlacementNone);
  return IntRect();
}

IntRect ScrollbarThemeMac::ForwardButtonRect(
    const ScrollbarThemeClient& scrollbar,
    ScrollbarPart part,
    bool painting) {
  DCHECK_EQ(ButtonsPlacement(), kWebScrollbarButtonsPlacementNone);
  return IntRect();
}

IntRect ScrollbarThemeMac::TrackRect(const ScrollbarThemeClient& scrollbar,
                                     bool painting) {
  DCHECK(!HasButtons(scrollbar));
  return scrollbar.FrameRect();
}

int ScrollbarThemeMac::MinimumThumbLength(
    const ScrollbarThemeClient& scrollbar) {
  return [PainterForScrollbar(scrollbar) knobMinLength];
}

void ScrollbarThemeMac::UpdateEnabledState(
    const ScrollbarThemeClient& scrollbar) {
  [PainterForScrollbar(scrollbar) setEnabled:scrollbar.Enabled()];
}

float ScrollbarThemeMac::ThumbOpacity(
    const ScrollbarThemeClient& scrollbar) const {
  ScrollbarPainter scrollbar_painter = PainterForScrollbar(scrollbar);
  return [scrollbar_painter knobAlpha];
}

// static
NSScrollerStyle ScrollbarThemeMac::RecommendedScrollerStyle() {
  if (RuntimeEnabledFeatures::OverlayScrollbarsEnabled())
    return NSScrollerStyleOverlay;
  return g_preferred_scroller_style;
}

}  // namespace blink
