// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_node.h"

#include "third_party/blink/renderer/core/layout/ng/grid/layout_ng_grid.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_placement.h"

namespace blink {

const NGGridPlacementData& NGGridNode::CachedPlacementData() const {
  auto* layout_grid = To<LayoutNGGrid>(box_.Get());
  return layout_grid->CachedPlacementData();
}

GridItems NGGridNode::ConstructGridItems(
    NGGridPlacementData* placement_data) const {
  DCHECK(placement_data);

  GridItems grid_items;
  auto* layout_grid = To<LayoutNGGrid>(box_.Get());
  const NGGridPlacementData* cached_placement_data = nullptr;

  if (!layout_grid->IsGridPlacementDirty()) {
    cached_placement_data = &layout_grid->CachedPlacementData();

    // Even if the cached placement data is incorrect, as long as the grid is
    // not marked as dirty, the grid item count should be the same.
    grid_items.ReserveCapacity(
        cached_placement_data->grid_item_positions.size());

    if (placement_data->column_auto_repetitions !=
            cached_placement_data->column_auto_repetitions ||
        placement_data->row_auto_repetitions !=
            cached_placement_data->row_auto_repetitions) {
      // We need to recompute grid item placement if the automatic column/row
      // repetitions changed due to updates in the container's style.
      cached_placement_data = nullptr;
    }
  }

  const auto& container_style = Style();
  bool should_sort_grid_items_by_order_property = false;
  const int initial_order = ComputedStyleInitialValues::InitialOrder();

  for (auto child = FirstChild(); child; child = child.NextSibling()) {
    GridItemData grid_item(To<NGBlockNode>(child), container_style,
                           container_style.GetWritingMode());

    // Order all of our in-flow children by their order property.
    if (!grid_item.IsOutOfFlow()) {
      should_sort_grid_items_by_order_property |=
          child.Style().Order() != initial_order;
      grid_items.Append(std::move(grid_item));
    }
  }

  // We only need to sort this when we encounter a non-initial order property.
  if (should_sort_grid_items_by_order_property) {
    auto CompareItemsByOrderProperty = [](const GridItemData& lhs,
                                          const GridItemData& rhs) {
      return lhs.node.Style().Order() < rhs.node.Style().Order();
    };
    std::stable_sort(grid_items.item_data.begin(), grid_items.item_data.end(),
                     CompareItemsByOrderProperty);
  }

#if DCHECK_IS_ON()
  if (cached_placement_data) {
    NGGridPlacement grid_placement(container_style, *placement_data);
    DCHECK(*cached_placement_data ==
           grid_placement.RunAutoPlacementAlgorithm(grid_items));
  }
#endif

  if (!cached_placement_data) {
    NGGridPlacement grid_placement(container_style, *placement_data);
    layout_grid->SetCachedPlacementData(
        grid_placement.RunAutoPlacementAlgorithm(grid_items));
    cached_placement_data = &layout_grid->CachedPlacementData();
  }

  // The provided |placement_data| needs to be updated since the start offsets
  // were either computed by the auto-placement algorithm or cached.
  placement_data->column_start_offset =
      cached_placement_data->column_start_offset;
  placement_data->row_start_offset = cached_placement_data->row_start_offset;

  // Copy each resolved position to its respective grid item data.
  auto* resolved_position = cached_placement_data->grid_item_positions.begin();
  for (auto& grid_item : grid_items.item_data)
    grid_item.resolved_position = *(resolved_position++);
  return grid_items;
}

GridItems NGGridNode::GridItemsIncludingSubgridded(
    NGGridPlacementData* placement_data) const {
  auto grid_items = ConstructGridItems(placement_data);

  const bool has_standalone_columns =
      placement_data->HasStandalonePlacement(kForColumns);
  const bool has_standalone_rows =
      placement_data->HasStandalonePlacement(kForRows);

  // Easy optimization: return early if this grid container is subgridded in
  // both axes and will not append any subgridded items anyway.
  if (!has_standalone_columns && !has_standalone_rows)
    return grid_items;

  for (wtf_size_t i = 0; i < grid_items.Size(); ++i) {
    const auto& current_item = grid_items.item_data[i];

    if (!current_item.node.IsGrid())
      continue;

    // TODO(ethavar): Don't consider subgrids with size containment.

    const auto subgrid = To<NGGridNode>(current_item.node);
    NGGridPlacementData subgrid_placement_data(
        /* is_parent_grid_container */ true);

    bool should_append_subgridded_items = false;
    const bool is_parallel_subgrid = IsParallelWritingMode(
        Style().GetWritingMode(), subgrid.Style().GetWritingMode());

    {
      const auto relative_column_direction =
          is_parallel_subgrid ? kForColumns : kForRows;

      if (current_item.HasSubgriddedAxis(relative_column_direction)) {
        subgrid_placement_data.SetSubgridSpanSize(
            current_item.SpanSize(kForColumns), relative_column_direction);
        should_append_subgridded_items |= has_standalone_columns;
      }

      const auto relative_row_direction =
          is_parallel_subgrid ? kForRows : kForColumns;

      if (current_item.HasSubgriddedAxis(relative_row_direction)) {
        subgrid_placement_data.SetSubgridSpanSize(
            current_item.SpanSize(kForRows), relative_row_direction);
        should_append_subgridded_items |= has_standalone_rows;
      }
    }

    if (!should_append_subgridded_items)
      continue;

    // TODO(ethavar): Compute automatic repetitions for subgridded axes as
    // described in https://drafts.csswg.org/css-grid-2/#auto-repeat.

    auto subgridded_items = subgrid.ConstructGridItems(&subgrid_placement_data);

    const wtf_size_t column_start_line = current_item.StartLine(kForColumns);
    const wtf_size_t row_start_line = current_item.StartLine(kForRows);

    for (auto grid_item : subgridded_items.item_data) {
      grid_item.is_subgridded_to_parent_grid = true;

      if (!is_parallel_subgrid) {
        std::swap(grid_item.resolved_position.columns,
                  grid_item.resolved_position.rows);
      }

      grid_item.resolved_position.columns.Translate(column_start_line);
      grid_item.resolved_position.rows.Translate(row_start_line);
      grid_items.Append(std::move(grid_item));
    }
  }
  return grid_items;
}

}  // namespace blink
