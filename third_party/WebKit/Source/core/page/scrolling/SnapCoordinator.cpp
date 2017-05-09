// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SnapCoordinator.h"

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutView.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"

namespace blink {

SnapCoordinator::SnapCoordinator() : snap_containers_() {}

SnapCoordinator::~SnapCoordinator() {}

SnapCoordinator* SnapCoordinator::Create() {
  return new SnapCoordinator();
}

// Returns the scroll container that can be affected by this snap area.
static LayoutBox* FindSnapContainer(const LayoutBox& snap_area) {
  // According to the new spec
  // https://drafts.csswg.org/css-scroll-snap/#snap-model
  // "Snap positions must only affect the nearest ancestor (on the element’s
  // containing block chain) scroll container".
  Element* viewport_defining_element =
      snap_area.GetDocument().ViewportDefiningElement();
  LayoutBox* box = snap_area.ContainingBlock();
  while (box && !box->HasOverflowClip() && !box->IsLayoutView() &&
         box->GetNode() != viewport_defining_element)
    box = box->ContainingBlock();

  // If we reach to viewportDefiningElement then we dispatch to viewport
  if (box && box->GetNode() == viewport_defining_element)
    return snap_area.GetDocument().GetLayoutView();

  return box;
}

void SnapCoordinator::SnapAreaDidChange(
    LayoutBox& snap_area,
    const Vector<LengthPoint>& snap_coordinates) {
  if (snap_coordinates.IsEmpty()) {
    snap_area.SetSnapContainer(nullptr);
    return;
  }

  if (LayoutBox* snap_container = FindSnapContainer(snap_area)) {
    snap_area.SetSnapContainer(snap_container);
  } else {
    // TODO(majidvp): keep track of snap areas that do not have any
    // container so that we check them again when a new container is
    // added to the page.
  }
}

void SnapCoordinator::SnapContainerDidChange(LayoutBox& snap_container,
                                             ScrollSnapType scroll_snap_type) {
  if (scroll_snap_type == kScrollSnapTypeNone) {
    // TODO(majidvp): Track and report these removals to CompositorWorker
    // instance responsible for snapping
    snap_containers_.erase(&snap_container);
    snap_container.ClearSnapAreas();
  } else {
    snap_containers_.insert(&snap_container);
  }

  // TODO(majidvp): Add logic to correctly handle orphaned snap areas here.
  // 1. Removing container: find a new snap container for its orphan snap
  // areas (most likely nearest ancestor of current container) otherwise add
  // them to an orphan list.
  // 2. Adding container: may take over snap areas from nearest ancestor snap
  // container or from existing areas in orphan pool.
}

// Translate local snap coordinates into snap container's scrolling content
// coordinate space.
static Vector<FloatPoint> LocalToContainerSnapCoordinates(
    const LayoutBox& container_box,
    const LayoutBox& snap_area) {
  Vector<FloatPoint> result;
  LayoutPoint scroll_offset(container_box.ScrollLeft(),
                            container_box.ScrollTop());

  const Vector<LengthPoint>& snap_coordinates =
      snap_area.Style()->ScrollSnapCoordinate();
  for (auto& coordinate : snap_coordinates) {
    FloatPoint local_point =
        FloatPointForLengthPoint(coordinate, FloatSize(snap_area.Size()));
    FloatPoint container_point =
        snap_area.LocalToAncestorPoint(local_point, &container_box);
    container_point.MoveBy(scroll_offset);
    result.push_back(container_point);
  }
  return result;
}

Vector<double> SnapCoordinator::SnapOffsets(const ContainerNode& element,
                                            ScrollbarOrientation orientation) {
  const ComputedStyle* style = element.GetComputedStyle();
  const LayoutBox* snap_container = element.GetLayoutBox();
  DCHECK(style);
  DCHECK(snap_container);

  Vector<double> result;

  if (style->GetScrollSnapType() == kScrollSnapTypeNone)
    return result;

  const ScrollSnapPoints& snap_points = (orientation == kHorizontalScrollbar)
                                            ? style->ScrollSnapPointsX()
                                            : style->ScrollSnapPointsY();

  LayoutUnit client_size = (orientation == kHorizontalScrollbar)
                               ? snap_container->ClientWidth()
                               : snap_container->ClientHeight();
  LayoutUnit scroll_size = (orientation == kHorizontalScrollbar)
                               ? snap_container->ScrollWidth()
                               : snap_container->ScrollHeight();

  if (snap_points.has_repeat) {
    LayoutUnit repeat = ValueForLength(snap_points.repeat_offset, client_size);

    // calc() values may be negative or zero in which case we clamp them to 1px.
    // See: https://lists.w3.org/Archives/Public/www-style/2015Jul/0075.html
    repeat = std::max<LayoutUnit>(repeat, LayoutUnit(1));
    for (LayoutUnit offset = repeat; offset <= (scroll_size - client_size);
         offset += repeat) {
      result.push_back(offset.ToFloat());
    }
  }

  // Compute element-based snap points by mapping the snap coordinates from
  // snap areas to snap container.
  bool did_add_snap_area_offset = false;
  if (SnapAreaSet* snap_areas = snap_container->SnapAreas()) {
    for (auto& snap_area : *snap_areas) {
      Vector<FloatPoint> snap_coordinates =
          LocalToContainerSnapCoordinates(*snap_container, *snap_area);
      for (const FloatPoint& snap_coordinate : snap_coordinates) {
        float snap_offset = (orientation == kHorizontalScrollbar)
                                ? snap_coordinate.X()
                                : snap_coordinate.Y();
        if (snap_offset > scroll_size - client_size)
          continue;
        result.push_back(snap_offset);
        did_add_snap_area_offset = true;
      }
    }
  }

  if (did_add_snap_area_offset)
    std::sort(result.begin(), result.end());

  return result;
}

#ifndef NDEBUG

void SnapCoordinator::ShowSnapAreaMap() {
  for (auto& container : snap_containers_)
    ShowSnapAreasFor(container);
}

void SnapCoordinator::ShowSnapAreasFor(const LayoutBox* container) {
  LOG(INFO) << *container->GetNode();
  if (SnapAreaSet* snap_areas = container->SnapAreas()) {
    for (auto& snap_area : *snap_areas) {
      LOG(INFO) << "    " << *snap_area->GetNode();
    }
  }
}

#endif

}  // namespace blink
