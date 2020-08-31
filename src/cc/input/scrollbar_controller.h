// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLLBAR_CONTROLLER_H_
#define CC_INPUT_SCROLLBAR_CONTROLLER_H_

#include "cc/cc_export.h"
#include "cc/input/input_handler.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"

// High level documentation:
// https://source.chromium.org/chromium/chromium/src/+/master:cc/input/README.md

// Click scrolling.
// - A click is considered as a kMouseDown and a kMouseUp in quick succession.
// Every click on a composited non-custom arrow leads to 3 GestureEvents in
// total.
// - GSB and GSU on get queued in the CTEQ on mousedown and a GSE on mouseup.
// - The delta scrolled is constant at 40px (scaled by the device_scale_factor)
// for scrollbar arrows and a function of the viewport length in the case of
// track autoscroll.

// Thumb dragging.
// - The sequence of events in the CTEQ would be something like GSB, GSU, GSU,
// GSU..., GSE
// - On every pointermove, the scroll delta is determined is as current pointer
// position - the point at which we got the initial mousedown.
// - The delta is then scaled by the scroller to scrollbar ratio so that
// dragging the thumb moves the scroller proportionately.
// - This ratio is calculated as:
// (scroll_layer_length - viewport_length) /
// (scrollbar_track_length - scrollbar_thumb_length)
// - On pointerup, the GSE clears state as mentioned above.

// VSync aligned autoscroll.
// - Autoscroll is implemented as a "scroll animation" which has a linear timing
// function (see cc::LinearTimingFunction) and a curve with a constant velocity.
// - The main thread does autoscrolling by pumping events at 50ms interval. To
// have a similar kind of behaviour on the compositor thread, the autoscroll
// velocity is set to 800px per second for scrollbar arrows.
// - For track autoscrolling however, the velocity is a function of the viewport
// length.
// - Based on this velocity, an autoscroll curve is created.
// - An autoscroll animation is set up. (via
// LayerTreeHostImpl::ScrollAnimationCreateInternal) on the the known
// scroll_node and the scroller starts animation when the pointer is held.

// Nuances:
// Thumb snapping.
// - During a thumb drag, if a pointer moves too far away from the scrollbar
// track, the thumb is supposed to snap back to it original place (i.e to the
// point before the thumb drag started).
// - This is done by having an imaginary no_snap_rect around the scrollbar
// track. This extends about 8 times the width of the track on either side. When
// a manipulation is in progress, the mouse is expected to stay within the
// bounds of this rect. Assuming a standard scrollbar, 17px wide, this is how
// it'd look like.
// https://github.com/rahul8805/CompositorThreadedScrollbarDocs/blob/master/snap.PNG?raw=true

// - When a pointerdown is received, record the original offset of the thumb.
// - On every pointermove, check if the pointer is within the bounds of the
// no_snap_rect. If false, snap to the initial_scroll_offset and stop processing
// pointermove(s) until the pointer reenters the bounds of the rect.
// - The moment the mouse re-enters the bounds of the no_snap_rect, we snap to
// the initial_scroll_offset + event.PositionInWidget.

// Thumb anchoring.
// - During a thumb drag, if the pointer runs off the track, there should be no
// additional scrolling until the pointer reenters the track and crosses the
// original mousedown point.
// - This is done by sending "clamped" deltas. The amount of scrollable delta is
// computed using LayerTreeHostImpl::ComputeScrollDelta.
// - Since the deltas are clamped, overscroll doesn't occur if it can't be
// consumed by the CurrentlyScrollingNode.

// Autoscroll play/pause.
// - When the pointer moves in and out of bounds of a scrollbar part that can
// initiate autoscrolls (like arrows or track), the autoscroll animation is
// expected to play or pause accordingly.
// - On every ScrollbarController::WillBeginMainFrame, the pointer location is
// constantly checked and if it is outside the bounds of the scrollbar part that
// initiated the autoscroll, the autoscroll is stopped.
// - Similarly, when the pointer reenters the bounds, autoscroll is restarted
// again. All the vital information during autoscrolling such the velocity,
// direction, scroll layer length etc is held in
// cc::ScrollbarController::AutoscrollState.

