/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/mac/ScrollAnimatorMac.h"

#import <AppKit/AppKit.h>

#include <memory>
#include "platform/Timer.h"
#include "platform/animation/TimingFunction.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntRect.h"
#include "platform/mac/BlockExceptions.h"
#include "platform/mac/NSScrollerImpDetails.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/scroll/ScrollbarThemeMac.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"

using namespace blink;

namespace {

bool SupportsUIStateTransitionProgress() {
  // FIXME: This is temporary until all platforms that support ScrollbarPainter
  // support this part of the API.
  static bool global_supports_ui_state_transition_progress =
      [NSClassFromString(@"NSScrollerImp")
          instancesRespondToSelector:@selector(mouseEnteredScroller)];
  return global_supports_ui_state_transition_progress;
}

bool SupportsExpansionTransitionProgress() {
  static bool global_supports_expansion_transition_progress =
      [NSClassFromString(@"NSScrollerImp")
          instancesRespondToSelector:@selector(expansionTransitionProgress)];
  return global_supports_expansion_transition_progress;
}

bool SupportsContentAreaScrolledInDirection() {
  static bool global_supports_content_area_scrolled_in_direction =
      [NSClassFromString(@"NSScrollerImpPair")
          instancesRespondToSelector:@selector
          (contentAreaScrolledInDirection:)];
  return global_supports_content_area_scrolled_in_direction;
}

ScrollbarThemeMac* MacOverlayScrollbarTheme() {
  ScrollbarTheme& scrollbar_theme = ScrollbarTheme::GetTheme();
  return !scrollbar_theme.IsMockTheme()
             ? static_cast<ScrollbarThemeMac*>(&scrollbar_theme)
             : nil;
}

ScrollbarPainter ScrollbarPainterForScrollbar(Scrollbar& scrollbar) {
  if (ScrollbarThemeMac* scrollbar_theme = MacOverlayScrollbarTheme())
    return scrollbar_theme->PainterForScrollbar(scrollbar);

  return nil;
}

}  // namespace

@interface NSObject (ScrollAnimationHelperDetails)
- (id)initWithDelegate:(id)delegate;
- (void)_stopRun;
- (BOOL)_isAnimating;
- (NSPoint)targetOrigin;
- (CGFloat)_progress;
@end

@interface BlinkScrollAnimationHelperDelegate : NSObject {
  blink::ScrollAnimatorMac* _animator;
}
- (id)initWithScrollAnimator:(blink::ScrollAnimatorMac*)scrollAnimator;
@end

static NSSize abs(NSSize size) {
  NSSize finalSize = size;
  if (finalSize.width < 0)
    finalSize.width = -finalSize.width;
  if (finalSize.height < 0)
    finalSize.height = -finalSize.height;
  return finalSize;
}

@implementation BlinkScrollAnimationHelperDelegate

- (id)initWithScrollAnimator:(blink::ScrollAnimatorMac*)scrollAnimator {
  self = [super init];
  if (!self)
    return nil;

  _animator = scrollAnimator;
  return self;
}

- (void)invalidate {
  _animator = 0;
}

- (NSRect)bounds {
  if (!_animator)
    return NSZeroRect;

  blink::ScrollOffset currentOffset = _animator->CurrentOffset();
  return NSMakeRect(currentOffset.Width(), currentOffset.Height(), 0, 0);
}

- (void)_immediateScrollToPoint:(NSPoint)newPosition {
  if (!_animator)
    return;
  _animator->ImmediateScrollToOffsetForScrollAnimation(
      ToScrollOffset(newPosition));
}

- (NSPoint)_pixelAlignProposedScrollPosition:(NSPoint)newOrigin {
  return newOrigin;
}

- (NSSize)convertSizeToBase:(NSSize)size {
  return abs(size);
}

- (NSSize)convertSizeFromBase:(NSSize)size {
  return abs(size);
}

- (NSSize)convertSizeToBacking:(NSSize)size {
  return abs(size);
}

- (NSSize)convertSizeFromBacking:(NSSize)size {
  return abs(size);
}

- (id)superview {
  return nil;
}

- (id)documentView {
  return nil;
}

- (id)window {
  return nil;
}

- (void)_recursiveRecomputeToolTips {
}

@end

@interface BlinkScrollbarPainterControllerDelegate : NSObject {
  ScrollableArea* _scrollableArea;
}
- (id)initWithScrollableArea:(ScrollableArea*)scrollableArea;
@end

@implementation BlinkScrollbarPainterControllerDelegate

- (id)initWithScrollableArea:(ScrollableArea*)scrollableArea {
  self = [super init];
  if (!self)
    return nil;

  _scrollableArea = scrollableArea;
  return self;
}

- (void)invalidate {
  _scrollableArea = 0;
}

- (NSRect)contentAreaRectForScrollerImpPair:(id)scrollerImpPair {
  if (!_scrollableArea)
    return NSZeroRect;

  blink::IntSize contentsSize = _scrollableArea->ContentsSize();
  return NSMakeRect(0, 0, contentsSize.Width(), contentsSize.Height());
}

