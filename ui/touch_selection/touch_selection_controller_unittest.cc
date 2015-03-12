// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_selection_controller.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/motion_event_test_utils.h"

using testing::ElementsAre;
using testing::IsEmpty;
using ui::test::MockMotionEvent;

namespace ui {
namespace {

const int kDefaultTapTimeoutMs = 200;
const float kDefaulTapSlop = 10.f;

class MockTouchHandleDrawable : public TouchHandleDrawable {
 public:
  explicit MockTouchHandleDrawable(bool* contains_point)
      : intersects_rect_(contains_point) {}
  ~MockTouchHandleDrawable() override {}
  void SetEnabled(bool enabled) override {}
  void SetOrientation(TouchHandleOrientation orientation) override {}
  void SetAlpha(float alpha) override {}
  void SetFocus(const gfx::PointF& position) override {}
  gfx::RectF GetVisibleBounds() const override {
    return *intersects_rect_ ? gfx::RectF(-1000, -1000, 2000, 2000)
                             : gfx::RectF(-1000, -1000, 0, 0);
  }

 private:
  bool* intersects_rect_;
};

}  // namespace

class TouchSelectionControllerTest : public testing::Test,
                                     public TouchSelectionControllerClient {
 public:
  TouchSelectionControllerTest()
      : caret_moved_(false),
        selection_moved_(false),
        selection_points_swapped_(false),
        needs_animate_(false),
        animation_enabled_(true),
        dragging_enabled_(false) {}

  ~TouchSelectionControllerTest() override {}

  // testing::Test implementation.

  void SetUp() override {
    // Default touch selection controller is created with
    // |show_on_tap_for_empty_editable| flag set to false. Use
    // |AllowShowingOnTapForEmptyEditable()| function to override it.
    bool show_on_tap_for_empty_editable = false;
    controller_.reset(new TouchSelectionController(
        this,
        base::TimeDelta::FromMilliseconds(kDefaultTapTimeoutMs),
        kDefaulTapSlop,
        show_on_tap_for_empty_editable));
  }

  void TearDown() override { controller_.reset(); }

  // TouchSelectionControllerClient implementation.

  bool SupportsAnimation() const override { return animation_enabled_; }

  void SetNeedsAnimate() override { needs_animate_ = true; }

  void MoveCaret(const gfx::PointF& position) override {
    caret_moved_ = true;
    caret_position_ = position;
  }

  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent) override {
    if (base == selection_end_ && extent == selection_start_)
      selection_points_swapped_ = true;

    selection_start_ = base;
    selection_end_ = extent;
  }

  void MoveRangeSelectionExtent(const gfx::PointF& extent) override {
    selection_moved_ = true;
    selection_end_ = extent;
  }

  void OnSelectionEvent(SelectionEventType event,
                        const gfx::PointF& end_position) override {
    events_.push_back(event);
    last_event_start_ = end_position;
  }

  scoped_ptr<TouchHandleDrawable> CreateDrawable() override {
    return make_scoped_ptr(new MockTouchHandleDrawable(&dragging_enabled_));
  }

  void AllowShowingOnTapForEmptyEditable() {
    bool show_on_tap_for_empty_editable = true;
    controller_.reset(new TouchSelectionController(
        this,
        base::TimeDelta::FromMilliseconds(kDefaultTapTimeoutMs),
        kDefaulTapSlop,
        show_on_tap_for_empty_editable));
  }

  void SetAnimationEnabled(bool enabled) { animation_enabled_ = enabled; }
  void SetDraggingEnabled(bool enabled) { dragging_enabled_ = enabled; }

  void ClearSelection() {
    controller_->OnSelectionBoundsChanged(SelectionBound(),
                                          SelectionBound());
  }

  void ClearInsertion() { ClearSelection(); }

  void ChangeInsertion(const gfx::RectF& rect, bool visible) {
    SelectionBound bound;
    bound.set_type(SelectionBound::CENTER);
    bound.SetEdge(rect.origin(), rect.bottom_left());
    bound.set_visible(visible);
    controller_->OnSelectionBoundsChanged(bound, bound);
  }

