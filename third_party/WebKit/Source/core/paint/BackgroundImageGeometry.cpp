// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BackgroundImageGeometry.h"

#include "core/frame/LocalFrameView.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableCol.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/paint/PaintLayer.h"
#include "platform/LayoutUnit.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

namespace {

// Return the amount of space to leave between image tiles for the
// background-repeat: space property.
inline LayoutUnit GetSpaceBetweenImageTiles(LayoutUnit area_size,
                                            LayoutUnit tile_size) {
  int number_of_tiles = (area_size / tile_size).ToInt();
  LayoutUnit space(-1);

  if (number_of_tiles > 1) {
    // Spec doesn't specify rounding, so use the same method as for
    // background-repeat: round.
    space = (area_size - number_of_tiles * tile_size) / (number_of_tiles - 1);
  }

  return space;
}

bool FixedBackgroundPaintsInLocalCoordinates(
    const LayoutObject& obj,
    const GlobalPaintFlags global_paint_flags) {
  if (!obj.IsLayoutView())
    return false;

  const LayoutView& view = ToLayoutView(obj);

  if (global_paint_flags & kGlobalPaintFlattenCompositingLayers)
    return false;

  PaintLayer* root_layer = view.Layer();
  if (!root_layer || root_layer->GetCompositingState() == kNotComposited)
    return false;

  return root_layer->GetCompositedLayerMapping()
      ->BackgroundLayerPaintsFixedRootBackground();
}

LayoutSize CalculateFillTileSize(const LayoutBoxModelObject& obj,
                                 const FillLayer& fill_layer,
                                 const LayoutSize& positioning_area_size) {
  StyleImage* image = fill_layer.GetImage();
  EFillSizeType type = fill_layer.Size().type;

  LayoutSize image_intrinsic_size = image->ImageSize(
      obj.GetDocument(), obj.Style()->EffectiveZoom(), positioning_area_size);
  switch (type) {
    case kSizeLength: {
      LayoutSize tile_size(positioning_area_size);

      Length layer_width = fill_layer.Size().size.Width();
      Length layer_height = fill_layer.Size().size.Height();

      if (layer_width.IsFixed())
        tile_size.SetWidth(LayoutUnit(layer_width.Value()));
      else if (layer_width.IsPercentOrCalc())
        tile_size.SetWidth(
            ValueForLength(layer_width, positioning_area_size.Width()));

      if (layer_height.IsFixed())
        tile_size.SetHeight(LayoutUnit(layer_height.Value()));
      else if (layer_height.IsPercentOrCalc())
        tile_size.SetHeight(
            ValueForLength(layer_height, positioning_area_size.Height()));

      // If one of the values is auto we have to use the appropriate
      // scale to maintain our aspect ratio.
      if (layer_width.IsAuto() && !layer_height.IsAuto()) {
        if (image_intrinsic_size.Height()) {
          LayoutUnit adjusted_width = image_intrinsic_size.Width() *
                                      tile_size.Height() /
                                      image_intrinsic_size.Height();
          if (image_intrinsic_size.Width() >= 1 && adjusted_width < 1)
            adjusted_width = LayoutUnit(1);
          tile_size.SetWidth(adjusted_width);
        }
      } else if (!layer_width.IsAuto() && layer_height.IsAuto()) {
        if (image_intrinsic_size.Width()) {
          LayoutUnit adjusted_height = image_intrinsic_size.Height() *
                                       tile_size.Width() /
                                       image_intrinsic_size.Width();
          if (image_intrinsic_size.Height() >= 1 && adjusted_height < 1)
            adjusted_height = LayoutUnit(1);
          tile_size.SetHeight(adjusted_height);
        }
      } else if (layer_width.IsAuto() && layer_height.IsAuto()) {
        // If both width and height are auto, use the image's intrinsic size.
        tile_size = image_intrinsic_size;
      }

      tile_size.ClampNegativeToZero();
      return tile_size;
    }
    case kSizeNone: {
      // If both values are 'auto' then the intrinsic width and/or height of the
      // image should be used, if any.
      if (!image_intrinsic_size.IsEmpty())
        return image_intrinsic_size;

      // If the image has neither an intrinsic width nor an intrinsic height,
      // its size is determined as for 'contain'.
      type = kContain;
    }
    case kContain:
    case kCover: {
      float horizontal_scale_factor =
          image_intrinsic_size.Width()
              ? positioning_area_size.Width().ToFloat() /
                    image_intrinsic_size.Width()
              : 1.0f;
      float vertical_scale_factor =
          image_intrinsic_size.Height()
              ? positioning_area_size.Height().ToFloat() /
                    image_intrinsic_size.Height()
              : 1.0f;
      // Force the dimension that determines the size to exactly match the
      // positioningAreaSize in that dimension, so that rounding of floating
      // point approximation to LayoutUnit do not shrink the image to smaller
      // than the positioningAreaSize.
      if (type == kContain) {
        if (horizontal_scale_factor < vertical_scale_factor)
          return LayoutSize(
              positioning_area_size.Width(),
              LayoutUnit(std::max(1.0f, image_intrinsic_size.Height() *
                                            horizontal_scale_factor)));
        return LayoutSize(
            LayoutUnit(std::max(
                1.0f, image_intrinsic_size.Width() * vertical_scale_factor)),
            positioning_area_size.Height());
      }
      if (horizontal_scale_factor > vertical_scale_factor)
        return LayoutSize(
            positioning_area_size.Width(),
            LayoutUnit(std::max(1.0f, image_intrinsic_size.Height() *
                                          horizontal_scale_factor)));
      return LayoutSize(LayoutUnit(std::max(1.0f, image_intrinsic_size.Width() *
                                                      vertical_scale_factor)),
                        positioning_area_size.Height());
    }
  }

  NOTREACHED();
  return LayoutSize();
}

IntPoint AccumulatedScrollOffsetForFixedBackground(
    const LayoutBoxModelObject& object,
    const LayoutBoxModelObject* container) {
  IntPoint result;
  if (&object == container)
    return result;
  for (const LayoutBlock* block = object.ContainingBlock(); block;
       block = block->ContainingBlock()) {
    if (block->HasOverflowClip())
      result += block->ScrolledContentOffset();
    if (block == container)
      break;
  }
  return result;
}

// When we match the sub-pixel fraction of the destination rect in a dimension,
// we snap the same way. This commonly occurs when the background is meant to
// fill the padding box but there's a border (which in Blink is always stored as
// an integer).  Otherwise we floor to avoid growing our tile size. Often these
// tiles are from a sprite map, and bleeding adjacent sprites is visually worse
// than clipping the intended one.
LayoutSize ApplySubPixelHeuristicToImageSize(const LayoutSize& size,
                                             const LayoutRect& destination) {
  LayoutSize snapped_size =
      LayoutSize(size.Width().Fraction() == destination.Width().Fraction()
                     ? SnapSizeToPixel(size.Width(), destination.X())
                     : size.Width().Floor(),
                 size.Height().Fraction() == destination.Height().Fraction()
                     ? SnapSizeToPixel(size.Height(), destination.Y())
                     : size.Height().Floor());
  return snapped_size;
}

}  // anonymous namespace

