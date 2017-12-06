// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/SnapCoordinator.h"

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/LengthFunctions.h"
#include "platform/scroll/ScrollSnapData.h"

namespace blink {

SnapCoordinator::SnapCoordinator() : snap_container_map_() {}

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
  LayoutBox* old_container = snap_area.SnapContainer();
  if (scroll_snap_align.alignmentX == SnapAlignment::kNone &&
      scroll_snap_align.alignmentY == SnapAlignment::kNone) {
    snap_area.SetSnapContainer(nullptr);
    if (old_container)
      UpdateSnapContainerData(*old_container);
    return;
  }

  if (LayoutBox* new_container = FindSnapContainer(snap_area)) {
    snap_area.SetSnapContainer(new_container);
    // TODO(sunyunjia): consider keep the SnapAreas in a map so it is
    // easier to update.
    // TODO(sunyunjia): Only update when the localframe doesn't need layout.
    UpdateSnapContainerData(*new_container);
    if (old_container && old_container != new_container)
      UpdateSnapContainerData(*old_container);
  } else {
    // TODO(majidvp): keep track of snap areas that do not have any
    // container so that we check them again when a new container is
    // added to the page.
  }
}

void SnapCoordinator::UpdateAllSnapContainerData() {
  for (const auto& entry : snap_container_map_) {
    UpdateSnapContainerData(*entry.key);
  }
}

static ScrollableArea* ScrollableAreaForSnapping(const LayoutBox& layout_box) {
  return layout_box.IsLayoutView()
             ? layout_box.GetFrameView()->LayoutViewportScrollableArea()
             : layout_box.GetScrollableArea();
}

void SnapCoordinator::UpdateSnapContainerData(const LayoutBox& snap_container) {
  if (snap_container.Style()->GetScrollSnapType().is_none)
    return;

  SnapContainerData snap_container_data(
      snap_container.Style()->GetScrollSnapType());

  ScrollableArea* scrollable_area = ScrollableAreaForSnapping(snap_container);
  if (!scrollable_area)
    return;
  snap_container_data.min_offset = scrollable_area->MinimumScrollOffset();
  snap_container_data.max_offset = scrollable_area->MaximumScrollOffset();

  if (SnapAreaSet* snap_areas = snap_container.SnapAreas()) {
    for (const LayoutBox* snap_area : *snap_areas) {
      snap_container_data.AddSnapAreaData(CalculateSnapAreaData(
          *snap_area, snap_container, snap_container_data.min_offset,
          snap_container_data.max_offset));
    }
  }
  snap_container_map_.Set(&snap_container, snap_container_data);
}

static float ClipInContainer(LayoutUnit unit, float min, float max) {
  float value = unit.ToFloat();
  return value < min ? min : (value > max ? max : value);
}