  void ChangeSelection(const gfx::RectF& start_rect,
                       bool start_visible,
                       const gfx::RectF& end_rect,
                       bool end_visible) {
    SelectionBound start_bound, end_bound;
    start_bound.set_type(SelectionBound::LEFT);
    end_bound.set_type(SelectionBound::RIGHT);
    start_bound.SetEdge(start_rect.origin(), start_rect.bottom_left());
    end_bound.SetEdge(end_rect.origin(), end_rect.bottom_left());
    start_bound.set_visible(start_visible);
    end_bound.set_visible(end_visible);
    controller_->OnSelectionBoundsChanged(start_bound, end_bound);
  }

  void Animate() {
    base::TimeTicks now = base::TimeTicks::Now();
    while (needs_animate_) {
      needs_animate_ = controller_->Animate(now);
      now += base::TimeDelta::FromMilliseconds(16);
    }
  }

  bool GetAndResetNeedsAnimate() {
    bool needs_animate = needs_animate_;
    Animate();
    return needs_animate;
  }

  bool GetAndResetCaretMoved() {
    bool moved = caret_moved_;
    caret_moved_ = false;
    return moved;
  }

  bool GetAndResetSelectionMoved() {
    bool moved = selection_moved_;
    selection_moved_ = false;
    return moved;
  }

  bool GetAndResetSelectionPointsSwapped() {
    bool swapped = selection_points_swapped_;
    selection_points_swapped_ = false;
    return swapped;
  }

  const gfx::PointF& GetLastCaretPosition() const { return caret_position_; }
  const gfx::PointF& GetLastSelectionStart() const { return selection_start_; }
  const gfx::PointF& GetLastSelectionEnd() const { return selection_end_; }
  const gfx::PointF& GetLastEventAnchor() const { return last_event_start_; }

  std::vector<SelectionEventType> GetAndResetEvents() {
    std::vector<SelectionEventType> events;
    events.swap(events_);
    return events;
  }

  TouchSelectionController& controller() { return *controller_; }

 private:
  gfx::PointF last_event_start_;
  gfx::PointF caret_position_;
  gfx::PointF selection_start_;
  gfx::PointF selection_end_;
  std::vector<SelectionEventType> events_;
  bool caret_moved_;
  bool selection_moved_;
  bool selection_points_swapped_;
  bool needs_animate_;
  bool animation_enabled_;
  bool dragging_enabled_;
  scoped_ptr<TouchSelectionController> controller_;
};

TEST_F(TouchSelectionControllerTest, InsertionBasic) {
  gfx::RectF insertion_rect(5, 5, 0, 10);
  bool visible = true;

  // Insertion events are ignored until automatic showing is enabled.
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());
  EXPECT_EQ(gfx::PointF(), GetLastEventAnchor());
  controller().OnTapEvent();

  // Insertion events are ignored until the selection region is marked editable.
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());
  EXPECT_EQ(gfx::PointF(), GetLastEventAnchor());

  controller().OnTapEvent();
  controller().OnSelectionEditable(true);
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_SHOWN));
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventAnchor());

  insertion_rect.Offset(1, 0);
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_MOVED));
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventAnchor());

  insertion_rect.Offset(0, 1);
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_MOVED));
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventAnchor());

  ClearInsertion();
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_CLEARED));
}

TEST_F(TouchSelectionControllerTest, InsertionClearedWhenNoLongerEditable) {
  gfx::RectF insertion_rect(5, 5, 0, 10);
  bool visible = true;
  controller().OnTapEvent();
  controller().OnSelectionEditable(true);

  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_SHOWN));
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventAnchor());

  controller().OnSelectionEditable(false);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_CLEARED));
}

