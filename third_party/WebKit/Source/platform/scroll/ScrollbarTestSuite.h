// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollbarTestSuite_h
#define ScrollbarTestSuite_h

#include <memory>
#include "platform/heap/GarbageCollected.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/scroll/Scrollbar.h"
#include "platform/scroll/ScrollbarThemeMock.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blink {

class MockScrollableArea : public GarbageCollectedFinalized<MockScrollableArea>,
                           public ScrollableArea {
  USING_GARBAGE_COLLECTED_MIXIN(MockScrollableArea);

 public:
  static MockScrollableArea* Create() { return new MockScrollableArea(); }

  static MockScrollableArea* Create(const ScrollOffset& maximum_scroll_offset) {
    MockScrollableArea* mock = Create();
    mock->SetMaximumScrollOffset(maximum_scroll_offset);
    return mock;
  }

  MOCK_CONST_METHOD0(VisualRectForScrollbarParts, LayoutRect());
  MOCK_CONST_METHOD0(IsActive, bool());
  MOCK_CONST_METHOD1(ScrollSize, int(ScrollbarOrientation));
  MOCK_CONST_METHOD0(IsScrollCornerVisible, bool());
  MOCK_CONST_METHOD0(ScrollCornerRect, IntRect());
  MOCK_CONST_METHOD0(EnclosingScrollableArea, ScrollableArea*());
  MOCK_CONST_METHOD1(VisibleContentRect, IntRect(IncludeScrollbarsInRect));
  MOCK_CONST_METHOD0(ContentsSize, IntSize());
  MOCK_CONST_METHOD0(ScrollableAreaBoundingBox, IntRect());
  MOCK_CONST_METHOD0(LayerForHorizontalScrollbar, GraphicsLayer*());
  MOCK_CONST_METHOD0(LayerForVerticalScrollbar, GraphicsLayer*());
  MOCK_CONST_METHOD0(HorizontalScrollbar, Scrollbar*());
  MOCK_CONST_METHOD0(VerticalScrollbar, Scrollbar*());
  MOCK_CONST_METHOD0(ScrollbarsHidden, bool());

  bool UserInputScrollable(ScrollbarOrientation) const override { return true; }
  bool ScrollbarsCanBeActive() const override { return true; }
  bool ShouldPlaceVerticalScrollbarOnLeft() const override { return false; }
  void UpdateScrollOffset(const ScrollOffset& offset, ScrollType) override {
    scroll_offset_ = offset.ShrunkTo(maximum_scroll_offset_);
  }
  IntSize ScrollOffsetInt() const override {
    return FlooredIntSize(scroll_offset_);
  }
  IntSize MinimumScrollOffsetInt() const override { return IntSize(); }
  IntSize MaximumScrollOffsetInt() const override {
    return ExpandedIntSize(maximum_scroll_offset_);
  }
  int VisibleHeight() const override { return 768; }
  int VisibleWidth() const override { return 1024; }
  CompositorElementId GetCompositorElementId() const override {
    return CompositorElementId();
  }
  bool ScrollAnimatorEnabled() const override { return false; }
  int PageStep(ScrollbarOrientation) const override { return 0; }
  void ScrollControlWasSetNeedsPaintInvalidation() {}
  void SetScrollOrigin(const IntPoint& origin) {
    ScrollableArea::SetScrollOrigin(origin);
  }

  RefPtr<WebTaskRunner> GetTimerTaskRunner() const final {
    return Platform::Current()->CurrentThread()->Scheduler()->TimerTaskRunner();
  }

  using ScrollableArea::HorizontalScrollbarNeedsPaintInvalidation;
  using ScrollableArea::VerticalScrollbarNeedsPaintInvalidation;
  using ScrollableArea::ClearNeedsPaintInvalidationForScrollControls;

  DEFINE_INLINE_VIRTUAL_TRACE() { ScrollableArea::Trace(visitor); }

 protected:
  explicit MockScrollableArea()
      : maximum_scroll_offset_(ScrollOffset(0, 100)) {}
  explicit MockScrollableArea(const ScrollOffset& offset)
      : maximum_scroll_offset_(offset) {}

 private:
  void SetMaximumScrollOffset(const ScrollOffset& maximum_scroll_offset) {
    maximum_scroll_offset_ = maximum_scroll_offset;
  }

  ScrollOffset scroll_offset_;
  ScrollOffset maximum_scroll_offset_;
};

}  // namespace blink

#endif
