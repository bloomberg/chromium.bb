// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/gestures/gesture_configuration.h"
#include "ui/base/gestures/gesture_sequence.h"
#include "ui/base/gestures/gesture_sequence_test_base.h"
#include "ui/base/gestures/gesture_types.h"

namespace ui {
namespace test {

namespace {
const int kTouchId1 = 2;
const int kTouchId2 = 5;
const int kTouchId3 = 3;
}  // namespace

typedef GestureSequenceTestBase GestureSequenceTest;
typedef scoped_ptr<FakeGestures> Gestures;

TEST_F(GestureSequenceTest, TapDown) {
  Gestures gestures(Press(0, 0, kTouchId1));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 1, ET_GESTURE_TAP_DOWN));
  EXPECT_TRUE(StatesAre(2,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK));
}

TEST_F(GestureSequenceTest, Tap) {
  Press(0, 0, kTouchId1);

  Gestures gestures(Release(0, 0, kTouchId1, 50));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 1, ET_GESTURE_TAP));

  EXPECT_TRUE(StatesAre(3,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_NO_GESTURE));
}

TEST_F(GestureSequenceTest, TapCancel) {
  Press(0, 0, kTouchId1);
  Gestures gestures(Cancel(0, 0, kTouchId1));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 0));

  EXPECT_TRUE(StatesAre(3,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_NO_GESTURE));
}

TEST_F(GestureSequenceTest, NonRailScroll) {
  Press(0, 0, kTouchId1);
  Gestures gestures(Move(50, 51, kTouchId1));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 2,
                            ET_GESTURE_SCROLL_BEGIN,
                            ET_GESTURE_SCROLL_UPDATE));

  EXPECT_EQ(50, (*gestures)[1]->param_first());
  EXPECT_EQ(51, (*gestures)[1]->param_second());

  gestures.reset(Move(150, 150, kTouchId1));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_SCROLL_UPDATE));

  EXPECT_EQ(100, (*gestures)[0]->param_first());
  EXPECT_EQ(99, (*gestures)[0]->param_second());

  gestures.reset(Release(0, 0, kTouchId1));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_SCROLL_END));

  EXPECT_TRUE(StatesAre(4,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_SCROLL,
                        GS_NO_GESTURE));
}

// Check that horizontal touch moves cause scrolls on horizontal rails.
// Also tests that horizontal rails can be broken.
TEST_F(GestureSequenceTest, HorizontalRailScroll) {
  Press(0, 0, kTouchId1);
  Gestures gestures(Move(20, 1, kTouchId1));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 2,
                            ET_GESTURE_SCROLL_BEGIN,
                            ET_GESTURE_SCROLL_UPDATE));

  EXPECT_EQ(20, (*gestures)[1]->param_first());
  // We should be on a horizontal rail.
  EXPECT_EQ(0, (*gestures)[1]->param_second());

  // Break the rail.
  for (int i = 0; i < GestureConfiguration::points_buffered_for_velocity(); ++i)
    Move(1, i * 100, kTouchId1, 50);

  Move(0, 0, kTouchId1);
  gestures.reset(Move(20, 1, kTouchId1));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_SCROLL_UPDATE));

  EXPECT_EQ(20, (*gestures)[0]->param_first());
  // We shouldn't be on a horizontal rail.
  EXPECT_EQ(1, (*gestures)[0]->param_second());

  EXPECT_TRUE(StatesAre(3,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_SCROLL));
}

// Check that vertical touch moves cause scrolls on vertical rails.
// Also tests that vertical rails can be broken.
TEST_F(GestureSequenceTest, VerticalRailScroll) {
  Press(0, 0, kTouchId1);
  Gestures gestures(Move(1, 20, kTouchId1));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 2,
                            ET_GESTURE_SCROLL_BEGIN,
                            ET_GESTURE_SCROLL_UPDATE));

  // We should be on a vertical rail.
  EXPECT_EQ(0, (*gestures)[1]->param_first());
  EXPECT_EQ(20, (*gestures)[1]->param_second());

  // Break the rail.
  for (int i = 0; i < GestureConfiguration::points_buffered_for_velocity(); ++i)
    Move(i * 100, 1, kTouchId1, 50);

  Move(0, 0, kTouchId1);
  gestures.reset(Move(1, 20, kTouchId1));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_SCROLL_UPDATE));

  // We shouldn't be on a vertical rail.
  EXPECT_EQ(1, (*gestures)[0]->param_first());
  EXPECT_EQ(20, (*gestures)[0]->param_second());

  EXPECT_TRUE(StatesAre(3,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_SCROLL));
}