void BackgroundImageGeometry::SetNoRepeatX(LayoutUnit x_offset) {
  int rounded_offset = RoundToInt(x_offset);
  dest_rect_.Move(std::max(rounded_offset, 0), 0);
  SetPhaseX(LayoutUnit(-std::min(rounded_offset, 0)));
  dest_rect_.SetWidth(tile_size_.Width() + std::min(rounded_offset, 0));
  SetSpaceSize(LayoutSize(LayoutUnit(), SpaceSize().Height()));
}

void BackgroundImageGeometry::SetNoRepeatY(LayoutUnit y_offset) {
  int rounded_offset = RoundToInt(y_offset);
  dest_rect_.Move(0, std::max(rounded_offset, 0));
  SetPhaseY(LayoutUnit(-std::min(rounded_offset, 0)));
  dest_rect_.SetHeight(tile_size_.Height() + std::min(rounded_offset, 0));
  SetSpaceSize(LayoutSize(SpaceSize().Width(), LayoutUnit()));
}

void BackgroundImageGeometry::SetRepeatX(const FillLayer& fill_layer,
                                         LayoutUnit unsnapped_tile_width,
                                         LayoutUnit snapped_available_width,
                                         LayoutUnit unsnapped_available_width,
                                         LayoutUnit extra_offset,
                                         LayoutUnit offset_for_cell) {
  // We would like to identify the phase as a fraction of the image size in the
  // absence of snapping, then re-apply it to the snapped values. This is to
  // handle large positions.
  if (unsnapped_tile_width) {
    LayoutUnit computed_x_position =
        RoundedMinimumValueForLength(fill_layer.XPosition(),
                                     unsnapped_available_width) -
        offset_for_cell;
    float number_of_tiles_in_position;
    if (fill_layer.BackgroundXOrigin() == kRightEdge) {
      number_of_tiles_in_position =
          (snapped_available_width - computed_x_position + extra_offset)
              .ToFloat() /
          unsnapped_tile_width.ToFloat();
    } else {
      number_of_tiles_in_position =
          (computed_x_position + extra_offset).ToFloat() /
          unsnapped_tile_width.ToFloat();
    }
    float fractional_position_within_tile =
        1.0f -
        (number_of_tiles_in_position - truncf(number_of_tiles_in_position));
    SetPhaseX(LayoutUnit(
        roundf(fractional_position_within_tile * TileSize().Width())));
  } else {
    SetPhaseX(LayoutUnit());
  }
  SetSpaceSize(LayoutSize(LayoutUnit(), SpaceSize().Height()));
}