TEST_F(TouchSelectionControllerTest, InsertionWithNoShowOnTapForEmptyEditable) {
  gfx::RectF insertion_rect(5, 5, 0, 10);
  bool visible = true;
  controller().OnSelectionEditable(true);

  // Taps on an empty editable region should be ignored if the controller is
  // created with |show_on_tap_for_empty_editable| set to false.
  controller().OnTapEvent();
  controller().OnSelectionEmpty(true);
  ChangeInsertion(insertion_rect, visible);
  EXPECT_EQ(gfx::PointF(), GetLastEventAnchor());

  // Once the region becomes non-empty, taps should show the insertion handle.
  controller().OnTapEvent();
  controller().OnSelectionEmpty(false);
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_SHOWN));
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventAnchor());

  // Reset the selection.
  controller().HideAndDisallowShowingAutomatically();
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_CLEARED));

  // Long-pressing should show the handle even if the editable region is empty.
  insertion_rect.Offset(2, -2);
  controller().OnLongPressEvent();
  controller().OnSelectionEmpty(true);
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_SHOWN));
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventAnchor());

  // Single Tap on an empty edit field should clear insertion handle.
  controller().OnTapEvent();
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_CLEARED));
}

TEST_F(TouchSelectionControllerTest, InsertionWithShowOnTapForEmptyEditable) {
  AllowShowingOnTapForEmptyEditable();

  gfx::RectF insertion_rect(5, 5, 0, 10);
  bool visible = true;
  controller().OnSelectionEditable(true);

  // Taps on an empty editable region should show the insertion handle if the
  // controller is created with |show_on_tap_for_empty_editable| set to true.
  controller().OnTapEvent();
  controller().OnSelectionEmpty(true);
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_SHOWN));
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventAnchor());

  // Additional taps should not hide the insertion handle in this case.
  controller().OnTapEvent();
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());
}

TEST_F(TouchSelectionControllerTest, InsertionAppearsAfterTapFollowingTyping) {
  gfx::RectF insertion_rect(5, 5, 0, 10);
  bool visible = true;

  // Simulate the user tapping an empty text field.
  controller().OnTapEvent();
  controller().OnSelectionEditable(true);
  controller().OnSelectionEmpty(true);
  ChangeInsertion(insertion_rect, visible);
  EXPECT_EQ(gfx::PointF(), GetLastEventAnchor());

  // Simulate the cursor moving while a user is typing.
  insertion_rect.Offset(10, 0);
  controller().OnSelectionEmpty(false);
  ChangeInsertion(insertion_rect, visible);
  EXPECT_EQ(gfx::PointF(), GetLastEventAnchor());

  // If the user taps the *same* position as the cursor at the end of the text
  // entry, the handle should appear.
  controller().OnTapEvent();
  ChangeInsertion(insertion_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_SHOWN));
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventAnchor());
}

TEST_F(TouchSelectionControllerTest, InsertionToSelectionTransition) {
  controller().OnLongPressEvent();
  controller().OnSelectionEditable(true);

  gfx::RectF start_rect(5, 5, 0, 10);
  gfx::RectF end_rect(50, 5, 0, 10);
  bool visible = true;

  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventAnchor());

  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_CLEARED,
                                               SELECTION_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventAnchor());

  ChangeInsertion(end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_CLEARED,
                                               INSERTION_SHOWN));
  EXPECT_EQ(end_rect.bottom_left(), GetLastEventAnchor());

  controller().HideAndDisallowShowingAutomatically();
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_CLEARED));

  controller().OnTapEvent();
  ChangeInsertion(end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_SHOWN));
  EXPECT_EQ(end_rect.bottom_left(), GetLastEventAnchor());
}

TEST_F(TouchSelectionControllerTest, InsertionDragged) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  controller().OnTapEvent();
  controller().OnSelectionEditable(true);

  // The touch sequence should not be handled if insertion is not active.
  MockMotionEvent event(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));

  float line_height = 10.f;
  gfx::RectF start_rect(10, 0, 0, line_height);
  bool visible = true;
  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventAnchor());

  // The touch sequence should be handled only if the drawable reports a hit.
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));
  SetDraggingEnabled(true);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetCaretMoved());
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_DRAG_STARTED));

  // The MoveCaret() result should reflect the movement.
  // The reported position is offset from the center of |start_rect|.
  gfx::PointF start_offset = start_rect.CenterPoint();
  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time, 0, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetCaretMoved());
  EXPECT_EQ(start_offset + gfx::Vector2dF(0, 5), GetLastCaretPosition());

  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time, 5, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetCaretMoved());
  EXPECT_EQ(start_offset + gfx::Vector2dF(5, 5), GetLastCaretPosition());

  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time, 10, 10);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetCaretMoved());
  EXPECT_EQ(start_offset + gfx::Vector2dF(10, 10), GetLastCaretPosition());

  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetCaretMoved());
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_DRAG_STOPPED));

  // Once the drag is complete, no more touch events should be consumed until
  // the next ACTION_DOWN.
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));
}