// Check Scroll End Events report correct velocities if the user was
// on a horizontal rail.
TEST_F(GestureSequenceTest, HorizontalRailFling) {
  Press(0, 0, kTouchId1);

  // Get a high x velocity, while still staying on a horizontal rail.
  for (int i = 0; i < GestureConfiguration::points_buffered_for_velocity(); ++i)
    Move(i * 100, 1, kTouchId1, 1);

  Gestures gestures(Release(
      GestureConfiguration::points_buffered_for_velocity() * 100,
      GestureConfiguration::points_buffered_for_velocity(),
      kTouchId1));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 2,
                            ET_GESTURE_SCROLL_END,
                            ET_SCROLL_FLING_START));

  // We should be on a horizontal rail.
  EXPECT_EQ(100000, (*gestures)[0]->param_first());
  EXPECT_EQ(0, (*gestures)[0]->param_second());

  EXPECT_TRUE(StatesAre(4,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_SCROLL,
                        GS_NO_GESTURE));
}

// Check Scroll End Events report correct velocities if the user was
// on a vertical rail.
TEST_F(GestureSequenceTest, VerticalRailFling) {
  Press(0, 0, kTouchId1);

  // Get a high y velocity, while still staying on a vertical rail.
  for (int i = 0; i < GestureConfiguration::points_buffered_for_velocity(); ++i)
    Move(1, i * 100, kTouchId1, 1);

  Gestures gestures(Release(
      GestureConfiguration::points_buffered_for_velocity(),
      GestureConfiguration::points_buffered_for_velocity() * 100,
      kTouchId1));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 2,
                            ET_GESTURE_SCROLL_END,
                            ET_SCROLL_FLING_START));

  // We should be on a vertical rail.
  EXPECT_EQ(0, (*gestures)[0]->param_first());
  EXPECT_EQ(100000, (*gestures)[0]->param_second());

  EXPECT_TRUE(StatesAre(4,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_SCROLL,
                        GS_NO_GESTURE));
}

// Check Scroll End Events report correct velocities if the user was
// not on a rail.
TEST_F(GestureSequenceTest, NonRailFling) {
  Press(0, 0, kTouchId1);
  // Start a non rail scroll.
  Move(20, 20, kTouchId1);

  // Get a high velocity.
  for (int i = 0; i < GestureConfiguration::points_buffered_for_velocity(); ++i)
    Move(i * 10, i * 100, kTouchId1, 1);

  Gestures gestures(Release(
      GestureConfiguration::points_buffered_for_velocity() * 10,
      GestureConfiguration::points_buffered_for_velocity() * 100,
      kTouchId1));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 2,
                            ET_GESTURE_SCROLL_END,
                            ET_SCROLL_FLING_START));

  EXPECT_EQ(10000, (*gestures)[0]->param_first());
  EXPECT_EQ(100000, (*gestures)[0]->param_second());

  EXPECT_TRUE(StatesAre(4,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_SCROLL,
                        GS_NO_GESTURE));
}