- (BOOL)inLiveResizeForScrollerImpPair:(id)scrollerImpPair {
  return NO;
}

- (NSPoint)mouseLocationInContentAreaForScrollerImpPair:(id)scrollerImpPair {
  if (!_scrollableArea)
    return NSZeroPoint;

  return _scrollableArea->LastKnownMousePosition();
}

- (NSPoint)scrollerImpPair:(id)scrollerImpPair
       convertContentPoint:(NSPoint)pointInContentArea
             toScrollerImp:(id)scrollerImp {
  if (!_scrollableArea || !scrollerImp)
    return NSZeroPoint;

  blink::Scrollbar* scrollbar = nil;
  if ([scrollerImp isHorizontal])
    scrollbar = _scrollableArea->HorizontalScrollbar();
  else
    scrollbar = _scrollableArea->VerticalScrollbar();

  // It is possible to have a null scrollbar here since it is possible for this
  // delegate
  // method to be called between the moment when a scrollbar has been set to 0
  // and the
  // moment when its destructor has been called. We should probably de-couple
  // some
  // of the clean-up work in ScrollbarThemeMac::unregisterScrollbar() to avoid
  // this
  // issue.
  if (!scrollbar)
    return NSZeroPoint;

  DCHECK_EQ(scrollerImp, ScrollbarPainterForScrollbar(*scrollbar));

  return scrollbar->ConvertFromContainingEmbeddedContentView(
      blink::IntPoint(pointInContentArea));
}

- (void)scrollerImpPair:(id)scrollerImpPair
    setContentAreaNeedsDisplayInRect:(NSRect)rect {
  if (!_scrollableArea)
    return;

  if (!_scrollableArea->ScrollbarsCanBeActive())
    return;

  _scrollableArea->GetScrollAnimator().ContentAreaWillPaint();
}

- (void)scrollerImpPair:(id)scrollerImpPair
    updateScrollerStyleForNewRecommendedScrollerStyle:
        (NSScrollerStyle)newRecommendedScrollerStyle {
  // Chrome has a single process mode which is used for testing on Mac. In that
  // mode, WebKit runs on a thread in the
  // browser process. This notification is called by the OS on the main thread
  // in the browser process, and not on the
  // the WebKit thread. Better to not update the style than crash.
  // http://crbug.com/126514
  if (!IsMainThread())
    return;

  if (!_scrollableArea)
    return;

  [scrollerImpPair setScrollerStyle:newRecommendedScrollerStyle];

  static_cast<ScrollAnimatorMac&>(_scrollableArea->GetScrollAnimator())
      .UpdateScrollerStyle();
}

@end

enum FeatureToAnimate {
  ThumbAlpha,
  TrackAlpha,
  UIStateTransition,
  ExpansionTransition
};

@class BlinkScrollbarPartAnimation;

namespace blink {

// This class is used to drive the animation timer for
// BlinkScrollbarPartAnimation
// objects. This is used instead of NSAnimation because CoreAnimation
// establishes connections to the WindowServer, which should not be done in a
// sandboxed renderer process.
class BlinkScrollbarPartAnimationTimer {
 public:
  BlinkScrollbarPartAnimationTimer(BlinkScrollbarPartAnimation* animation,
                                   CFTimeInterval duration)
      : timer_(this, &BlinkScrollbarPartAnimationTimer::TimerFired),
        start_time_(0.0),
        duration_(duration),
        animation_(animation),
        timing_function_(CubicBezierTimingFunction::Preset(
            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)) {}

  ~BlinkScrollbarPartAnimationTimer() {}

  void Start() {
    start_time_ = WTF::CurrentTime();
    // Set the framerate of the animation. NSAnimation uses a default
    // framerate of 60 Hz, so use that here.
    timer_.StartRepeating(1.0 / 60.0, BLINK_FROM_HERE);
  }

  void Stop() { timer_.Stop(); }

  void SetDuration(CFTimeInterval duration) { duration_ = duration; }

 private:
  void TimerFired(TimerBase*) {
    double current_time = WTF::CurrentTime();
    double delta = current_time - start_time_;

    if (delta >= duration_)
      timer_.Stop();

    double fraction = delta / duration_;
    fraction = clampTo(fraction, 0.0, 1.0);
    double progress = timing_function_->Evaluate(fraction, 0.001);
    [animation_ setCurrentProgress:progress];
  }

  Timer<BlinkScrollbarPartAnimationTimer> timer_;
  double start_time_;                       // In seconds.
  double duration_;                         // In seconds.
  BlinkScrollbarPartAnimation* animation_;  // Weak, owns this.
  RefPtr<CubicBezierTimingFunction> timing_function_;
};

}  // namespace blink

@interface BlinkScrollbarPartAnimation : NSObject {
  Scrollbar* _scrollbar;
  std::unique_ptr<BlinkScrollbarPartAnimationTimer> _timer;
  RetainPtr<ScrollbarPainter> _scrollbarPainter;
  FeatureToAnimate _featureToAnimate;
  CGFloat _startValue;
  CGFloat _endValue;
}
- (id)initWithScrollbar:(Scrollbar*)scrollbar
       featureToAnimate:(FeatureToAnimate)featureToAnimate
            animateFrom:(CGFloat)startValue
              animateTo:(CGFloat)endValue
               duration:(NSTimeInterval)duration;