// Shift + click.
// - Doing a shift click on any part of a scrollbar track is supposed to do an
// instant scroll to that location (such that the thumb is still centered on the
// pointer).
// - When the MouseEvent reaches the
// InputHandlerProxy::RouteToTypeSpecificHandler, if the event is found to have
// a "Shift" modifier, the ScrollbarController calculates the offset based on
// the pointers current location on the track.
// - Once the offset is determined, the InputHandlerProxy creates a GSU with
// state that tells the LayerTreeHostImpl to perform a non-animated scroll to
// the offset.

// Continuous autoscrolling.
// - This builds on top of the autoscolling implementation. "Continuous"
// autoscrolling is when an autoscroll is in progress and the size of the
// content keeps increasing. For eg: When you keep the down arrow pressed on
// websites like Facebook, the autoscrolling is expected to keep on going until
// the mouse is released.
// - This is implemented by monitoring the length of the scroller layer at every
// frame and if the length increases (and if autoscroll in the forward direction
// is already in progress), the old animation is aborted and a new autoscroll
// animation with the new scroller length is kicked off.

namespace cc {
// This class is responsible for hit testing composited scrollbars, event
// handling and creating gesture scroll deltas.
class CC_EXPORT ScrollbarController {
 public:
  explicit ScrollbarController(LayerTreeHostImpl*);
  virtual ~ScrollbarController();

  InputHandlerPointerResult HandlePointerDown(
      const gfx::PointF position_in_widget,
      const bool shift_modifier);
  InputHandlerPointerResult HandlePointerMove(
      const gfx::PointF position_in_widget);
  InputHandlerPointerResult HandlePointerUp(
      const gfx::PointF position_in_widget);

  // "velocity" here is calculated based on the initial scroll delta (See
  // InitialDeltaToAutoscrollVelocity). This value carries a "sign" which is
  // needed to determine whether we should set up the autoscrolling in the
  // forwards or the backwards direction.
  void StartAutoScrollAnimation(float velocity,
                                const ScrollbarLayerImplBase* scrollbar,
                                ScrollbarPart pressed_scrollbar_part);
  bool ScrollbarScrollIsActive() { return scrollbar_scroll_is_active_; }
  void DidUnregisterScrollbar(ElementId element_id);
  ScrollbarLayerImplBase* ScrollbarLayer();
  void WillBeginImplFrame();
  void ResetState();

 private:
  // "Autoscroll" here means the continuous scrolling that occurs when the
  // pointer is held down on a hit-testable area of the scrollbar such as an
  // arrows of the track itself.
  enum AutoScrollDirection { AUTOSCROLL_FORWARD, AUTOSCROLL_BACKWARD };

  struct CC_EXPORT AutoScrollState {
    // Can only be either AUTOSCROLL_FORWARD or AUTOSCROLL_BACKWARD.
    AutoScrollDirection direction = AutoScrollDirection::AUTOSCROLL_FORWARD;

    // Stores the autoscroll velocity. The sign is used to set the "direction".
    float velocity = 0.f;

    // Used to track the scroller length while autoscrolling. Helpful for
    // setting up infinite scrolling.
    float scroll_layer_length = 0.f;

    // Used to lookup the rect corresponding to the ScrollbarPart so that
    // autoscroll animations can be played/paused depending on the current
    // pointer location.
    ScrollbarPart pressed_scrollbar_part;
  };

  struct CC_EXPORT DragState {
    // This marks the point at which the drag initiated (relative to the widget)
    gfx::PointF drag_origin;

    // This is needed for thumb snapping when the pointer moves too far away
    // from the track while scrolling.
    float scroll_position_at_start_;

    // The |scroller_displacement| indicates the scroll offset compensation that
    // needs to be applied when the scroller's length changes dynamically mid
    // thumb drag. This is needed done to ensure that the scroller does not jump
    // while a thumb drag is in progress.
    float scroller_displacement;
    float scroller_length_at_previous_move;
  };

  struct CC_EXPORT CapturedScrollbarMetadata {
    // Needed to retrieve the ScrollbarSet for a particular ElementId.
    ElementId scroll_element_id;

    // Needed to identify the correct scrollbar from the ScrollbarSet.
    ScrollbarOrientation orientation;
  };

  // Returns the DSF based on whether use-zoom-for-dsf is enabled.
  float ScreenSpaceScaleFactor() const;