TEST_F(GestureSequenceTest, TapFollowedByScroll) {
  Press(101, 201, kTouchId1);
  Gestures gestures(Release(101, 201, kTouchId1, 50));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_TAP));

  // Make sure it isn't counted as a double tap, with a big delay.
  gestures.reset(Press(101, 201, kTouchId1, 1000));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_TAP_DOWN));

  gestures.reset(Move(130, 230, kTouchId1));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 2,
                            ET_GESTURE_SCROLL_BEGIN,
                            ET_GESTURE_SCROLL_UPDATE));

  EXPECT_EQ(29, (*gestures)[1]->param_first());
  EXPECT_EQ(29, (*gestures)[1]->param_second());

  gestures.reset(Move(110, 211, kTouchId1));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_SCROLL_UPDATE));

  EXPECT_EQ(-20, (*gestures)[0]->param_first());
  EXPECT_EQ(-19, (*gestures)[0]->param_second());

  gestures.reset(Move(140, 215, kTouchId1));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_SCROLL_UPDATE));

  EXPECT_EQ(30, (*gestures)[0]->param_first());
  EXPECT_EQ(4, (*gestures)[0]->param_second());

  gestures.reset(Release(101, 201, kTouchId1));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_SCROLL_END));

  EXPECT_TRUE(StatesAre(6,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_SCROLL,
                        GS_NO_GESTURE));
}

TEST_F(GestureSequenceTest, Pinch) {
  Press(101, 201, kTouchId1);
  Gestures gestures(Press(10, 10, kTouchId2));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 3,
                            ET_GESTURE_TAP_DOWN,
                            ET_GESTURE_PINCH_BEGIN,
                            ET_GESTURE_SCROLL_BEGIN));

  gestures.reset(Release(101, 201, kTouchId1));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_PINCH_END));

  gestures.reset(Release(10, 10, kTouchId2));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_SCROLL_END));

  EXPECT_TRUE(StatesAre(5,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_PINCH,
                        GS_SCROLL,
                        GS_NO_GESTURE));
}

TEST_F(GestureSequenceTest, PinchFromScroll) {
  Press(101, 201, kTouchId1);
  Move(130, 201, kTouchId1);
  // Press finger 2
  Gestures gestures(Press(10, 10, kTouchId2));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 2,
                            ET_GESTURE_TAP_DOWN,
                            ET_GESTURE_PINCH_BEGIN));
  // Move finger 1 away from finger 2.
  gestures.reset(Move(400, 400, kTouchId1));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_PINCH_UPDATE));

  // We should have zoomed in.
  EXPECT_LT(2, (*gestures)[0]->param_first());
  EXPECT_GT(3, (*gestures)[0]->param_first());
  // Move finger 2 towards finger 1.
  gestures.reset(Move(300, 300, kTouchId2));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_PINCH_UPDATE));

  // We should have zoomed out.
  EXPECT_LT(0.2, (*gestures)[0]->param_first());
  EXPECT_GT(0.5, (*gestures)[0]->param_first());
  // Release with finger 1
  gestures.reset(Release(101, 201, kTouchId1));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_PINCH_END));

  // Should still be able to scroll with one finger.
  gestures.reset(Move(25, 10, kTouchId2));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_SCROLL_UPDATE));

  EXPECT_TRUE(StatesAre(5,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_SCROLL,
                        GS_PINCH,
                        GS_SCROLL));
}

TEST_F(GestureSequenceTest, PinchFromScrollFromPinch) {
  Press(101, 201, kTouchId1);
  Press(10, 10, kTouchId2);
  // Pinch.
  Gestures gestures(Move(130, 230, kTouchId1));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_PINCH_UPDATE));

  // Release finger 1.
  gestures.reset(Release(101, 201, kTouchId1));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_PINCH_END));

  // Press finger one again.
  Press(101, 201, kTouchId1);
  // Pinch.
  gestures.reset(Move(130, 230, kTouchId1));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_PINCH_UPDATE));

  EXPECT_TRUE(StatesAre(5,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_PINCH,
                        GS_SCROLL,
                        GS_PINCH));
}

TEST_F(GestureSequenceTest, PinchFromTap) {
  Press(101, 201, kTouchId1);
  Press(10, 10, kTouchId2);

  // Move finger 1 away from finger 2.
  Gestures gestures(Move(400, 400, kTouchId1));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_PINCH_UPDATE));

  // We should have zoomed in.
  EXPECT_LT(2, (*gestures)[0]->param_first());
  EXPECT_GT(3, (*gestures)[0]->param_first());
  // Move finger 2 towards finger 1.
  gestures.reset(Move(300, 300, kTouchId2));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_PINCH_UPDATE));

  // We should have zoomed out.
  EXPECT_LT(0.2, (*gestures)[0]->param_first());
  EXPECT_GT(0.5, (*gestures)[0]->param_first());
  // Release with finger 1
  gestures.reset(Release(101, 201, kTouchId1));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_PINCH_END));

  // Should still be able to scroll with one finger.
  gestures.reset(Move(25, 10, kTouchId2));
  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_SCROLL_UPDATE));

  EXPECT_TRUE(StatesAre(4,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_PINCH,
                        GS_SCROLL));
}