// Returns scroll offset at which the snap area and snap containers meet the
// requested snapping alignment on the given axis.
// If the scroll offset required for the alignment is outside the valid range
// then it will be clamped.
// alignment - The scroll-snap-align specified on the snap area.
//    https://www.w3.org/TR/css-scroll-snap-1/#scroll-snap-align
// axis - The axis for which we consider alignment on. Should be either X or Y
// container - The snap_container rect relative to the container_element's
//    boundary. Note that this rect is represented by the dotted box below,
//    which is contracted by the scroll-padding from the element's original
//    boundary.
// scrollable_size - The maximal scrollable offset of the container. The
//    calculated snap_offset can not be larger than this value.
// area - The snap area rect relative to the snap container's boundary. Note
//    that this rect is represented by the dotted box below, which is expanded
//    by the scroll-snap-margin from the element's original boundary.
static float CalculateSnapOffset(SnapAlignment alignment,
                                 SnapAxis axis,
                                 const LayoutRect& container,
                                 const ScrollOffset& min_offset,
                                 const ScrollOffset& max_offset,
                                 const LayoutRect& area) {
  DCHECK(axis == SnapAxis::kX || axis == SnapAxis::kY);
  switch (alignment) {
    /* Start alignment aligns the area's start edge with container's start edge.
       https://www.w3.org/TR/css-scroll-snap-1/#valdef-scroll-snap-align-start
      + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
      +                   ^                                     +
      +                   |                                     +
      +                   |snap_offset                          +
      +                   |                                     +
      +                   v                                     +
      +  \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\  +
      +  \                 scroll-padding                    \  +
      +  \   . . . . . . . . . . . . . . . . . . . . . . .   \  +
      +  \   .       .   scroll-snap-margin  .           .   \  +
      +  \   .       .  |=================|  .           .   \  +
      +  \   .       .  |                 |  .           .   \  +
      +  \   .       .  |    snap_area    |  .           .   \  +
      +  \   .       .  |                 |  .           .   \  +
      +  \   .       .  |=================|  .           .   \  +
      +  \   .       .                       .           .   \  +
      +  \   .       . . . . . . . . . . . . .           .   \  +
      +  \   .                                           .   \  +
      +  \   .                                           .   \  +
      +  \   .                                           .   \  +
      +  \   . . . . . . .snap_container . . . . . . . . .   \  +
      +  \                                                   \  +
      +  \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\  +
      +                                                         +
      +                scrollable_content                       +
      + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +

    */
    case SnapAlignment::kStart:
      if (axis == SnapAxis::kX) {
        return ClipInContainer(area.X() - container.X(), min_offset.Width(),
                               max_offset.Width());
      }
      return ClipInContainer(area.Y() - container.Y(), min_offset.Height(),
                             max_offset.Height());

    /* Center alignment aligns the snap_area(with margin)'s center line with
       snap_container(without padding)'s center line.
       https://www.w3.org/TR/css-scroll-snap-1/#valdef-scroll-snap-align-center
      + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
      +                    ^                                    +
      +                    | snap_offset                        +
      +                    v                                    +
      +  \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\  +
      +  \                 scroll-padding                    \  +
      +  \   . . . . . . . . . . . . . . . . . . . . . . .   \  +
      +  \   .                                           .   \  +
      +  \   .       . . . . . . . . . . . . .           .   \  +
      +  \   .       .   scroll-snap-margin  .           .   \  +
      +  \   .       .  |=================|  .           .   \  +
      +  \   .       .  |    snap_area    |  .           .   \  +
      +  \* *.* * * *.* * * * * * * * * * * *.* * * * * * * * * * Center line
      +  \   .       .  |                 |  .           .   \  +
      +  \   .       .  |=================|  .           .   \  +
      +  \   .       .                       .           .   \  +
      +  \   .       . . . . . . . . . . . . .           .   \  +
      +  \   .                                           .   \  +
      +  \   . . . . . . snap_container. . . . . . . . . .   \  +
      +  \                                                   \  +
      +  \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\  +
      +                                                         +
      +                                                         +
      +                scrollable_content                       +
      +                                                         +
      + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +

    */
    case SnapAlignment::kCenter:
      if (axis == SnapAxis::kX) {
        return ClipInContainer(area.Center().X() - container.Center().X(),
                               min_offset.Width(), max_offset.Width());
      }
      return ClipInContainer(area.Center().Y() - container.Center().Y(),
                             min_offset.Height(), max_offset.Height());

    /* End alignment aligns the snap_area(with margin)'s end edge with
       snap_container(without padding)'s end edge.
       https://www.w3.org/TR/css-scroll-snap-1/#valdef-scroll-snap-align-end
      + + + + + + + + + + + + + + + + + + + + + + + + + + + + . .
      +                    ^                                    +
      +                    | snap_offset                        +
      +                    v                                    +
      +  \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\  +
      +  \                                                   \  +
      +  \   . . . . . . . . snap_container. . . . . . . .   \  +
      +  \   .                                           .   \  +
      +  \   .                                           .   \  +
      +  \   .                                           .   \  +
      +  \   .       . . . . . . . . . . . . .           .   \  +
      +  \   .       .   scroll-snap-margin  .           .   \  +
      +  \   .       .  |=================|  .           .   \  +
      +  \   .       .  |                 |  .           .   \  +
      +  \   .       .  |    snap_area    |  .           .   \  +
      +  \   .       .  |                 |  .           .   \  +
      +  \   .       .  |=================|  .           .   \  +
      +  \   .       .                       .           .   \  +
      +  \   . . . . . . . . . . . . . . . . . . . . . . .   \  +
      +  \               scroll-padding                      \  +
      +  \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\  +
      +                                                         +
      +               scrollable_content                        +
      +                                                         +
      + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +

    */
    case SnapAlignment::kEnd:
      if (axis == SnapAxis::kX) {
        return ClipInContainer(area.MaxX() - container.MaxX(),
                               min_offset.Width(), max_offset.Width());
      }
      return ClipInContainer(area.MaxY() - container.MaxY(),
                             min_offset.Height(), max_offset.Height());
    default:
      return LayoutUnit(SnapAreaData::kInvalidScrollOffset);
  }
}