@end

@implementation BlinkScrollbarPartAnimation

- (id)initWithScrollbar:(Scrollbar*)scrollbar
       featureToAnimate:(FeatureToAnimate)featureToAnimate
            animateFrom:(CGFloat)startValue
              animateTo:(CGFloat)endValue
               duration:(NSTimeInterval)duration {
  self = [super init];
  if (!self)
    return nil;

  _timer =
      WTF::WrapUnique(new BlinkScrollbarPartAnimationTimer(self, duration));
  _scrollbar = scrollbar;
  _featureToAnimate = featureToAnimate;
  _startValue = startValue;
  _endValue = endValue;

  return self;
}

- (void)startAnimation {
  DCHECK(_scrollbar);

  _scrollbarPainter = ScrollbarPainterForScrollbar(*_scrollbar);
  _timer->Start();
}

- (void)stopAnimation {
  _timer->Stop();
}

- (void)setDuration:(CFTimeInterval)duration {
  _timer->SetDuration(duration);
}

- (void)setStartValue:(CGFloat)startValue {
  _startValue = startValue;
}

- (void)setEndValue:(CGFloat)endValue {
  _endValue = endValue;
}

- (void)setCurrentProgress:(NSAnimationProgress)progress {
  DCHECK(_scrollbar);

  CGFloat currentValue;
  if (_startValue > _endValue)
    currentValue = 1 - progress;
  else
    currentValue = progress;

  ScrollbarPart invalidParts = kNoPart;
  switch (_featureToAnimate) {
    case ThumbAlpha:
      [_scrollbarPainter.Get() setKnobAlpha:currentValue];
      break;
    case TrackAlpha:
      [_scrollbarPainter.Get() setTrackAlpha:currentValue];
      invalidParts = static_cast<ScrollbarPart>(~kThumbPart);
      break;
    case UIStateTransition:
      [_scrollbarPainter.Get() setUiStateTransitionProgress:currentValue];
      invalidParts = kAllParts;
      break;
    case ExpansionTransition:
      [_scrollbarPainter.Get() setExpansionTransitionProgress:currentValue];
      invalidParts = kThumbPart;
      break;
  }

  _scrollbar->SetNeedsPaintInvalidation(invalidParts);
}

- (void)invalidate {
  BEGIN_BLOCK_OBJC_EXCEPTIONS;
  [self stopAnimation];
  END_BLOCK_OBJC_EXCEPTIONS;
  _scrollbar = 0;
}

@end

@interface BlinkScrollbarPainterDelegate : NSObject<NSAnimationDelegate> {
  blink::Scrollbar* _scrollbar;

  RetainPtr<BlinkScrollbarPartAnimation> _knobAlphaAnimation;
  RetainPtr<BlinkScrollbarPartAnimation> _trackAlphaAnimation;
  RetainPtr<BlinkScrollbarPartAnimation> _uiStateTransitionAnimation;
  RetainPtr<BlinkScrollbarPartAnimation> _expansionTransitionAnimation;
  BOOL _hasExpandedSinceInvisible;
}
- (id)initWithScrollbar:(blink::Scrollbar*)scrollbar;
- (void)updateVisibilityImmediately:(bool)show;
- (void)cancelAnimations;
@end

@implementation BlinkScrollbarPainterDelegate

- (id)initWithScrollbar:(blink::Scrollbar*)scrollbar {
  self = [super init];
  if (!self)
    return nil;

  _scrollbar = scrollbar;
  return self;
}

- (void)updateVisibilityImmediately:(bool)show {
  [self cancelAnimations];
  [ScrollbarPainterForScrollbar(*_scrollbar) setKnobAlpha:(show ? 1.0 : 0.0)];
}

- (void)cancelAnimations {
  BEGIN_BLOCK_OBJC_EXCEPTIONS;
  [_knobAlphaAnimation.Get() stopAnimation];
  [_trackAlphaAnimation.Get() stopAnimation];
  [_uiStateTransitionAnimation.Get() stopAnimation];
  [_expansionTransitionAnimation.Get() stopAnimation];
  END_BLOCK_OBJC_EXCEPTIONS;
}

- (ScrollAnimatorMac&)scrollAnimator {
  return static_cast<ScrollAnimatorMac&>(
      _scrollbar->GetScrollableArea()->GetScrollAnimator());
}

- (NSRect)convertRectToBacking:(NSRect)aRect {
  return aRect;
}

- (NSRect)convertRectFromBacking:(NSRect)aRect {
  return aRect;
}

- (NSPoint)mouseLocationInScrollerForScrollerImp:(id)scrollerImp {
  if (!_scrollbar)
    return NSZeroPoint;

  DCHECK_EQ(scrollerImp, ScrollbarPainterForScrollbar(*_scrollbar));

  return _scrollbar->ConvertFromContainingEmbeddedContentView(
      _scrollbar->GetScrollableArea()->LastKnownMousePosition());
}