TEST_F(GestureSequenceTest, PinchScroll) {
  Press(0, 0, kTouchId1);
  Press(100, 0, kTouchId2);

  // Pinch scroll events only occur once the total movement of the
  // users fingers exceed
  // GestureConfiguration::minimum_distance_for_pinch_scroll_in_pixels().

  // This will generate a series of events, all of which should be
  // scroll updates, but there will be some touch moves which don't
  // generate any events at all.
  Gestures gestures(new FakeGestures());
  for (int i = 0;
       i < GestureConfiguration::points_buffered_for_velocity();
       ++i) {
    FakeGestures* a = Move(0, 10 * i, kTouchId1);
    FakeGestures* b = Move(100, 10 * i, kTouchId2);
    gestures->insert(gestures->end(), a->begin(), a->end());
    gestures->insert(gestures->end(), b->begin(), b->end());
  }

  // All events were scroll updates.
  for (size_t i = 0; i < gestures->size(); i++)
    EXPECT_EQ((*gestures)[i]->type(), ET_GESTURE_SCROLL_UPDATE);

  // More than 1 scroll update event occurred.
  EXPECT_LT(1U, gestures->size());

  EXPECT_TRUE(StatesAre(3,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_PINCH));
}

TEST_F(GestureSequenceTest, IgnoreDisconnectedEvents) {
  Gestures gestures(Release(0, 0, kTouchId1));
  EXPECT_EQ(NULL, gestures.get());

  gestures.reset(Move(10, 10, kTouchId1));
  EXPECT_EQ(NULL, gestures.get());

  EXPECT_TRUE(StatesAre(1, GS_NO_GESTURE));
}

TEST_F(GestureSequenceTest, LongPress) {
  Press(0, 0, kTouchId1);

  EXPECT_FALSE(helper_.long_press_fired());
  gesture_sequence()->ForceTimeout();
  EXPECT_TRUE(helper_.long_press_fired());

  Release(0, 0, kTouchId1);

  EXPECT_TRUE(StatesAre(3,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_NO_GESTURE));
}

TEST_F(GestureSequenceTest, LongPressCancelledByScroll) {
  Press(0, 0, kTouchId1);
  Move(10, 10, kTouchId1);

  gesture_sequence()->ForceTimeout();
  EXPECT_FALSE(helper_.long_press_fired());

  EXPECT_TRUE(StatesAre(3,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_SCROLL));
}

TEST_F(GestureSequenceTest, LongPressCancelledByPinch) {
  Press(0, 0, kTouchId1);
  Press(10, 10, kTouchId2);

  gesture_sequence()->ForceTimeout();
  EXPECT_FALSE(helper_.long_press_fired());

  EXPECT_TRUE(StatesAre(3,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_PINCH));
}

TEST_F(GestureSequenceTest, DoubleTap) {
  Press(0, 0, kTouchId1);
  Release(0, 0, kTouchId1, 50);
  Press(0, 0, kTouchId1);

  Gestures gestures(Release(0, 0, kTouchId1, 50));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 2,
                            ET_GESTURE_TAP,
                            ET_GESTURE_DOUBLE_TAP));

  EXPECT_TRUE(StatesAre(5,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_NO_GESTURE));
}

TEST_F(GestureSequenceTest, OnlyDoubleTapIfClose) {
  Press(0, 0, kTouchId1);
  Release(0, 0, kTouchId1, 50);
  Press(0, 100, kTouchId1);

  Gestures gestures(Release(0, 100, kTouchId1, 50));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_TAP));

  EXPECT_TRUE(StatesAre(5,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_NO_GESTURE,
                        GS_PENDING_SYNTHETIC_CLICK,
                        GS_NO_GESTURE));
}

