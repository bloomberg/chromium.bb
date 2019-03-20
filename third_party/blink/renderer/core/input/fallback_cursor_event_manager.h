// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_FALLBACK_CURSOR_EVENT_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_FALLBACK_CURSOR_EVENT_MANAGER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class Node;
class IntPoint;
class IntSize;
class LocalFrame;
class ScrollableArea;
class WebMouseEvent;

class CORE_EXPORT FallbackCursorEventManager
    : public GarbageCollected<FallbackCursorEventManager> {
 public:
  FallbackCursorEventManager(LocalFrame&);
  void Trace(blink::Visitor*);

  void HandleMouseMoveEvent(const WebMouseEvent&);
  void HandleMousePressEvent(const WebMouseEvent&);

 private:
  FRIEND_TEST_ALL_PREFIXES(FallbackCursorEventManagerTest,
                           MouseMoveCursorLockOnDiv);
  FRIEND_TEST_ALL_PREFIXES(FallbackCursorEventManagerTest,
                           MouseMoveCursorLockOnIFrame);

  enum class Direction {
    kLeft,
    kRight,
    kUp,
    kDown,
  };

  void ResetCurrentScrollable();
  void InvalidateCurrentScrollableIfNeeded();
  LocalFrame* CurrentScrollingFrame();
  Node* CurrentScrollingNode();
  ScrollableArea* CurrentScrollingScrollableArea();

  bool ShouldLock(Direction d,
                  const ScrollableArea& scrollable,
                  const IntSize& node_size,
                  const IntPoint& cursor_location_in_node);
  void ComputeLockCursor(const IntPoint& location_in_root_frame);
  void LockCursor(bool left, bool right, bool up, bool down);
  void SetCursorVisibility(bool visible);

  const Member<LocalFrame> root_frame_;
  Member<Node> current_node_;

  DISALLOW_COPY_AND_ASSIGN(FallbackCursorEventManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_FALLBACK_CURSOR_EVENT_MANAGER_H_