- (void)setUpAlphaAnimation:
            (RetainPtr<BlinkScrollbarPartAnimation>&)scrollbarPartAnimation
            scrollerPainter:(ScrollbarPainter)scrollerPainter
                       part:(blink::ScrollbarPart)part
             animateAlphaTo:(CGFloat)newAlpha
                   duration:(NSTimeInterval)duration {
  // If the user has scrolled the page, then the scrollbars must be animated
  // here.
  // This overrides the early returns.
  bool mustAnimate = [self scrollAnimator].HaveScrolledSincePageLoad();

  if ([self scrollAnimator].ScrollbarPaintTimerIsActive() && !mustAnimate)
    return;

  if (_scrollbar->GetScrollableArea()->ShouldSuspendScrollAnimations() &&
      !mustAnimate) {
    [self scrollAnimator].StartScrollbarPaintTimer();
    return;
  }

  // At this point, we are definitely going to animate now, so stop the timer.
  [self scrollAnimator].StopScrollbarPaintTimer();

  // If we are currently animating, stop
  if (scrollbarPartAnimation) {
    [scrollbarPartAnimation.Get() stopAnimation];
    scrollbarPartAnimation = nullptr;
  }

  if (part == blink::kThumbPart &&
      _scrollbar->Orientation() == kVerticalScrollbar) {
    if (newAlpha == 1) {
      IntRect thumbRect = IntRect([scrollerPainter rectForPart:NSScrollerKnob]);
      [self scrollAnimator].SetVisibleScrollerThumbRect(thumbRect);
    } else
      [self scrollAnimator].SetVisibleScrollerThumbRect(IntRect());
  }

  scrollbarPartAnimation.AdoptNS([[BlinkScrollbarPartAnimation alloc]
      initWithScrollbar:_scrollbar
       featureToAnimate:part == kThumbPart ? ThumbAlpha : TrackAlpha
            animateFrom:part == kThumbPart ? [scrollerPainter knobAlpha]
                                           : [scrollerPainter trackAlpha]
              animateTo:newAlpha
               duration:duration]);
  [scrollbarPartAnimation.Get() startAnimation];
}

- (void)scrollerImp:(id)scrollerImp
    animateKnobAlphaTo:(CGFloat)newKnobAlpha
              duration:(NSTimeInterval)duration {
  if (!_scrollbar)
    return;

  DCHECK_EQ(scrollerImp, ScrollbarPainterForScrollbar(*_scrollbar));

  ScrollbarPainter scrollerPainter = (ScrollbarPainter)scrollerImp;
  [self setUpAlphaAnimation:_knobAlphaAnimation
            scrollerPainter:scrollerPainter
                       part:blink::kThumbPart
             animateAlphaTo:newKnobAlpha
                   duration:duration];
}

- (void)scrollerImp:(id)scrollerImp
    animateTrackAlphaTo:(CGFloat)newTrackAlpha
               duration:(NSTimeInterval)duration {
  if (!_scrollbar)
    return;

  DCHECK_EQ(scrollerImp, ScrollbarPainterForScrollbar(*_scrollbar));

  ScrollbarPainter scrollerPainter = (ScrollbarPainter)scrollerImp;
  [self setUpAlphaAnimation:_trackAlphaAnimation
            scrollerPainter:scrollerPainter
                       part:blink::kBackTrackPart
             animateAlphaTo:newTrackAlpha
                   duration:duration];
}

- (void)scrollerImp:(id)scrollerImp
    animateUIStateTransitionWithDuration:(NSTimeInterval)duration {
  if (!_scrollbar)
    return;

  if (!SupportsUIStateTransitionProgress())
    return;

  DCHECK_EQ(scrollerImp, ScrollbarPainterForScrollbar(*_scrollbar));

  ScrollbarPainter scrollbarPainter = (ScrollbarPainter)scrollerImp;

  // UIStateTransition always animates to 1. In case an animation is in progress
  // this avoids a hard transition.
  [scrollbarPainter
      setUiStateTransitionProgress:1 - [scrollerImp uiStateTransitionProgress]];

  if (!_uiStateTransitionAnimation)
    _uiStateTransitionAnimation.AdoptNS([[BlinkScrollbarPartAnimation alloc]
        initWithScrollbar:_scrollbar
         featureToAnimate:UIStateTransition
              animateFrom:[scrollbarPainter uiStateTransitionProgress]
                animateTo:1.0
                 duration:duration]);
  else {
    // If we don't need to initialize the animation, just reset the values in
    // case they have changed.
    [_uiStateTransitionAnimation.Get()
        setStartValue:[scrollbarPainter uiStateTransitionProgress]];
    [_uiStateTransitionAnimation.Get() setEndValue:1.0];
    [_uiStateTransitionAnimation.Get() setDuration:duration];
  }
  [_uiStateTransitionAnimation.Get() startAnimation];
}

