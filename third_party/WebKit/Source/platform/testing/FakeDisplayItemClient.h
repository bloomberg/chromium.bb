// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeDisplayItemClient_h
#define FakeDisplayItemClient_h

#include "platform/geometry/LayoutRect.h"
#include "platform/wtf/Forward.h"

namespace blink {

// A simple DisplayItemClient implementation suitable for use in unit tests.
class FakeDisplayItemClient : public DisplayItemClient {
 public:
  FakeDisplayItemClient(const String& name = "FakeDisplayItemClient",
                        const LayoutRect& visual_rect = LayoutRect())
      : name_(name), visual_rect_(visual_rect) {}

  String DebugName() const final { return name_; }
  LayoutRect VisualRect() const override { return visual_rect_; }
  LayoutRect PartialInvalidationRect() const override {
    return partial_invalidation_rect_;
  }
  void ClearPartialInvalidationRect() const override {
    partial_invalidation_rect_ = LayoutRect();
  }

  void SetVisualRect(const LayoutRect& r) { visual_rect_ = r; }
  void SetPartialInvalidationRect(const LayoutRect& r) {
    SetDisplayItemsUncached(PaintInvalidationReason::kRectangle);
    partial_invalidation_rect_ = r;
  }

  // This simulates a paint without needing a PaintController.
  void UpdateCacheGeneration() {
    SetDisplayItemsCached(CacheGenerationOrInvalidationReason::Next());
  }

 private:
  String name_;
  LayoutRect visual_rect_;
  mutable LayoutRect partial_invalidation_rect_;
};

}  // namespace blink

#endif  // FakeDisplayItemClient_h
