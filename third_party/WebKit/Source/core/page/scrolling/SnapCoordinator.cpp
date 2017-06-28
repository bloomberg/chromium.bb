// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SnapCoordinator.h"

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutView.h"
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

void SnapCoordinator::SnapAreaDidChange(LayoutBox& snap_area,
                                        ScrollSnapAlign scroll_snap_align) {
  if (scroll_snap_align.alignmentX == kSnapAlignmentNone &&
      scroll_snap_align.alignmentY == kSnapAlignmentNone) {
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
  if (scroll_snap_type.is_none) {
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