// Three Fingered Swipes always report a downward swipe on Win. crbug.com/124410
#if defined(OS_WIN)
#define MAYBE_ThreeFingeredSwipeRight DISABLED_ThreeFingeredSwipeRight
#define MAYBE_ThreeFingeredSwipeLeft DISABLED_ThreeFingeredSwipeLeft
#define MAYBE_ThreeFingeredSwipeUp DISABLED_ThreeFingeredSwipeUp
#define MAYBE_ThreeFingeredSwipeDown DISABLED_ThreeFingeredSwipeDown
#define MAYBE_ThreeFingeredSwipeDiagonal DISABLED_ThreeFingeredSwipeDiagonal
#define MAYBE_ThreeFingeredSwipeSpread DISABLED_ThreeFingeredSwipeSpread
#else
#define MAYBE_ThreeFingeredSwipeRight ThreeFingeredSwipeRight
#define MAYBE_ThreeFingeredSwipeLeft ThreeFingeredSwipeLeft
#define MAYBE_ThreeFingeredSwipeUp ThreeFingeredSwipeUp
#define MAYBE_ThreeFingeredSwipeDown ThreeFingeredSwipeDown
#define MAYBE_ThreeFingeredSwipeDiagonal ThreeFingeredSwipeDiagonal
#define MAYBE_ThreeFingeredSwipeSpread ThreeFingeredSwipeSpread
#endif

TEST_F(GestureSequenceTest, MAYBE_ThreeFingeredSwipeRight) {
  Gestures gestures(ThreeFingeredSwipe(kTouchId1, 10, 0,
                                       kTouchId2, 10, 0,
                                       kTouchId3, 10, 0));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_THREE_FINGER_SWIPE));

  EXPECT_EQ(1, (*gestures)[0]->param_first());
  EXPECT_EQ(0, (*gestures)[0]->param_second());
}

TEST_F(GestureSequenceTest, MAYBE_ThreeFingeredSwipeLeft) {
  Gestures gestures(ThreeFingeredSwipe(kTouchId1, -10, 0,
                                       kTouchId2, -10, 0,
                                       kTouchId3, -10, 0));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_THREE_FINGER_SWIPE));
  EXPECT_EQ(-1, (*gestures)[0]->param_first());
  EXPECT_EQ(0, (*gestures)[0]->param_second());
}

TEST_F(GestureSequenceTest, MAYBE_ThreeFingeredSwipeUp) {
  Gestures gestures(ThreeFingeredSwipe(kTouchId1, 0, 10,
                                       kTouchId2, 0, 10,
                                       kTouchId3, 0, 10));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_THREE_FINGER_SWIPE));
  EXPECT_EQ(0, (*gestures)[0]->param_first());
  EXPECT_EQ(1, (*gestures)[0]->param_second());
}

TEST_F(GestureSequenceTest, MAYBE_ThreeFingeredSwipeDown) {
  Gestures gestures(ThreeFingeredSwipe(kTouchId1, 0, -10,
                                       kTouchId2, 0, -10,
                                       kTouchId3, 0, -10));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 1,
                            ET_GESTURE_THREE_FINGER_SWIPE));
  EXPECT_EQ(0, (*gestures)[0]->param_first());
  EXPECT_EQ(-1, (*gestures)[0]->param_second());
}

TEST_F(GestureSequenceTest, MAYBE_ThreeFingeredSwipeDiagonal) {
  Gestures gestures(ThreeFingeredSwipe(kTouchId1, 10, 10,
                                       kTouchId2, 10, 10,
                                       kTouchId3, 10, 10));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 0));
}

TEST_F(GestureSequenceTest, MAYBE_ThreeFingeredSwipeSpread) {
  Gestures gestures(ThreeFingeredSwipe(kTouchId1, 10, 0,
                                       kTouchId2, 10, 0,
                                       kTouchId3, -10, 0));

  EXPECT_TRUE(EventTypesAre(gestures.get(), 0));
}

}  // namespace test
}  // namespace ui