- (void)scrollerImp:(id)scrollerImp
    animateExpansionTransitionWithDuration:(NSTimeInterval)duration {
  if (!_scrollbar)
    return;

  if (!SupportsExpansionTransitionProgress())
    return;

  DCHECK_EQ(scrollerImp, ScrollbarPainterForScrollbar(*_scrollbar));

  ScrollbarPainter scrollbarPainter = (ScrollbarPainter)scrollerImp;

  // ExpansionTransition always animates to 1. In case an animation is in
  // progress this avoids a hard transition.
  [scrollbarPainter
      setExpansionTransitionProgress:1 -
                                     [scrollerImp expansionTransitionProgress]];

  if (!_expansionTransitionAnimation) {
    _expansionTransitionAnimation.AdoptNS([[BlinkScrollbarPartAnimation alloc]
        initWithScrollbar:_scrollbar
         featureToAnimate:ExpansionTransition
              animateFrom:[scrollbarPainter expansionTransitionProgress]
                animateTo:1.0
                 duration:duration]);
  } else {
    // If we don't need to initialize the animation, just reset the values in
    // case they have changed.
    [_expansionTransitionAnimation.Get()
        setStartValue:[scrollbarPainter uiStateTransitionProgress]];
    [_expansionTransitionAnimation.Get() setEndValue:1.0];
    [_expansionTransitionAnimation.Get() setDuration:duration];
  }
  [_expansionTransitionAnimation.Get() startAnimation];
}

- (void)scrollerImp:(id)scrollerImp
    overlayScrollerStateChangedTo:(NSUInteger)newOverlayScrollerState {
  // The names of these states are based on their observed behavior, and are not
  // based on documentation.
  enum {
    NSScrollerStateInvisible = 0,
    NSScrollerStateKnob = 1,
    NSScrollerStateExpanded = 2
  };
  // We do not receive notifications about the thumb un-expanding when the
  // scrollbar fades away. Ensure
  // that we re-paint the thumb the next time that we transition away from being
  // invisible, so that
  // the thumb doesn't stick in an expanded state.
  if (newOverlayScrollerState == NSScrollerStateExpanded) {
    _hasExpandedSinceInvisible = YES;
  } else if (newOverlayScrollerState != NSScrollerStateInvisible &&
             _hasExpandedSinceInvisible) {
    _scrollbar->SetNeedsPaintInvalidation(kThumbPart);
    _hasExpandedSinceInvisible = NO;
  }
}

- (void)invalidate {
  _scrollbar = 0;
  BEGIN_BLOCK_OBJC_EXCEPTIONS;
  [_knobAlphaAnimation.Get() invalidate];
  [_trackAlphaAnimation.Get() invalidate];
  [_uiStateTransitionAnimation.Get() invalidate];
  [_expansionTransitionAnimation.Get() invalidate];
  END_BLOCK_OBJC_EXCEPTIONS;
}

@end