void BackgroundImageGeometry::SetRepeatY(const FillLayer& fill_layer,
                                         LayoutUnit unsnapped_tile_height,
                                         LayoutUnit snapped_available_height,
                                         LayoutUnit unsnapped_available_height,
                                         LayoutUnit extra_offset,
                                         LayoutUnit offset_for_cell) {
  // We would like to identify the phase as a fraction of the image size in the
  // absence of snapping, then re-apply it to the snapped values. This is to
  // handle large positions.
  if (unsnapped_tile_height) {
    LayoutUnit computed_y_position =
        RoundedMinimumValueForLength(fill_layer.YPosition(),
                                     unsnapped_available_height) -
        offset_for_cell;
    float number_of_tiles_in_position;
    if (fill_layer.BackgroundYOrigin() == kBottomEdge) {
      number_of_tiles_in_position =
          (snapped_available_height - computed_y_position + extra_offset)
              .ToFloat() /
          unsnapped_tile_height.ToFloat();
    } else {
      number_of_tiles_in_position =
          (computed_y_position + extra_offset).ToFloat() /
          unsnapped_tile_height.ToFloat();
    }
    float fractional_position_within_tile =
        1.0f -
        (number_of_tiles_in_position - truncf(number_of_tiles_in_position));
    SetPhaseY(LayoutUnit(
        roundf(fractional_position_within_tile * TileSize().Height())));
  } else {
    SetPhaseY(LayoutUnit());
  }
  SetSpaceSize(LayoutSize(SpaceSize().Width(), LayoutUnit()));
}

void BackgroundImageGeometry::SetSpaceX(LayoutUnit space,
                                        LayoutUnit available_width,
                                        LayoutUnit extra_offset) {
  LayoutUnit computed_x_position =
      RoundedMinimumValueForLength(Length(), available_width);
  SetSpaceSize(LayoutSize(space.Round(), SpaceSize().Height().ToInt()));
  LayoutUnit actual_width = TileSize().Width() + space;
  SetPhaseX(actual_width
                ? LayoutUnit(roundf(actual_width -
                                    fmodf((computed_x_position + extra_offset),
                                          actual_width)))
                : LayoutUnit());
}