TEST_F(TouchSelectionControllerTest, InsertionTapped) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  controller().OnTapEvent();
  controller().OnSelectionEditable(true);
  SetDraggingEnabled(true);

  gfx::RectF start_rect(10, 0, 0, 10);
  bool visible = true;
  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_SHOWN));

  MockMotionEvent event(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  //TODO(AKV): this test case has to be modified once crbug.com/394093 is fixed.
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_DRAG_STARTED));

  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_TAPPED,
                                               INSERTION_DRAG_STOPPED));

  // Reset the insertion.
  ClearInsertion();
  controller().OnTapEvent();
  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_CLEARED,
                                               INSERTION_SHOWN));

  // No tap should be signalled if the time between DOWN and UP was too long.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  event = MockMotionEvent(MockMotionEvent::ACTION_UP,
                          event_time + base::TimeDelta::FromSeconds(1),
                          0,
                          0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_DRAG_STARTED,
                                               INSERTION_DRAG_STOPPED));

  // Reset the insertion.
  ClearInsertion();
  controller().OnTapEvent();
  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_CLEARED,
                                               INSERTION_SHOWN));

  // No tap should be signalled if the drag was too long.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time, 100, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 100, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_DRAG_STARTED,
                                               INSERTION_DRAG_STOPPED));

  // Reset the insertion.
  ClearInsertion();
  controller().OnTapEvent();
  ChangeInsertion(start_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_CLEARED,
                                               INSERTION_SHOWN));

  // No tap should be signalled if the touch sequence is cancelled.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  event = MockMotionEvent(MockMotionEvent::ACTION_CANCEL, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_DRAG_STARTED,
                                               INSERTION_DRAG_STOPPED));
}

TEST_F(TouchSelectionControllerTest, InsertionNotResetByRepeatedTapOrPress) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  controller().OnTapEvent();
  controller().OnSelectionEditable(true);
  SetDraggingEnabled(true);

  gfx::RectF anchor_rect(10, 0, 0, 10);
  bool visible = true;
  ChangeInsertion(anchor_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_SHOWN));
  EXPECT_EQ(anchor_rect.bottom_left(), GetLastEventAnchor());

  // Tapping again shouldn't reset the active insertion point.
  controller().OnTapEvent();
  MockMotionEvent event(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_DRAG_STARTED));
  EXPECT_EQ(anchor_rect.bottom_left(), GetLastEventAnchor());

  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_TAPPED,
                                               INSERTION_DRAG_STOPPED));
  EXPECT_EQ(anchor_rect.bottom_left(), GetLastEventAnchor());

  anchor_rect.Offset(5, 15);
  ChangeInsertion(anchor_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_MOVED));
  EXPECT_EQ(anchor_rect.bottom_left(), GetLastEventAnchor());

  // Pressing shouldn't reset the active insertion point.
  controller().OnLongPressEvent();
  controller().OnSelectionEmpty(true);
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_DRAG_STARTED));
  EXPECT_EQ(anchor_rect.bottom_left(), GetLastEventAnchor());

  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_TAPPED,
                                               INSERTION_DRAG_STOPPED));
  EXPECT_EQ(anchor_rect.bottom_left(), GetLastEventAnchor());
}

TEST_F(TouchSelectionControllerTest, SelectionBasic) {
  gfx::RectF start_rect(5, 5, 0, 10);
  gfx::RectF end_rect(50, 5, 0, 10);
  bool visible = true;

  // Selection events are ignored until automatic showing is enabled.
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_EQ(gfx::PointF(), GetLastEventAnchor());

  controller().OnLongPressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventAnchor());

  start_rect.Offset(1, 0);
  ChangeSelection(start_rect, visible, end_rect, visible);
  // Selection movement does not currently trigger a separate event.
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());

  ClearSelection();
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_CLEARED));
  EXPECT_EQ(gfx::PointF(), GetLastEventAnchor());
}

