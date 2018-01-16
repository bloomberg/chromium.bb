// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SnapCoordinator_h
#define SnapCoordinator_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "platform/heap/Handle.h"

namespace blink {

class LayoutBox;

// Snap Coordinator keeps track of snap containers and all of their associated
// snap areas. It also contains the logic to generate the list of valid snap
// positions for a given snap container.
//
// Snap container:
//   An scroll container that has 'scroll-snap-type' value other
//   than 'none'.
// Snap area:
//   A snap container's descendant that contributes snap positions. An element
//   only contributes snap positions to its nearest ancestor (on the element’s
//   containing block chain) scroll container.
//
// For more information see spec: https://drafts.csswg.org/css-snappoints/
class CORE_EXPORT SnapCoordinator final
    : public GarbageCollectedFinalized<SnapCoordinator> {
 public:
  static SnapCoordinator* Create();
  ~SnapCoordinator();
  void Trace(blink::Visitor* visitor) {}

  void SnapContainerDidChange(LayoutBox&, ScrollSnapType);
  void SnapAreaDidChange(LayoutBox&, ScrollSnapAlign);

  // Returns the SnapContainerData if the snap container has one.
  Optional<SnapContainerData> GetSnapContainerData(
      const LayoutBox& snap_container) const;

  // Calculate the SnapAreaData for the specific snap area in its snap
  // container.
  SnapAreaData CalculateSnapAreaData(const LayoutBox& snap_area,
                                     const LayoutBox& snap_container,
                                     const FloatPoint& max_position);

  // Called by LocalFrameView::PerformPostLayoutTasks(), so that the snap data
  // are updated whenever a layout happens.
  void UpdateAllSnapContainerData();
  void UpdateSnapContainerData(const LayoutBox&);

  // Called by ScrollManager::HandleGestureScrollEnd() to animate to the snap
  // position for the current scroll on the specific direction if there is
  // a valid snap position.
  void PerformSnapping(const LayoutBox& snap_container,
                       bool did_scroll_x,
                       bool did_scroll_y);
  bool GetSnapPosition(const LayoutBox& snap_container,
                       bool did_scroll_x,
                       bool did_scroll_y,
                       FloatPoint* snap_position);

#ifndef NDEBUG
  void ShowSnapAreaMap();
  void ShowSnapAreasFor(const LayoutBox*);
  void ShowSnapDataFor(const LayoutBox*);
#endif

 private:
  friend class SnapCoordinatorTest;
  explicit SnapCoordinator();

  HashMap<const LayoutBox*, SnapContainerData> snap_container_map_;
  DISALLOW_COPY_AND_ASSIGN(SnapCoordinator);
};

}  // namespace blink

#endif  // SnapCoordinator_h