void BackgroundImageGeometry::SetSpaceY(LayoutUnit space,
                                        LayoutUnit available_height,
                                        LayoutUnit extra_offset) {
  LayoutUnit computed_y_position =
      RoundedMinimumValueForLength(Length(), available_height);
  SetSpaceSize(LayoutSize(SpaceSize().Width().ToInt(), space.Round()));
  LayoutUnit actual_height = TileSize().Height() + space;
  SetPhaseY(actual_height
                ? LayoutUnit(roundf(actual_height -
                                    fmodf((computed_y_position + extra_offset),
                                          actual_height)))
                : LayoutUnit());
}

void BackgroundImageGeometry::UseFixedAttachment(
    const LayoutPoint& attachment_point) {
  LayoutPoint aligned_point = attachment_point;
  phase_.Move(std::max(aligned_point.X() - dest_rect_.X(), LayoutUnit()),
              std::max(aligned_point.Y() - dest_rect_.Y(), LayoutUnit()));
  SetPhase(LayoutPoint(RoundedIntPoint(phase_)));
}

enum ColumnGroupDirection { kColumnGroupStart, kColumnGroupEnd };

static void ExpandToTableColumnGroup(const LayoutTableCell& cell,
                                     const LayoutTableCol& column_group,
                                     LayoutUnit& value,
                                     ColumnGroupDirection column_direction) {
  auto sibling_cell = column_direction == kColumnGroupStart
                          ? &LayoutTableCell::PreviousCell
                          : &LayoutTableCell::NextCell;
  for (const auto* sibling = (cell.*sibling_cell)(); sibling;
       sibling = (sibling->*sibling_cell)()) {
    LayoutTableCol* innermost_col =
        cell.Table()
            ->ColElementAtAbsoluteColumn(sibling->AbsoluteColumnIndex())
            .InnermostColOrColGroup();
    if (!innermost_col || innermost_col->EnclosingColumnGroup() != column_group)
      break;
    value += sibling->Size().Width();
  }
}

LayoutPoint BackgroundImageGeometry::GetOffsetForCell(
    const LayoutTableCell& cell,
    const LayoutBox& positioning_box) {
  LayoutSize border_spacing = LayoutSize(cell.Table()->HBorderSpacing(),
                                         cell.Table()->VBorderSpacing());
  if (positioning_box.IsTableSection())
    return cell.Location() - border_spacing;
  if (positioning_box.IsTableRow()) {
    return LayoutPoint(cell.Location().X(), LayoutUnit()) -
           LayoutSize(border_spacing.Width(), LayoutUnit());
  }

  LayoutRect sections_rect(LayoutPoint(), cell.Table()->Size());
  cell.Table()->SubtractCaptionRect(sections_rect);
  LayoutUnit height_of_captions =
      cell.Table()->Size().Height() - sections_rect.Height();
  LayoutPoint offset_in_background = LayoutPoint(
      LayoutUnit(), (cell.Section()->Location().Y() -
                     cell.Table()->BorderBefore() - height_of_captions) +
                        cell.Location().Y());

  DCHECK(positioning_box.IsLayoutTableCol());
  if (ToLayoutTableCol(positioning_box).IsTableColumn()) {
    return offset_in_background -
           LayoutSize(LayoutUnit(), border_spacing.Height());
  }

  DCHECK(ToLayoutTableCol(positioning_box).IsTableColumnGroup());
  LayoutUnit offset = offset_in_background.X();
  ExpandToTableColumnGroup(cell, ToLayoutTableCol(positioning_box), offset,
                           kColumnGroupStart);
  offset_in_background.Move(offset, LayoutUnit());
  return offset_in_background -
         LayoutSize(LayoutUnit(), border_spacing.Height());
}