TEST_F(TouchSelectionControllerTest, SelectionRepeatedLongPress) {
  gfx::RectF start_rect(5, 5, 0, 10);
  gfx::RectF end_rect(50, 5, 0, 10);
  bool visible = true;

  controller().OnLongPressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventAnchor());

  // A long press triggering a new selection should re-send the SELECTION_SHOWN
  // event notification.
  start_rect.Offset(10, 10);
  controller().OnLongPressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventAnchor());
}

TEST_F(TouchSelectionControllerTest, SelectionDragged) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  controller().OnLongPressEvent();

  // The touch sequence should not be handled if selection is not active.
  MockMotionEvent event(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));

  float line_height = 10.f;
  gfx::RectF start_rect(0, 0, 0, line_height);
  gfx::RectF end_rect(50, 0, 0, line_height);
  bool visible = true;
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventAnchor());

  // The touch sequence should be handled only if the drawable reports a hit.
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));
  SetDraggingEnabled(true);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetSelectionMoved());

  // The SelectBetweenCoordinates() result should reflect the movement. Note
  // that the start coordinate will always reflect the "fixed" handle's
  // position, in this case the position from |end_rect|.
  // Note that the reported position is offset from the center of the
  // input rects (i.e., the middle of the corresponding text line).
  gfx::PointF fixed_offset = end_rect.CenterPoint();
  gfx::PointF start_offset = start_rect.CenterPoint();
  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time, 0, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_DRAG_STARTED));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(fixed_offset, GetLastSelectionStart());
  EXPECT_EQ(start_offset + gfx::Vector2dF(0, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time, 5, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(fixed_offset, GetLastSelectionStart());
  EXPECT_EQ(start_offset + gfx::Vector2dF(5, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(fixed_offset, GetLastSelectionStart());
  EXPECT_EQ(start_offset + gfx::Vector2dF(10, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_DRAG_STOPPED));
  EXPECT_FALSE(GetAndResetSelectionMoved());

  // Once the drag is complete, no more touch events should be consumed until
  // the next ACTION_DOWN.
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));
}

TEST_F(TouchSelectionControllerTest, SelectionDraggedWithOverlap) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  controller().OnLongPressEvent();

  float line_height = 10.f;
  gfx::RectF start_rect(0, 0, 0, line_height);
  gfx::RectF end_rect(50, 0, 0, line_height);
  bool visible = true;
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventAnchor());

  // The ACTION_DOWN should lock to the closest handle.
  gfx::PointF end_offset = end_rect.CenterPoint();
  gfx::PointF fixed_offset = start_rect.CenterPoint();
  float touch_down_x = (end_offset.x() + fixed_offset.x()) / 2 + 1.f;
  MockMotionEvent event(
      MockMotionEvent::ACTION_DOWN, event_time, touch_down_x, 0);
  SetDraggingEnabled(true);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_DRAG_STARTED));
  EXPECT_FALSE(GetAndResetSelectionMoved());

  // Even though the ACTION_MOVE is over the start handle, it should continue
  // targetting the end handle that consumed the ACTION_DOWN.
  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(fixed_offset, GetLastSelectionStart());
  EXPECT_EQ(end_offset - gfx::Vector2dF(touch_down_x, 0),
            GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_DRAG_STOPPED));
  EXPECT_FALSE(GetAndResetSelectionMoved());
}

