// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeScrollableArea_h
#define FakeScrollableArea_h

#include "platform/scheduler/child/web_scheduler.h"
#include "platform/scroll/ScrollableArea.h"
#include "public/platform/Platform.h"

namespace blink {

class FakeScrollableArea : public GarbageCollectedFinalized<FakeScrollableArea>,
                           public ScrollableArea {
  USING_GARBAGE_COLLECTED_MIXIN(FakeScrollableArea);

 public:
  static FakeScrollableArea* Create() { return new FakeScrollableArea; }

  CompositorElementId GetCompositorElementId() const override {
    return CompositorElementId();
  }
  bool IsActive() const override { return false; }
  int ScrollSize(ScrollbarOrientation) const override { return 100; }
  bool IsScrollCornerVisible() const override { return false; }
  IntRect ScrollCornerRect() const override { return IntRect(); }
  IntRect VisibleContentRect(
      IncludeScrollbarsInRect = kExcludeScrollbars) const override {
    return IntRect(ScrollOffsetInt().Width(), ScrollOffsetInt().Height(), 10,
                   10);
  }
  IntSize ContentsSize() const override { return IntSize(100, 100); }
  bool ScrollbarsCanBeActive() const override { return false; }
  IntRect ScrollableAreaBoundingBox() const override { return IntRect(); }
  void ScrollControlWasSetNeedsPaintInvalidation() override {}
  bool UserInputScrollable(ScrollbarOrientation) const override { return true; }
  bool ShouldPlaceVerticalScrollbarOnLeft() const override { return false; }
  int PageStep(ScrollbarOrientation) const override { return 0; }
  IntSize MinimumScrollOffsetInt() const override { return IntSize(); }
  IntSize MaximumScrollOffsetInt() const override {
    return ContentsSize() - IntSize(VisibleWidth(), VisibleHeight());
  }

  void UpdateScrollOffset(const ScrollOffset& offset, ScrollType) override {
    scroll_offset_ = offset;
  }
  ScrollOffset GetScrollOffset() const override { return scroll_offset_; }
  IntSize ScrollOffsetInt() const override {
    return FlooredIntSize(scroll_offset_);
  }

  RefPtr<WebTaskRunner> GetTimerTaskRunner() const final {
    return Platform::Current()->CurrentThread()->Scheduler()->TimerTaskRunner();
  }

  virtual void Trace(blink::Visitor* visitor) {
    ScrollableArea::Trace(visitor);
  }

 private:
  ScrollOffset scroll_offset_;
};

}  // namespace blink

#endif  // FakeScrollableArea