LayoutSize BackgroundImageGeometry::GetBackgroundObjectDimensions(
    const LayoutTableCell& cell,
    const LayoutBox& positioning_box) {
  LayoutSize border_spacing = LayoutSize(cell.Table()->HBorderSpacing(),
                                         cell.Table()->VBorderSpacing());
  if (positioning_box.IsTableSection())
    return positioning_box.Size() - border_spacing - border_spacing;

  if (positioning_box.IsTableRow()) {
    return positioning_box.Size() -
           LayoutSize(border_spacing.Width(), LayoutUnit()) -
           LayoutSize(border_spacing.Width(), LayoutUnit());
  }

  DCHECK(positioning_box.IsLayoutTableCol());
  LayoutRect sections_rect(LayoutPoint(), cell.Table()->Size());
  cell.Table()->SubtractCaptionRect(sections_rect);
  LayoutUnit column_height = sections_rect.Height() -
                             cell.Table()->BorderBefore() -
                             border_spacing.Height() - border_spacing.Height();
  if (ToLayoutTableCol(positioning_box).IsTableColumn())
    return LayoutSize(cell.Size().Width(), column_height);

  DCHECK(ToLayoutTableCol(positioning_box).IsTableColumnGroup());
  LayoutUnit width = cell.Size().Width();
  ExpandToTableColumnGroup(cell, ToLayoutTableCol(positioning_box), width,
                           kColumnGroupStart);
  ExpandToTableColumnGroup(cell, ToLayoutTableCol(positioning_box), width,
                           kColumnGroupEnd);

  return LayoutSize(width, column_height);
}

namespace {

bool ShouldUseFixedAttachment(const FillLayer& fill_layer) {
  if (RuntimeEnabledFeatures::FastMobileScrollingEnabled()) {
    // As a side effect of an optimization to blit on scroll, we do not honor
    // the CSS property "background-attachment: fixed" because it may result in
    // rendering artifacts. Note, these artifacts only appear if we are blitting
    // on scroll of a page that has fixed background images.
    return false;
  }
  return fill_layer.Attachment() == kFixedBackgroundAttachment;
}

LayoutRect FixedAttachmentPositioningArea(const LayoutBoxModelObject& obj,
                                          const LayoutBoxModelObject* container,
                                          const GlobalPaintFlags flags) {
  LayoutRect rect = obj.ViewRect();
  if (FixedBackgroundPaintsInLocalCoordinates(obj, flags)) {
    rect.SetLocation(LayoutPoint());
  } else {
    if (LocalFrameView* frame_view = obj.View()->GetFrameView())
      rect.SetLocation(IntPoint(frame_view->ScrollOffsetInt()));
    // Compensate the translations created by ScrollRecorders.
    // TODO(trchen): Fix this for SP phase 2. crbug.com/529963.
    rect.MoveBy(AccumulatedScrollOffsetForFixedBackground(obj, container));
  }

  if (container)
    rect.MoveBy(LayoutPoint(-container->LocalToAbsolute(FloatPoint())));

  return rect;
}

}  // Anonymous namespace

BackgroundImageGeometry::BackgroundImageGeometry(const LayoutView& view)
    : box_(view),
      positioning_box_(view.RootBox()),
      has_non_local_geometry_(false),
      cell_using_container_background_(false) {
  // The background of the box generated by the root element covers the
  // entire canvas and will be painted by the view object, but the we should
  // still use the root element box for positioning.
  positioning_size_override_ = view.RootBox().Size();

  // The input paint rect is specified in root element local coordinate
  // (i.e. a transform is applied on the context for painting), and is
  // expanded to cover the whole canvas.  Since left/top is relative to the
  // paint rect, we need to offset them back.
  coordinate_offset_by_paint_rect_ = true;
}

BackgroundImageGeometry::BackgroundImageGeometry(
    const LayoutBoxModelObject& obj)
    : box_(obj),
      positioning_box_(obj),
      has_non_local_geometry_(false),
      coordinate_offset_by_paint_rect_(false),
      cell_using_container_background_(false) {
  // Specialized constructor should be used for LayoutView.
  DCHECK(!obj.IsLayoutView());
}

BackgroundImageGeometry::BackgroundImageGeometry(
    const LayoutTableCell& cell,
    const LayoutObject* background_object)
    : box_(cell),
      positioning_box_(background_object && !background_object->IsTableCell()
                           ? ToLayoutBoxModelObject(*background_object)
                           : cell),
      has_non_local_geometry_(false),
      coordinate_offset_by_paint_rect_(false) {
  cell_using_container_background_ =
      background_object && !background_object->IsTableCell();
  if (cell_using_container_background_) {
    offset_in_background_ =
        GetOffsetForCell(cell, ToLayoutBox(*background_object));
    positioning_size_override_ =
        GetBackgroundObjectDimensions(cell, ToLayoutBox(*background_object));
  }
}