TEST_F(TouchSelectionControllerTest, SelectionDraggedToSwitchBaseAndExtent) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  controller().OnLongPressEvent();

  float line_height = 10.f;
  gfx::RectF start_rect(50, line_height, 0, line_height);
  gfx::RectF end_rect(100, line_height, 0, line_height);
  bool visible = true;
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventAnchor());

  SetDraggingEnabled(true);

  // Move the extent, not triggering a swap of points.
  MockMotionEvent event(MockMotionEvent::ACTION_DOWN, event_time,
                        end_rect.x(), end_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetSelectionMoved());
  EXPECT_FALSE(GetAndResetSelectionPointsSwapped());

  gfx::PointF base_offset = start_rect.CenterPoint();
  gfx::PointF extent_offset = end_rect.CenterPoint();
  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time,
                          end_rect.x(), end_rect.bottom() + 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_DRAG_STARTED));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_FALSE(GetAndResetSelectionPointsSwapped());
  EXPECT_EQ(base_offset, GetLastSelectionStart());
  EXPECT_EQ(extent_offset + gfx::Vector2dF(0, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_DRAG_STOPPED));
  EXPECT_FALSE(GetAndResetSelectionMoved());

  end_rect += gfx::Vector2dF(0, 5);
  ChangeSelection(start_rect, visible, end_rect, visible);

  // Move the base, triggering a swap of points.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time,
                          start_rect.x(), start_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetSelectionMoved());
  EXPECT_TRUE(GetAndResetSelectionPointsSwapped());

  base_offset = end_rect.CenterPoint();
  extent_offset = start_rect.CenterPoint();
  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time,
                          start_rect.x(), start_rect.bottom() + 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_DRAG_STARTED));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_FALSE(GetAndResetSelectionPointsSwapped());
  EXPECT_EQ(base_offset, GetLastSelectionStart());
  EXPECT_EQ(extent_offset + gfx::Vector2dF(0, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_DRAG_STOPPED));
  EXPECT_FALSE(GetAndResetSelectionMoved());

  start_rect += gfx::Vector2dF(0, 5);
  ChangeSelection(start_rect, visible, end_rect, visible);

  // Move the same point again, not triggering a swap of points.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time,
                          start_rect.x(), start_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetSelectionMoved());
  EXPECT_FALSE(GetAndResetSelectionPointsSwapped());

  base_offset = end_rect.CenterPoint();
  extent_offset = start_rect.CenterPoint();
  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time,
                          start_rect.x(), start_rect.bottom() + 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_DRAG_STARTED));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_FALSE(GetAndResetSelectionPointsSwapped());
  EXPECT_EQ(base_offset, GetLastSelectionStart());
  EXPECT_EQ(extent_offset + gfx::Vector2dF(0, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_DRAG_STOPPED));
  EXPECT_FALSE(GetAndResetSelectionMoved());

  start_rect += gfx::Vector2dF(0, 5);
  ChangeSelection(start_rect, visible, end_rect, visible);

  // Move the base, triggering a swap of points.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time,
                          end_rect.x(), end_rect.bottom());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetSelectionMoved());
  EXPECT_TRUE(GetAndResetSelectionPointsSwapped());

  base_offset = start_rect.CenterPoint();
  extent_offset = end_rect.CenterPoint();
  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time,
                          end_rect.x(), end_rect.bottom() + 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_DRAG_STARTED));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_FALSE(GetAndResetSelectionPointsSwapped());
  EXPECT_EQ(base_offset, GetLastSelectionStart());
  EXPECT_EQ(extent_offset + gfx::Vector2dF(0, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_DRAG_STOPPED));
  EXPECT_FALSE(GetAndResetSelectionMoved());
}

TEST_F(TouchSelectionControllerTest, SelectionDragExtremeLineSize) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  controller().OnLongPressEvent();

  float small_line_height = 1.f;
  float large_line_height = 50.f;
  gfx::RectF small_line_rect(0, 0, 0, small_line_height);
  gfx::RectF large_line_rect(50, 50, 0, large_line_height);
  bool visible = true;
  ChangeSelection(small_line_rect, visible, large_line_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_SHOWN));
  EXPECT_EQ(small_line_rect.bottom_left(), GetLastEventAnchor());

  // Start dragging the handle on the small line.
  MockMotionEvent event(MockMotionEvent::ACTION_DOWN, event_time,
                        small_line_rect.x(), small_line_rect.y());
  SetDraggingEnabled(true);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_DRAG_STARTED));
  // The drag coordinate for large lines should be capped to a reasonable
  // offset, allowing seamless transition to neighboring lines with different
  // sizes. The drag coordinate for small lines should have an offset
  // commensurate with the small line size.
  EXPECT_EQ(large_line_rect.bottom_left() - gfx::Vector2dF(0, 5.f),
            GetLastSelectionStart());
  EXPECT_EQ(small_line_rect.CenterPoint(), GetLastSelectionEnd());

  small_line_rect += gfx::Vector2dF(25.f, 0);
  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time,
                          small_line_rect.x(), small_line_rect.y());
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(small_line_rect.CenterPoint(), GetLastSelectionEnd());
}