SnapAreaData SnapCoordinator::CalculateSnapAreaData(
    const LayoutBox& snap_area,
    const LayoutBox& snap_container,
    const ScrollOffset& min_offset,
    const ScrollOffset& max_offset) {
  const ComputedStyle* container_style = snap_container.Style();
  const ComputedStyle* area_style = snap_area.Style();
  SnapAreaData snap_area_data;
  LayoutRect container(
      LayoutPoint(),
      LayoutSize(snap_container.OffsetWidth(), snap_container.OffsetHeight()));
  // We assume that the snap_container is the snap_area's ancestor in layout
  // tree, as the snap_container is found by walking up the layout tree in
  // FindSnapContainer(). Under this assumption,
  // snap_area.LocalToAncestorPoint(FloatPoint(0, 0), snap_container) returns
  // the snap_area's position relative to its container. And the |area| below
  // represents the snap_area rect in respect to the snap_container.
  Element* container_element;
  if (snap_container.GetNode()->IsElementNode())
    container_element = ToElement(snap_container.GetNode());
  else
    container_element = nullptr;
  // If the container_element is nullptr, which is when the container is
  // the root element, OffsetPoint() returns the distance between the canvas
  // origin and the left/top border edge of the snap_area.
  LayoutRect area(
      snap_area.OffsetPoint(container_element),
      LayoutSize(snap_area.OffsetWidth(), snap_area.OffsetHeight()));
  LayoutRectOutsets container_padding(
      // The percentage of scroll-padding is different from that of normal
      // padding, as scroll-padding resolves the percentage against
      // corresponding dimension of the scrollport[1], while the normal padding
      // resolves that against "width".[2,3]
      // We use MinimumValueForLength here to ensure kAuto is resolved to
      // LayoutUnit() which is the correct behavior for padding.
      // [1] https://drafts.csswg.org/css-scroll-snap-1/#scroll-padding
      //     "relative to the corresponding dimension of the scroll container’s
      //      scrollport"
      // [2] https://drafts.csswg.org/css-box/#padding-props
      // [3] See for example LayoutBoxModelObject::ComputedCSSPadding where it
      //     uses |MinimumValueForLength| but against the "width".
      MinimumValueForLength(container_style->ScrollPaddingTop(),
                            container.Height()),
      MinimumValueForLength(container_style->ScrollPaddingRight(),
                            container.Width()),
      MinimumValueForLength(container_style->ScrollPaddingBottom(),
                            container.Height()),
      MinimumValueForLength(container_style->ScrollPaddingLeft(),
                            container.Width()));
  LayoutRectOutsets area_margin(
      // According to spec
      // https://www.w3.org/TR/css-scroll-snap-1/#scroll-snap-margin
      // Only fixed values are valid inputs. So we would return 0 for percentage
      // values.
      // TODO(sunyunjia): Update CSS parser to reject percentage and auto values
      // for scroll-snap-margin, and reject auto values for scroll-padding.
      MinimumValueForLength(area_style->ScrollSnapMarginTop(), LayoutUnit()),
      MinimumValueForLength(area_style->ScrollSnapMarginRight(), LayoutUnit()),
      MinimumValueForLength(area_style->ScrollSnapMarginBottom(), LayoutUnit()),
      MinimumValueForLength(area_style->ScrollSnapMarginLeft(), LayoutUnit()));
  container.Contract(container_padding);
  area.Expand(area_margin);

  ScrollSnapAlign align = area_style->GetScrollSnapAlign();
  snap_area_data.snap_offset.SetWidth(CalculateSnapOffset(
      align.alignmentX, SnapAxis::kX, container, min_offset, max_offset, area));
  snap_area_data.snap_offset.SetHeight(CalculateSnapOffset(
      align.alignmentY, SnapAxis::kY, container, min_offset, max_offset, area));

  if (align.alignmentX != SnapAlignment::kNone &&
      align.alignmentY != SnapAlignment::kNone) {
    snap_area_data.snap_axis = SnapAxis::kBoth;
  } else if (align.alignmentX != SnapAlignment::kNone &&
             align.alignmentY == SnapAlignment::kNone) {
    snap_area_data.snap_axis = SnapAxis::kX;
  } else {
    snap_area_data.snap_axis = SnapAxis::kY;
  }

  snap_area_data.must_snap =
      (area_style->ScrollSnapStop() == EScrollSnapStop::kAlways);

  return snap_area_data;
}