void BackgroundImageGeometry::Calculate(const LayoutBoxModelObject* container,
                                        const GlobalPaintFlags flags,
                                        const FillLayer& fill_layer,
                                        const LayoutRect& paint_rect) {
  LayoutRect positioning_area;
  LayoutRectOutsets box_outset;
  if (!ShouldUseFixedAttachment(fill_layer)) {
    if (coordinate_offset_by_paint_rect_ || cell_using_container_background_)
      positioning_area.SetSize(positioning_size_override_);
    else
      positioning_area = paint_rect;
    if (fill_layer.Origin() != kBorderFillBox) {
      box_outset = positioning_box_.BorderBoxOutsets();
      if (fill_layer.Origin() == kContentFillBox)
        box_outset += positioning_box_.PaddingOutsets();
    }
    positioning_area.Contract(box_outset);
    SetDestRect(paint_rect);

    if (coordinate_offset_by_paint_rect_) {
      box_outset.SetLeft(box_outset.Left() - paint_rect.Location().X());
      box_outset.SetTop(box_outset.Top() - paint_rect.Location().Y());
    }
  } else {
    SetHasNonLocalGeometry();
    offset_in_background_ = LayoutPoint();
    positioning_area = FixedAttachmentPositioningArea(box_, container, flags);
    SetDestRect(positioning_area);
  }

  LayoutSize fill_tile_size(CalculateFillTileSize(positioning_box_, fill_layer,
                                                  positioning_area.Size()));
  // It's necessary to apply the heuristic here prior to any further
  // calculations to avoid incorrectly using sub-pixel values that won't be
  // present in the painted tile.
  SetTileSize(
      ApplySubPixelHeuristicToImageSize(fill_tile_size, positioning_area));

  EFillRepeat background_repeat_x = fill_layer.RepeatX();
  EFillRepeat background_repeat_y = fill_layer.RepeatY();
  LayoutUnit unsnapped_available_width =
      positioning_area.Width() - fill_tile_size.Width();
  LayoutUnit unsnapped_available_height =
      positioning_area.Height() - fill_tile_size.Height();
  LayoutSize positioning_area_size =
      LayoutSize(SnapSizeToPixel(positioning_area.Width(), dest_rect_.X()),
                 SnapSizeToPixel(positioning_area.Height(), dest_rect_.Y()));
  LayoutUnit available_width =
      positioning_area_size.Width() - TileSize().Width();
  LayoutUnit available_height =
      positioning_area_size.Height() - TileSize().Height();

  LayoutUnit computed_x_position =
      RoundedMinimumValueForLength(fill_layer.XPosition(), available_width) -
      offset_in_background_.X();
  if (background_repeat_x == kRoundFill &&
      positioning_area_size.Width() > LayoutUnit() &&
      fill_tile_size.Width() > LayoutUnit()) {
    int nr_tiles = std::max(
        1, RoundToInt(positioning_area_size.Width() / fill_tile_size.Width()));
    LayoutUnit rounded_width = positioning_area_size.Width() / nr_tiles;

    // Maintain aspect ratio if background-size: auto is set
    if (fill_layer.Size().size.Height().IsAuto() &&
        background_repeat_y != kRoundFill) {
      fill_tile_size.SetHeight(fill_tile_size.Height() * rounded_width /
                               fill_tile_size.Width());
    }
    fill_tile_size.SetWidth(rounded_width);

    SetTileSize(
        ApplySubPixelHeuristicToImageSize(fill_tile_size, positioning_area));
    SetPhaseX(
        TileSize().Width()
            ? LayoutUnit(roundf(TileSize().Width() -
                                fmodf((computed_x_position + box_outset.Left()),
                                      TileSize().Width())))
            : LayoutUnit());
    SetSpaceSize(LayoutSize());
  }

  LayoutUnit computed_y_position =
      RoundedMinimumValueForLength(fill_layer.YPosition(), available_height) -
      offset_in_background_.Y();
  if (background_repeat_y == kRoundFill &&
      positioning_area_size.Height() > LayoutUnit() &&
      fill_tile_size.Height() > LayoutUnit()) {
    int nr_tiles = std::max(1, RoundToInt(positioning_area_size.Height() /
                                          fill_tile_size.Height()));
    LayoutUnit rounded_height = positioning_area_size.Height() / nr_tiles;
    // Maintain aspect ratio if background-size: auto is set
    if (fill_layer.Size().size.Width().IsAuto() &&
        background_repeat_x != kRoundFill) {
      fill_tile_size.SetWidth(fill_tile_size.Width() * rounded_height /
                              fill_tile_size.Height());
    }
    fill_tile_size.SetHeight(rounded_height);

    SetTileSize(
        ApplySubPixelHeuristicToImageSize(fill_tile_size, positioning_area));
    SetPhaseY(
        TileSize().Height()
            ? LayoutUnit(roundf(TileSize().Height() -
                                fmodf((computed_y_position + box_outset.Top()),
                                      TileSize().Height())))
            : LayoutUnit());
    SetSpaceSize(LayoutSize());
  }

  if (background_repeat_x == kRepeatFill) {
    SetRepeatX(fill_layer, fill_tile_size.Width(), available_width,
               unsnapped_available_width, box_outset.Left(),
               offset_in_background_.X());
  } else if (background_repeat_x == kSpaceFill &&
             TileSize().Width() > LayoutUnit()) {
    LayoutUnit space = GetSpaceBetweenImageTiles(positioning_area_size.Width(),
                                                 TileSize().Width());
    if (space >= LayoutUnit())
      SetSpaceX(space, available_width, box_outset.Left());
    else
      background_repeat_x = kNoRepeatFill;
  }
  if (background_repeat_x == kNoRepeatFill) {
    LayoutUnit x_offset = fill_layer.BackgroundXOrigin() == kRightEdge
                              ? available_width - computed_x_position
                              : computed_x_position;
    SetNoRepeatX(box_outset.Left() + x_offset);
    if (offset_in_background_.X() > TileSize().Width())
      SetDestRect(LayoutRect());
  }

  if (background_repeat_y == kRepeatFill) {
    SetRepeatY(fill_layer, fill_tile_size.Height(), available_height,
               unsnapped_available_height, box_outset.Top(),
               offset_in_background_.Y());
  } else if (background_repeat_y == kSpaceFill &&
             TileSize().Height() > LayoutUnit()) {
    LayoutUnit space = GetSpaceBetweenImageTiles(positioning_area_size.Height(),
                                                 TileSize().Height());
    if (space >= LayoutUnit())
      SetSpaceY(space, available_height, box_outset.Top());
    else
      background_repeat_y = kNoRepeatFill;
  }
  if (background_repeat_y == kNoRepeatFill) {
    LayoutUnit y_offset = fill_layer.BackgroundYOrigin() == kBottomEdge
                              ? available_height - computed_y_position
                              : computed_y_position;
    SetNoRepeatY(box_outset.Top() + y_offset);
    if (offset_in_background_.Y() > TileSize().Height())
      SetDestRect(LayoutRect());
  }

  if (ShouldUseFixedAttachment(fill_layer))
    UseFixedAttachment(paint_rect.Location());

  // Clip the final output rect to the paint rect
  dest_rect_.Intersect(paint_rect);

  // Snap as-yet unsnapped values.
  SetDestRect(LayoutRect(PixelSnappedIntRect(dest_rect_)));
}

const ImageResourceObserver& BackgroundImageGeometry::ImageClient() const {
  return coordinate_offset_by_paint_rect_ ? box_ : positioning_box_;
}

const Document& BackgroundImageGeometry::ImageDocument() const {
  return box_.GetDocument();
}

const ComputedStyle& BackgroundImageGeometry::ImageStyle() const {
  return box_.StyleRef();
}

}  // namespace blink