namespace blink {

ScrollAnimatorBase* ScrollAnimatorBase::Create(
    ScrollableArea* scrollable_area) {
  return new ScrollAnimatorMac(scrollable_area);
}

ScrollAnimatorMac::ScrollAnimatorMac(ScrollableArea* scrollable_area)
    : ScrollAnimatorBase(scrollable_area),
      task_runner_(
          Platform::Current()->CurrentThread()->Scheduler()->TimerTaskRunner()),
      have_scrolled_since_page_load_(false),
      needs_scroller_style_update_(false) {
  scroll_animation_helper_delegate_.AdoptNS(
      [[BlinkScrollAnimationHelperDelegate alloc] initWithScrollAnimator:this]);
  scroll_animation_helper_.AdoptNS(
      [[NSClassFromString(@"NSScrollAnimationHelper") alloc]
          initWithDelegate:scroll_animation_helper_delegate_.Get()]);

  scrollbar_painter_controller_delegate_.AdoptNS(
      [[BlinkScrollbarPainterControllerDelegate alloc]
          initWithScrollableArea:scrollable_area]);
  scrollbar_painter_controller_ =
      [[[NSClassFromString(@"NSScrollerImpPair") alloc] init] autorelease];
  [scrollbar_painter_controller_.Get()
      performSelector:@selector(setDelegate:)
           withObject:scrollbar_painter_controller_delegate_.Get()];
  [scrollbar_painter_controller_.Get()
      setScrollerStyle:ScrollbarThemeMac::RecommendedScrollerStyle()];
}

ScrollAnimatorMac::~ScrollAnimatorMac() {}

void ScrollAnimatorMac::Dispose() {
  BEGIN_BLOCK_OBJC_EXCEPTIONS;
  [scrollbar_painter_controller_delegate_.Get() invalidate];
  [scrollbar_painter_controller_.Get() setDelegate:nil];
  [horizontal_scrollbar_painter_delegate_.Get() invalidate];
  [vertical_scrollbar_painter_delegate_.Get() invalidate];
  [scroll_animation_helper_delegate_.Get() invalidate];
  END_BLOCK_OBJC_EXCEPTIONS;

  initial_scrollbar_paint_task_handle_.Cancel();
  send_content_area_scrolled_task_handle_.Cancel();
}

ScrollResult ScrollAnimatorMac::UserScroll(ScrollGranularity granularity,
                                           const ScrollOffset& delta) {
  have_scrolled_since_page_load_ = true;

  if (!scrollable_area_->ScrollAnimatorEnabled())
    return ScrollAnimatorBase::UserScroll(granularity, delta);

  if (granularity == kScrollByPixel || granularity == kScrollByPrecisePixel)
    return ScrollAnimatorBase::UserScroll(granularity, delta);

  ScrollOffset consumed_delta = ComputeDeltaToConsume(delta);
  ScrollOffset new_offset = current_offset_ + consumed_delta;
  if (current_offset_ == new_offset)
    return ScrollResult();

  // Prevent clobbering an existing animation on an unscrolled axis.
  if ([scroll_animation_helper_.Get() _isAnimating]) {
    NSPoint target_origin = [scroll_animation_helper_.Get() targetOrigin];
    if (!delta.Width())
      new_offset.SetWidth(target_origin.x);
    if (!delta.Height())
      new_offset.SetHeight(target_origin.y);
  }

  NSPoint new_point = NSMakePoint(new_offset.Width(), new_offset.Height());
  [scroll_animation_helper_.Get() scrollToPoint:new_point];

  // TODO(bokan): This has different semantics on ScrollResult than
  // ScrollAnimator,
  // which only returns unused delta if there's no animation and we don't start
  // one.
  return ScrollResult(consumed_delta.Width(), consumed_delta.Height(),
                      delta.Width() - consumed_delta.Width(),
                      delta.Height() - consumed_delta.Height());
}

void ScrollAnimatorMac::ScrollToOffsetWithoutAnimation(
    const ScrollOffset& offset) {
  [scroll_animation_helper_.Get() _stopRun];
  ImmediateScrollTo(offset);
}

ScrollOffset ScrollAnimatorMac::AdjustScrollOffsetIfNecessary(
    const ScrollOffset& offset) const {
  ScrollOffset min_offset = scrollable_area_->MinimumScrollOffset();
  ScrollOffset max_offset = scrollable_area_->MaximumScrollOffset();

  float new_x = clampTo<float, float>(offset.Width(), min_offset.Width(),
                                      max_offset.Width());
  float new_y = clampTo<float, float>(offset.Height(), min_offset.Height(),
                                      max_offset.Height());

  return ScrollOffset(new_x, new_y);
}

void ScrollAnimatorMac::ImmediateScrollTo(const ScrollOffset& new_offset) {
  ScrollOffset adjusted_offset = AdjustScrollOffsetIfNecessary(new_offset);

  bool offset_changed = adjusted_offset != current_offset_;
  if (!offset_changed && !GetScrollableArea()->ScrollOriginChanged())
    return;

  ScrollOffset delta = adjusted_offset - current_offset_;

  current_offset_ = adjusted_offset;
  NotifyContentAreaScrolled(delta, kUserScroll);
  NotifyOffsetChanged();
}

void ScrollAnimatorMac::ImmediateScrollToOffsetForScrollAnimation(
    const ScrollOffset& new_offset) {
  DCHECK(scroll_animation_helper_);
  ImmediateScrollTo(new_offset);
}

void ScrollAnimatorMac::ContentAreaWillPaint() const {
  if (!GetScrollableArea()->ScrollbarsCanBeActive())
    return;
  [scrollbar_painter_controller_.Get() contentAreaWillDraw];
}

void ScrollAnimatorMac::MouseEnteredContentArea() const {
  if (!GetScrollableArea()->ScrollbarsCanBeActive())
    return;
  [scrollbar_painter_controller_.Get() mouseEnteredContentArea];
}

void ScrollAnimatorMac::MouseExitedContentArea() const {
  if (!GetScrollableArea()->ScrollbarsCanBeActive())
    return;
  [scrollbar_painter_controller_.Get() mouseExitedContentArea];
}

void ScrollAnimatorMac::MouseMovedInContentArea() const {
  if (!GetScrollableArea()->ScrollbarsCanBeActive())
    return;
  [scrollbar_painter_controller_.Get() mouseMovedInContentArea];
}

void ScrollAnimatorMac::MouseEnteredScrollbar(Scrollbar& scrollbar) const {
  if (!GetScrollableArea()->ScrollbarsCanBeActive())
    return;

  if (!SupportsUIStateTransitionProgress())
    return;
  if (ScrollbarPainter painter = ScrollbarPainterForScrollbar(scrollbar))
    [painter mouseEnteredScroller];
}

void ScrollAnimatorMac::MouseExitedScrollbar(Scrollbar& scrollbar) const {
  if (!GetScrollableArea()->ScrollbarsCanBeActive())
    return;

  if (!SupportsUIStateTransitionProgress())
    return;
  if (ScrollbarPainter painter = ScrollbarPainterForScrollbar(scrollbar))
    [painter mouseExitedScroller];
}

void ScrollAnimatorMac::ContentsResized() const {
  if (!GetScrollableArea()->ScrollbarsCanBeActive())
    return;
  [scrollbar_painter_controller_.Get() contentAreaDidResize];
}

void ScrollAnimatorMac::ContentAreaDidShow() const {
  if (!GetScrollableArea()->ScrollbarsCanBeActive())
    return;
  [scrollbar_painter_controller_.Get() windowOrderedIn];
}

void ScrollAnimatorMac::ContentAreaDidHide() const {
  if (!GetScrollableArea()->ScrollbarsCanBeActive())
    return;
  [scrollbar_painter_controller_.Get() windowOrderedOut];
}

void ScrollAnimatorMac::FinishCurrentScrollAnimations() {
  [scrollbar_painter_controller_.Get() hideOverlayScrollers];
}

void ScrollAnimatorMac::DidAddVerticalScrollbar(Scrollbar& scrollbar) {
  ScrollbarPainter painter = ScrollbarPainterForScrollbar(scrollbar);
  if (!painter)
    return;

  DCHECK(!vertical_scrollbar_painter_delegate_);
  vertical_scrollbar_painter_delegate_.AdoptNS(
      [[BlinkScrollbarPainterDelegate alloc] initWithScrollbar:&scrollbar]);

  [painter setDelegate:vertical_scrollbar_painter_delegate_.Get()];
  [scrollbar_painter_controller_.Get() setVerticalScrollerImp:painter];
}

void ScrollAnimatorMac::WillRemoveVerticalScrollbar(Scrollbar& scrollbar) {
  ScrollbarPainter painter = ScrollbarPainterForScrollbar(scrollbar);
  if (!painter)
    return;

  DCHECK(vertical_scrollbar_painter_delegate_);
  [vertical_scrollbar_painter_delegate_.Get() invalidate];
  vertical_scrollbar_painter_delegate_ = nullptr;

  [painter setDelegate:nil];
  [scrollbar_painter_controller_.Get() setVerticalScrollerImp:nil];
}

void ScrollAnimatorMac::DidAddHorizontalScrollbar(Scrollbar& scrollbar) {
  ScrollbarPainter painter = ScrollbarPainterForScrollbar(scrollbar);
  if (!painter)
    return;

  DCHECK(!horizontal_scrollbar_painter_delegate_);
  horizontal_scrollbar_painter_delegate_.AdoptNS(
      [[BlinkScrollbarPainterDelegate alloc] initWithScrollbar:&scrollbar]);

  [painter setDelegate:horizontal_scrollbar_painter_delegate_.Get()];
  [scrollbar_painter_controller_.Get() setHorizontalScrollerImp:painter];
}

void ScrollAnimatorMac::WillRemoveHorizontalScrollbar(Scrollbar& scrollbar) {
  ScrollbarPainter painter = ScrollbarPainterForScrollbar(scrollbar);
  if (!painter)
    return;

  DCHECK(horizontal_scrollbar_painter_delegate_);
  [horizontal_scrollbar_painter_delegate_.Get() invalidate];
  horizontal_scrollbar_painter_delegate_ = nullptr;

  [painter setDelegate:nil];
  [scrollbar_painter_controller_.Get() setHorizontalScrollerImp:nil];
}

void ScrollAnimatorMac::NotifyContentAreaScrolled(const ScrollOffset& delta,
                                                  ScrollType scrollType) {
  // This function is called when a page is going into the page cache, but the
  // page
  // isn't really scrolling in that case. We should only pass the message on to
  // the
  // ScrollbarPainterController when we're really scrolling on an active page.
  if (IsExplicitScrollType(scrollType) &&
      GetScrollableArea()->ScrollbarsCanBeActive())
    SendContentAreaScrolledSoon(delta);
}

bool ScrollAnimatorMac::SetScrollbarsVisibleForTesting(bool show) {
  if (show)
    [scrollbar_painter_controller_.Get() flashScrollers];
  else
    [scrollbar_painter_controller_.Get() hideOverlayScrollers];

  [vertical_scrollbar_painter_delegate_.Get() updateVisibilityImmediately:show];
  [horizontal_scrollbar_painter_delegate_.Get()
      updateVisibilityImmediately:show];
  return true;
}

void ScrollAnimatorMac::CancelAnimation() {
  [scroll_animation_helper_.Get() _stopRun];
  have_scrolled_since_page_load_ = false;
}

void ScrollAnimatorMac::UpdateScrollerStyle() {
  if (!GetScrollableArea()->ScrollbarsCanBeActive()) {
    needs_scroller_style_update_ = true;
    return;
  }

  ScrollbarThemeMac* mac_theme = MacOverlayScrollbarTheme();
  if (!mac_theme) {
    needs_scroller_style_update_ = false;
    return;
  }

  NSScrollerStyle new_style =
      [scrollbar_painter_controller_.Get() scrollerStyle];

  if (Scrollbar* vertical_scrollbar =
          GetScrollableArea()->VerticalScrollbar()) {
    vertical_scrollbar->SetNeedsPaintInvalidation(kAllParts);

    ScrollbarPainter old_vertical_painter =
        [scrollbar_painter_controller_.Get() verticalScrollerImp];
    ScrollbarPainter new_vertical_painter = [NSClassFromString(@"NSScrollerImp")
        scrollerImpWithStyle:new_style
                 controlSize:(NSControlSize)vertical_scrollbar->GetControlSize()
                  horizontal:NO
        replacingScrollerImp:old_vertical_painter];
    [scrollbar_painter_controller_.Get()
        setVerticalScrollerImp:new_vertical_painter];
    mac_theme->SetNewPainterForScrollbar(*vertical_scrollbar,
                                         new_vertical_painter);

    // The different scrollbar styles have different thicknesses, so we must
    // re-set the
    // frameRect to the new thickness, and the re-layout below will ensure the
    // offset
    // and length are properly updated.
    int thickness =
        mac_theme->ScrollbarThickness(vertical_scrollbar->GetControlSize());
    vertical_scrollbar->SetFrameRect(IntRect(0, 0, thickness, thickness));
  }

  if (Scrollbar* horizontal_scrollbar =
          GetScrollableArea()->HorizontalScrollbar()) {
    horizontal_scrollbar->SetNeedsPaintInvalidation(kAllParts);

    ScrollbarPainter old_horizontal_painter =
        [scrollbar_painter_controller_.Get() horizontalScrollerImp];
    ScrollbarPainter new_horizontal_painter =
        [NSClassFromString(@"NSScrollerImp")
            scrollerImpWithStyle:new_style
                     controlSize:(NSControlSize)
                                     horizontal_scrollbar->GetControlSize()
                      horizontal:YES
            replacingScrollerImp:old_horizontal_painter];
    [scrollbar_painter_controller_.Get()
        setHorizontalScrollerImp:new_horizontal_painter];
    mac_theme->SetNewPainterForScrollbar(*horizontal_scrollbar,
                                         new_horizontal_painter);

    // The different scrollbar styles have different thicknesses, so we must
    // re-set the
    // frameRect to the new thickness, and the re-layout below will ensure the
    // offset
    // and length are properly updated.
    int thickness =
        mac_theme->ScrollbarThickness(horizontal_scrollbar->GetControlSize());
    horizontal_scrollbar->SetFrameRect(IntRect(0, 0, thickness, thickness));
  }

  // If m_needsScrollerStyleUpdate is true, then the page is restoring from the
  // page cache, and
  // a relayout will happen on its own. Otherwise, we must initiate a re-layout
  // ourselves.
  if (!needs_scroller_style_update_)
    GetScrollableArea()->ScrollbarStyleChanged();

  needs_scroller_style_update_ = false;
}

void ScrollAnimatorMac::StartScrollbarPaintTimer() {
  // Post a task with 1 ms delay to give a chance to run other immediate tasks
  // that may cancel this.
  initial_scrollbar_paint_task_handle_ =
      task_runner_->PostDelayedCancellableTask(
          BLINK_FROM_HERE,
          WTF::Bind(&ScrollAnimatorMac::InitialScrollbarPaintTask,
                    WrapWeakPersistent(this)),
          TimeDelta::FromMilliseconds(1));
}

bool ScrollAnimatorMac::ScrollbarPaintTimerIsActive() const {
  return initial_scrollbar_paint_task_handle_.IsActive();
}

void ScrollAnimatorMac::StopScrollbarPaintTimer() {
  initial_scrollbar_paint_task_handle_.Cancel();
}

void ScrollAnimatorMac::InitialScrollbarPaintTask() {
  // To force the scrollbars to flash, we have to call hide first. Otherwise,
  // the ScrollbarPainterController
  // might think that the scrollbars are already showing and bail early.
  [scrollbar_painter_controller_.Get() hideOverlayScrollers];
  [scrollbar_painter_controller_.Get() flashScrollers];
}

void ScrollAnimatorMac::SendContentAreaScrolledSoon(const ScrollOffset& delta) {
  content_area_scrolled_timer_scroll_delta_ = delta;

  if (send_content_area_scrolled_task_handle_.IsActive())
    return;
  send_content_area_scrolled_task_handle_ = task_runner_->PostCancellableTask(
      BLINK_FROM_HERE,
      WTF::Bind(&ScrollAnimatorMac::SendContentAreaScrolledTask,
                WrapWeakPersistent(this)));
}

void ScrollAnimatorMac::SendContentAreaScrolledTask() {
  if (SupportsContentAreaScrolledInDirection()) {
    [scrollbar_painter_controller_.Get()
        contentAreaScrolledInDirection:
            NSMakePoint(content_area_scrolled_timer_scroll_delta_.Width(),
                        content_area_scrolled_timer_scroll_delta_.Height())];
    content_area_scrolled_timer_scroll_delta_ = ScrollOffset();
  } else
    [scrollbar_painter_controller_.Get() contentAreaScrolled];
}

void ScrollAnimatorMac::SetVisibleScrollerThumbRect(
    const IntRect& scroller_thumb) {
  IntRect rect_in_view_coordinates = scroller_thumb;
  if (Scrollbar* vertical_scrollbar = scrollable_area_->VerticalScrollbar())
    rect_in_view_coordinates =
        vertical_scrollbar->ConvertToContainingEmbeddedContentView(
            scroller_thumb);

  if (rect_in_view_coordinates == visible_scroller_thumb_rect_)
    return;

  visible_scroller_thumb_rect_ = rect_in_view_coordinates;
}

}  // namespace blink