TEST_F(TouchSelectionControllerTest, Animation) {
  controller().OnTapEvent();
  controller().OnSelectionEditable(true);

  gfx::RectF insertion_rect(5, 5, 0, 10);

  bool visible = true;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_FALSE(GetAndResetNeedsAnimate());

  visible = false;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_TRUE(GetAndResetNeedsAnimate());

  visible = true;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_TRUE(GetAndResetNeedsAnimate());

  // If the handles are explicity hidden, no animation should be triggered.
  controller().HideAndDisallowShowingAutomatically();
  EXPECT_FALSE(GetAndResetNeedsAnimate());

  // If the client doesn't support animation, no animation should be triggered.
  SetAnimationEnabled(false);
  controller().OnTapEvent();
  visible = true;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_FALSE(GetAndResetNeedsAnimate());
}

TEST_F(TouchSelectionControllerTest, TemporarilyHidden) {
  controller().OnTapEvent();
  controller().OnSelectionEditable(true);

  gfx::RectF insertion_rect(5, 5, 0, 10);

  bool visible = true;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_FALSE(GetAndResetNeedsAnimate());

  controller().SetTemporarilyHidden(true);
  EXPECT_TRUE(GetAndResetNeedsAnimate());

  visible = false;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_FALSE(GetAndResetNeedsAnimate());

  visible = true;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_FALSE(GetAndResetNeedsAnimate());

  controller().SetTemporarilyHidden(false);
  EXPECT_TRUE(GetAndResetNeedsAnimate());
}

TEST_F(TouchSelectionControllerTest, SelectionClearOnTap) {
  gfx::RectF start_rect(5, 5, 0, 10);
  gfx::RectF end_rect(50, 5, 0, 10);
  bool visible = true;

  controller().OnLongPressEvent();
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_SHOWN));

  // Selection should not be cleared if the selection bounds have not changed.
  controller().OnTapEvent();
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventAnchor());

  controller().OnTapEvent();
  ClearSelection();
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_CLEARED));
  EXPECT_EQ(gfx::PointF(), GetLastEventAnchor());
}

TEST_F(TouchSelectionControllerTest, AllowShowingFromCurrentSelection) {
  gfx::RectF start_rect(5, 5, 0, 10);
  gfx::RectF end_rect(50, 5, 0, 10);
  bool visible = true;

  // The selection should not be activated, as it wasn't yet allowed.
  ChangeSelection(start_rect, visible, end_rect, visible);
  EXPECT_EQ(gfx::PointF(), GetLastEventAnchor());

  // Now explicitly allow showing from the previously supplied bounds.
  controller().AllowShowingFromCurrentSelection();
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_SHOWN));
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventAnchor());

  // Repeated calls to show from the current selection should be ignored.
  controller().AllowShowingFromCurrentSelection();
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());
  EXPECT_EQ(start_rect.bottom_left(), GetLastEventAnchor());

  // Trying to show from an empty selection will have no result.
  ClearSelection();
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(SELECTION_CLEARED));
  controller().AllowShowingFromCurrentSelection();
  EXPECT_THAT(GetAndResetEvents(), IsEmpty());

  // Showing the insertion handle should also be supported.
  controller().OnSelectionEditable(true);
  controller().OnSelectionEmpty(false);
  controller().HideAndDisallowShowingAutomatically();
  gfx::RectF insertion_rect(5, 5, 0, 10);
  ChangeInsertion(insertion_rect, visible);
  controller().AllowShowingFromCurrentSelection();
  EXPECT_THAT(GetAndResetEvents(), ElementsAre(INSERTION_SHOWN));
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventAnchor());
}

}  // namespace ui