  // Helper to convert scroll offset to autoscroll velocity.
  float InitialDeltaToAutoscrollVelocity(
      const ScrollbarLayerImplBase* scrollbar,
      gfx::ScrollOffset scroll_offset) const;

  // Returns the hit tested ScrollbarPart based on the position_in_widget.
  ScrollbarPart GetScrollbarPartFromPointerDown(
      const ScrollbarLayerImplBase* scrollbar,
      const gfx::PointF position_in_widget);

  // Returns scroll offsets based on which ScrollbarPart was hit tested.
  gfx::ScrollOffset GetScrollOffsetForScrollbarPart(
      const ScrollbarLayerImplBase* scrollbar,
      const ScrollbarPart scrollbar_part,
      const bool shift_modifier);

  // Returns the rect for the ScrollbarPart.
  gfx::Rect GetRectForScrollbarPart(const ScrollbarLayerImplBase* scrollbar,
                                    const ScrollbarPart scrollbar_part);

  LayerImpl* GetLayerHitByPoint(const gfx::PointF position_in_widget);
  int GetScrollDeltaForScrollbarPart(const ScrollbarLayerImplBase* scrollbar,
                                     const ScrollbarPart scrollbar_part,
                                     const bool shift_modifier);

  // Makes position_in_widget relative to the scrollbar.
  gfx::PointF GetScrollbarRelativePosition(
      const ScrollbarLayerImplBase* scrollbar,
      const gfx::PointF position_in_widget,
      bool* clipped);

  // Decides if the scroller should snap to the offset that it was originally at
  // (i.e the offset before the thumb drag).
  bool SnapToDragOrigin(const ScrollbarLayerImplBase* scrollbar,
                        const gfx::PointF pointer_position_in_widget);

  // Decides whether a track autoscroll should be aborted (or restarted) due to
  // the thumb reaching the pointer or the pointer leaving (or re-entering) the
  // bounds.
  void RecomputeAutoscrollStateIfNeeded();

  // Shift (or "Option" in case of Mac) + click is expected to do a non-animated
  // jump to a certain offset.
  float GetScrollDeltaForAbsoluteJump(const ScrollbarLayerImplBase* scrollbar);

  // Determines if the delta needs to be animated.
  ui::ScrollGranularity Granularity(const ScrollbarPart scrollbar_part,
                                    bool shift_modifier);

  // Calculates the delta based on position_in_widget and drag_origin.
  int GetScrollDeltaForDragPosition(
      const ScrollbarLayerImplBase* scrollbar,
      const gfx::PointF pointer_position_in_widget);

  // Returns the ratio of the scroller length to the scrollbar length. This is
  // needed to scale the scroll delta for thumb drag.
  float GetScrollerToScrollbarRatio(const ScrollbarLayerImplBase* scrollbar);

  int GetViewportLength(const ScrollbarLayerImplBase* scrollbar) const;

  LayerTreeHostImpl* layer_tree_host_impl_;

  // Used to safeguard against firing GSE without firing GSB and GSU. For
  // example, if mouse is pressed outside the scrollbar but released after
  // moving inside the scrollbar, a GSE will get queued up without this flag.
  bool scrollbar_scroll_is_active_;

  // This is relative to the RenderWidget's origin.
  gfx::PointF last_known_pointer_position_;

  // Set only while interacting with the scrollbar (eg: drag, click etc).
  base::Optional<CapturedScrollbarMetadata> captured_scrollbar_metadata_;

  // Holds information pertaining to autoscrolling. This member is empty if and
  // only if an autoscroll is *not* in progress.
  base::Optional<AutoScrollState> autoscroll_state_;

  // Holds information pertaining to thumb drags. Useful while making decisions
  // about thumb anchoring/snapping.
  base::Optional<DragState> drag_state_;

  // Used to track if a GSU was processed for the current frame or not. Without
  // this, thumb drag will appear jittery. The reason this happens is because
  // when the first GSU is processed, it gets queued in the compositor thread
  // event queue. So a second request within the same frame will end up
  // calculating an incorrect delta (as ComputeThumbQuadRect would not have
  // accounted for the delta in the first GSU that was not yet dispatched and
  // pointermoves are not VSync aligned).
  bool drag_processed_for_current_frame_;

  std::unique_ptr<base::CancelableOnceClosure> cancelable_autoscroll_task_;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_CONTROLLER_H_