ScrollOffset SnapCoordinator::FindSnapOffset(const ScrollOffset& current_offset,
                                             const SnapContainerData& data,
                                             bool should_snap_on_x,
                                             bool should_snap_on_y) {
  float smallest_distance_x = std::numeric_limits<float>::max();
  float smallest_distance_y = std::numeric_limits<float>::max();
  ScrollOffset snap_offset = current_offset;
  for (SnapAreaData snap_area_data : data.snap_area_list) {
    // TODO(sunyunjia): We should consider visiblity when choosing snap offset.
    if (should_snap_on_x && (snap_area_data.snap_axis == SnapAxis::kX ||
                             snap_area_data.snap_axis == SnapAxis::kBoth)) {
      float offset = snap_area_data.snap_offset.Width();
      if (offset == SnapAreaData::kInvalidScrollOffset)
        continue;
      float distance = std::abs(current_offset.Width() - offset);
      if (distance < smallest_distance_x) {
        smallest_distance_x = distance;
        snap_offset.SetWidth(offset);
      }
    }
    if (should_snap_on_y && (snap_area_data.snap_axis == SnapAxis::kY ||
                             snap_area_data.snap_axis == SnapAxis::kBoth)) {
      float offset = snap_area_data.snap_offset.Height();
      if (offset == SnapAreaData::kInvalidScrollOffset)
        continue;
      float distance = std::abs(current_offset.Height() - offset);
      if (distance < smallest_distance_y) {
        smallest_distance_y = distance;
        snap_offset.SetHeight(offset);
      }
    }
  }
  return snap_offset;
}

bool SnapCoordinator::GetSnapPosition(const LayoutBox& snap_container,
                                      bool did_scroll_x,
                                      bool did_scroll_y,
                                      ScrollOffset* snap_offset) {
  auto iter = snap_container_map_.find(&snap_container);
  if (iter == snap_container_map_.end())
    return false;

  SnapContainerData data = iter->value;
  if (!data.snap_area_list.size())
    return false;

  SnapAxis axis = data.scroll_snap_type.axis;
  did_scroll_x &= (axis == SnapAxis::kX || axis == SnapAxis::kBoth);
  did_scroll_y &= (axis == SnapAxis::kY || axis == SnapAxis::kBoth);

  ScrollableArea* scrollable_area = ScrollableAreaForSnapping(snap_container);
  if (!scrollable_area)
    return false;

  ScrollOffset current_scroll_offset;
  current_scroll_offset = scrollable_area->GetScrollOffset();

  *snap_offset =
      FindSnapOffset(current_scroll_offset, data, did_scroll_x, did_scroll_y);

  return *snap_offset != current_scroll_offset;
}

void SnapCoordinator::PerformSnapping(const LayoutBox& snap_container,
                                      bool did_scroll_x,
                                      bool did_scroll_y) {
  ScrollOffset snap_offset;
  if (GetSnapPosition(snap_container, did_scroll_x, did_scroll_y,
                      &snap_offset)) {
    if (ScrollableArea* scrollable_area =
            ScrollableAreaForSnapping(snap_container)) {
      scrollable_area->SetScrollOffset(snap_offset, kProgrammaticScroll,
                                       kScrollBehaviorSmooth);
    }
  }
}

void SnapCoordinator::SnapContainerDidChange(LayoutBox& snap_container,
                                             ScrollSnapType scroll_snap_type) {
  if (scroll_snap_type.is_none) {
    snap_container_map_.erase(&snap_container);
    snap_container.ClearSnapAreas();
  } else {
    if (scroll_snap_type.axis == SnapAxis::kInline) {
      if (snap_container.Style()->IsHorizontalWritingMode())
        scroll_snap_type.axis = SnapAxis::kX;
      else
        scroll_snap_type.axis = SnapAxis::kY;
    }
    if (scroll_snap_type.axis == SnapAxis::kBlock) {
      if (snap_container.Style()->IsHorizontalWritingMode())
        scroll_snap_type.axis = SnapAxis::kY;
      else
        scroll_snap_type.axis = SnapAxis::kX;
    }
    // TODO(sunyunjia): Only update when the localframe doesn't need layout.
    UpdateSnapContainerData(snap_container);
  }

  // TODO(majidvp): Add logic to correctly handle orphaned snap areas here.
  // 1. Removing container: find a new snap container for its orphan snap
  // areas (most likely nearest ancestor of current container) otherwise add
  // them to an orphan list.
  // 2. Adding container: may take over snap areas from nearest ancestor snap
  // container or from existing areas in orphan pool.
}

SnapContainerData SnapCoordinator::EnsureSnapContainerData(
    const LayoutBox& snap_container) {
  auto iter = snap_container_map_.find(&snap_container);
  if (iter != snap_container_map_.end()) {
    return iter->value;
  }
  return SnapContainerData();
}

#ifndef NDEBUG

void SnapCoordinator::ShowSnapAreaMap() {
  for (auto& container : snap_container_map_.Keys())
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

void SnapCoordinator::ShowSnapDataFor(const LayoutBox* snap_container) {
  auto iter = snap_container_map_.find(snap_container);
  if (iter == snap_container_map_.end())
    return;
  LOG(INFO) << iter->value;
}

#endif

}  // namespace blink
