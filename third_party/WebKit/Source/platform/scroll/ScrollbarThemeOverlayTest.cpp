// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scroll/ScrollbarThemeOverlay.h"

#include "platform/scroll/ScrollbarTestSuite.h"
#include "platform/testing/TestingPlatformSupport.h"

namespace blink {

using testing::NiceMock;
using testing::Return;

using ScrollbarThemeOverlayTest = testing::Test;

TEST_F(ScrollbarThemeOverlayTest, PaintInvalidation) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  NiceMock<MockScrollableArea>* mockScrollableArea =
      new NiceMock<MockScrollableArea>(ScrollOffset(100, 100));
  ScrollbarThemeOverlay theme(14, 0, ScrollbarThemeOverlay::AllowHitTest);

  Scrollbar* verticalScrollbar = Scrollbar::createForTesting(
      mockScrollableArea, VerticalScrollbar, RegularScrollbar, &theme);
  Scrollbar* horizontalScrollbar = Scrollbar::createForTesting(
      mockScrollableArea, HorizontalScrollbar, RegularScrollbar, &theme);
  ON_CALL(*mockScrollableArea, verticalScrollbar())
      .WillByDefault(Return(verticalScrollbar));
  ON_CALL(*mockScrollableArea, horizontalScrollbar())
      .WillByDefault(Return(horizontalScrollbar));

  IntRect verticalRect(1010, 0, 14, 768);
  IntRect horizontalRect(0, 754, 1024, 14);
  verticalScrollbar->setFrameRect(verticalRect);
  horizontalScrollbar->setFrameRect(horizontalRect);

  ASSERT_EQ(verticalScrollbar, mockScrollableArea->verticalScrollbar());
  ASSERT_EQ(horizontalScrollbar, mockScrollableArea->horizontalScrollbar());

  verticalScrollbar->clearTrackNeedsRepaint();
  verticalScrollbar->clearThumbNeedsRepaint();
  horizontalScrollbar->clearTrackNeedsRepaint();
  horizontalScrollbar->clearThumbNeedsRepaint();
  mockScrollableArea->clearNeedsPaintInvalidationForScrollControls();

  ASSERT_FALSE(verticalScrollbar->thumbNeedsRepaint());
  ASSERT_FALSE(verticalScrollbar->trackNeedsRepaint());
  ASSERT_FALSE(mockScrollableArea->verticalScrollbarNeedsPaintInvalidation());
  ASSERT_FALSE(horizontalScrollbar->thumbNeedsRepaint());
  ASSERT_FALSE(horizontalScrollbar->trackNeedsRepaint());
  ASSERT_FALSE(mockScrollableArea->horizontalScrollbarNeedsPaintInvalidation());

  // Changing the scroll offset shouldn't invalide the thumb nor track, but it
  // should cause a "general" invalidation for non-composited scrollbars.
  // Ensure the horizontal scrollbar is unaffected.
  mockScrollableArea->updateScrollOffset(ScrollOffset(0, 5), UserScroll);
  verticalScrollbar->offsetDidChange();
  horizontalScrollbar->offsetDidChange();
  EXPECT_FALSE(verticalScrollbar->thumbNeedsRepaint());
  EXPECT_FALSE(verticalScrollbar->trackNeedsRepaint());
  EXPECT_TRUE(mockScrollableArea->verticalScrollbarNeedsPaintInvalidation());
  EXPECT_FALSE(horizontalScrollbar->thumbNeedsRepaint());
  EXPECT_FALSE(horizontalScrollbar->trackNeedsRepaint());
  EXPECT_FALSE(mockScrollableArea->horizontalScrollbarNeedsPaintInvalidation());

  // Try the horizontal scrollbar.
  mockScrollableArea->clearNeedsPaintInvalidationForScrollControls();
  mockScrollableArea->updateScrollOffset(ScrollOffset(5, 5), UserScroll);
  horizontalScrollbar->offsetDidChange();
  verticalScrollbar->offsetDidChange();
  EXPECT_FALSE(verticalScrollbar->thumbNeedsRepaint());
  EXPECT_FALSE(verticalScrollbar->trackNeedsRepaint());
  EXPECT_FALSE(mockScrollableArea->verticalScrollbarNeedsPaintInvalidation());
  EXPECT_FALSE(horizontalScrollbar->thumbNeedsRepaint());
  EXPECT_FALSE(horizontalScrollbar->trackNeedsRepaint());
  EXPECT_TRUE(mockScrollableArea->horizontalScrollbarNeedsPaintInvalidation());

  mockScrollableArea->clearNeedsPaintInvalidationForScrollControls();

  // Move the mouse over the vertical scrollbar's thumb. Ensure the thumb is
  // invalidated as its state is changed to hover.
  verticalScrollbar->setHoveredPart(ThumbPart);
  EXPECT_TRUE(verticalScrollbar->thumbNeedsRepaint());
  EXPECT_TRUE(mockScrollableArea->verticalScrollbarNeedsPaintInvalidation());

  verticalScrollbar->clearThumbNeedsRepaint();
  mockScrollableArea->clearNeedsPaintInvalidationForScrollControls();

  // Pressing down should also cause an invalidation.
  verticalScrollbar->setPressedPart(ThumbPart);
  EXPECT_TRUE(verticalScrollbar->thumbNeedsRepaint());
  EXPECT_TRUE(mockScrollableArea->verticalScrollbarNeedsPaintInvalidation());

  verticalScrollbar->clearThumbNeedsRepaint();
  mockScrollableArea->clearNeedsPaintInvalidationForScrollControls();

  // Release should cause invalidation.
  verticalScrollbar->setPressedPart(NoPart);
  EXPECT_TRUE(verticalScrollbar->thumbNeedsRepaint());
  EXPECT_TRUE(mockScrollableArea->verticalScrollbarNeedsPaintInvalidation());

  verticalScrollbar->clearThumbNeedsRepaint();
  mockScrollableArea->clearNeedsPaintInvalidationForScrollControls();

  // Move off should cause invalidation
  verticalScrollbar->setHoveredPart(NoPart);
  EXPECT_TRUE(verticalScrollbar->thumbNeedsRepaint());
  EXPECT_TRUE(mockScrollableArea->verticalScrollbarNeedsPaintInvalidation());

  verticalScrollbar->clearThumbNeedsRepaint();
  mockScrollableArea->clearNeedsPaintInvalidationForScrollControls();

  // Disabling the scrollbar is used to hide it so it should cause invalidation
  // but only in the general sense since the compositor will have hidden it
  // without a repaint.
  verticalScrollbar->setEnabled(false);
  EXPECT_FALSE(verticalScrollbar->thumbNeedsRepaint());
  EXPECT_TRUE(mockScrollableArea->verticalScrollbarNeedsPaintInvalidation());

  mockScrollableArea->clearNeedsPaintInvalidationForScrollControls();

  verticalScrollbar->setEnabled(true);
  EXPECT_FALSE(verticalScrollbar->thumbNeedsRepaint());
  EXPECT_TRUE(mockScrollableArea->verticalScrollbarNeedsPaintInvalidation());

  ThreadState::current()->collectAllGarbage();
}

}  // namespace blink
