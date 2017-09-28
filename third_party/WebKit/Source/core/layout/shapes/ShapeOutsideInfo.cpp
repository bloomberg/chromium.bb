/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "core/layout/shapes/ShapeOutsideInfo.h"

#include <memory>
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/FloatingObjects.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutImage.h"
#include "core/layout/api/LineLayoutBlockFlow.h"
#include "platform/LengthFunctions.h"
#include "platform/wtf/AutoReset.h"
#include "public/platform/Platform.h"

namespace blink {

CSSBoxType ReferenceBox(const ShapeValue& shape_value) {
  if (shape_value.CssBox() == kBoxMissing)
    return kMarginBox;
  return shape_value.CssBox();
}

void ShapeOutsideInfo::SetReferenceBoxLogicalSize(
    LayoutSize new_reference_box_logical_size) {
  bool is_horizontal_writing_mode =
      layout_box_.ContainingBlock()->Style()->IsHorizontalWritingMode();
  switch (ReferenceBox(*layout_box_.Style()->ShapeOutside())) {
    case kMarginBox:
      if (is_horizontal_writing_mode)
        new_reference_box_logical_size.Expand(layout_box_.MarginWidth(),
                                              layout_box_.MarginHeight());
      else
        new_reference_box_logical_size.Expand(layout_box_.MarginHeight(),
                                              layout_box_.MarginWidth());
      break;
    case kBorderBox:
      break;
    case kPaddingBox:
      if (is_horizontal_writing_mode)
        new_reference_box_logical_size.Shrink(layout_box_.BorderWidth(),
                                              layout_box_.BorderHeight());
      else
        new_reference_box_logical_size.Shrink(layout_box_.BorderHeight(),
                                              layout_box_.BorderWidth());
      break;
    case kContentBox:
      if (is_horizontal_writing_mode)
        new_reference_box_logical_size.Shrink(
            layout_box_.BorderAndPaddingWidth(),
            layout_box_.BorderAndPaddingHeight());
      else
        new_reference_box_logical_size.Shrink(
            layout_box_.BorderAndPaddingHeight(),
            layout_box_.BorderAndPaddingWidth());
      break;
    case kBoxMissing:
      NOTREACHED();
      break;
  }

  new_reference_box_logical_size.ClampNegativeToZero();

  if (reference_box_logical_size_ == new_reference_box_logical_size)
    return;
  MarkShapeAsDirty();
  reference_box_logical_size_ = new_reference_box_logical_size;
}

static bool CheckShapeImageOrigin(Document& document,
                                  const StyleImage& style_image) {
  if (style_image.IsGeneratedImage())
    return true;

  DCHECK(style_image.CachedImage());
  ImageResourceContent& image_resource = *(style_image.CachedImage());
  if (image_resource.IsAccessAllowed(document.GetSecurityOrigin()))
    return true;

  const KURL& url = image_resource.Url();
  String url_string = url.IsNull() ? "''" : url.ElidedString();
  document.AddConsoleMessage(
      ConsoleMessage::Create(kSecurityMessageSource, kErrorMessageLevel,
                             "Unsafe attempt to load URL " + url_string + "."));

  return false;
}

static LayoutRect GetShapeImageMarginRect(
    const LayoutBox& layout_box,
    const LayoutSize& reference_box_logical_size) {
  LayoutPoint margin_box_origin(
      -layout_box.MarginLineLeft() - layout_box.BorderAndPaddingLogicalLeft(),
      -layout_box.MarginBefore() - layout_box.BorderBefore() -
          layout_box.PaddingBefore());
  LayoutSize margin_box_size_delta(
      layout_box.MarginLogicalWidth() +
          layout_box.BorderAndPaddingLogicalWidth(),
      layout_box.MarginLogicalHeight() +
          layout_box.BorderAndPaddingLogicalHeight());
  LayoutSize margin_rect_size(reference_box_logical_size +
                              margin_box_size_delta);
  margin_rect_size.ClampNegativeToZero();
  return LayoutRect(margin_box_origin, margin_rect_size);
}

static bool IsValidRasterShapeRect(const LayoutRect& rect) {
  static double max_image_size_bytes = 0;
  if (!max_image_size_bytes) {
    size_t size32_max_bytes =
        0xFFFFFFFF / 4;  // Some platforms don't limit maxDecodedImageBytes.
    max_image_size_bytes =
        std::min(size32_max_bytes, Platform::Current()->MaxDecodedImageBytes());
  }
  return (rect.Width().ToFloat() * rect.Height().ToFloat() * 4.0) <
         max_image_size_bytes;
}

std::unique_ptr<Shape> ShapeOutsideInfo::CreateShapeForImage(
    StyleImage* style_image,
    float shape_image_threshold,
    WritingMode writing_mode,
    float margin) const {
  const LayoutSize& image_size = style_image->ImageSize(
      layout_box_.GetDocument(), layout_box_.Style()->EffectiveZoom(),
      reference_box_logical_size_);

  const LayoutRect& margin_rect =
      GetShapeImageMarginRect(layout_box_, reference_box_logical_size_);
  const LayoutRect& image_rect =
      (layout_box_.IsLayoutImage())
          ? ToLayoutImage(layout_box_).ReplacedContentRect()
          : LayoutRect(LayoutPoint(), image_size);

  if (!IsValidRasterShapeRect(margin_rect) ||
      !IsValidRasterShapeRect(image_rect)) {
    layout_box_.GetDocument().AddConsoleMessage(
        ConsoleMessage::Create(kRenderingMessageSource, kErrorMessageLevel,
                               "The shape-outside image is too large."));
    return Shape::CreateEmptyRasterShape(writing_mode, margin);
  }

  DCHECK(!style_image->IsPendingImage());
  RefPtr<Image> image = style_image->GetImage(
      layout_box_, layout_box_.GetDocument(), layout_box_.StyleRef(),
      FlooredIntSize(image_size), nullptr);

  return Shape::CreateRasterShape(image.get(), shape_image_threshold,
                                  image_rect, margin_rect, writing_mode,
                                  margin);
}

const Shape& ShapeOutsideInfo::ComputedShape() const {
  if (Shape* shape = shape_.get())
    return *shape;

  AutoReset<bool> is_in_computing_shape(&is_computing_shape_, true);

  const ComputedStyle& style = *layout_box_.Style();
  DCHECK(layout_box_.ContainingBlock());
  const ComputedStyle& containing_block_style =
      *layout_box_.ContainingBlock()->Style();

  WritingMode writing_mode = containing_block_style.GetWritingMode();
  // Make sure contentWidth is not negative. This can happen when containing
  // block has a vertical scrollbar and its content is smaller than the
  // scrollbar width.
  LayoutUnit maximum_value =
      layout_box_.ContainingBlock()
          ? std::max(LayoutUnit(),
                     layout_box_.ContainingBlock()->ContentWidth())
          : LayoutUnit();
  float margin = FloatValueForLength(layout_box_.Style()->ShapeMargin(),
                                     maximum_value.ToFloat());

  float shape_image_threshold = style.ShapeImageThreshold();
  DCHECK(style.ShapeOutside());
  const ShapeValue& shape_value = *style.ShapeOutside();

  switch (shape_value.GetType()) {
    case ShapeValue::kShape:
      DCHECK(shape_value.Shape());
      shape_ =
          Shape::CreateShape(shape_value.Shape(), reference_box_logical_size_,
                             writing_mode, margin);
      break;
    case ShapeValue::kImage:
      DCHECK(shape_value.IsImageValid());
      shape_ = CreateShapeForImage(shape_value.GetImage(),
                                   shape_image_threshold, writing_mode, margin);
      break;
    case ShapeValue::kBox: {
      const FloatRoundedRect& shape_rect = style.GetRoundedBorderFor(
          LayoutRect(LayoutPoint(), reference_box_logical_size_),
          layout_box_.View());
      shape_ = Shape::CreateLayoutBoxShape(shape_rect, writing_mode, margin);
      break;
    }
  }

  DCHECK(shape_);
  return *shape_;
}

inline LayoutUnit BorderBeforeInWritingMode(const LayoutBox& layout_box,
                                            WritingMode writing_mode) {
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      return LayoutUnit(layout_box.BorderTop());
    case WritingMode::kVerticalLr:
      return LayoutUnit(layout_box.BorderLeft());
    case WritingMode::kVerticalRl:
      return LayoutUnit(layout_box.BorderRight());
  }

  NOTREACHED();
  return LayoutUnit(layout_box.BorderBefore());
}

inline LayoutUnit BorderAndPaddingBeforeInWritingMode(
    const LayoutBox& layout_box,
    WritingMode writing_mode) {
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      return layout_box.BorderTop() + layout_box.PaddingTop();
    case WritingMode::kVerticalLr:
      return layout_box.BorderLeft() + layout_box.PaddingLeft();
    case WritingMode::kVerticalRl:
      return layout_box.BorderRight() + layout_box.PaddingRight();
  }

  NOTREACHED();
  return layout_box.BorderAndPaddingBefore();
}

LayoutUnit ShapeOutsideInfo::LogicalTopOffset() const {
  switch (ReferenceBox(*layout_box_.Style()->ShapeOutside())) {
    case kMarginBox:
      return -layout_box_.MarginBefore(layout_box_.ContainingBlock()->Style());
    case kBorderBox:
      return LayoutUnit();
    case kPaddingBox:
      return BorderBeforeInWritingMode(
          layout_box_,
          layout_box_.ContainingBlock()->Style()->GetWritingMode());
    case kContentBox:
      return BorderAndPaddingBeforeInWritingMode(
          layout_box_,
          layout_box_.ContainingBlock()->Style()->GetWritingMode());
    case kBoxMissing:
      break;
  }

  NOTREACHED();
  return LayoutUnit();
}

inline LayoutUnit BorderStartWithStyleForWritingMode(
    const LayoutBox& layout_box,
    const ComputedStyle* style) {
  if (style->IsHorizontalWritingMode()) {
    if (style->IsLeftToRightDirection())
      return LayoutUnit(layout_box.BorderLeft());

    return LayoutUnit(layout_box.BorderRight());
  }
  if (style->IsLeftToRightDirection())
    return LayoutUnit(layout_box.BorderTop());

  return LayoutUnit(layout_box.BorderBottom());
}

inline LayoutUnit BorderAndPaddingStartWithStyleForWritingMode(
    const LayoutBox& layout_box,
    const ComputedStyle* style) {
  if (style->IsHorizontalWritingMode()) {
    if (style->IsLeftToRightDirection())
      return layout_box.BorderLeft() + layout_box.PaddingLeft();

    return layout_box.BorderRight() + layout_box.PaddingRight();
  }
  if (style->IsLeftToRightDirection())
    return layout_box.BorderTop() + layout_box.PaddingTop();

  return layout_box.BorderBottom() + layout_box.PaddingBottom();
}

LayoutUnit ShapeOutsideInfo::LogicalLeftOffset() const {
  switch (ReferenceBox(*layout_box_.Style()->ShapeOutside())) {
    case kMarginBox:
      return -layout_box_.MarginStart(layout_box_.ContainingBlock()->Style());
    case kBorderBox:
      return LayoutUnit();
    case kPaddingBox:
      return BorderStartWithStyleForWritingMode(
          layout_box_, layout_box_.ContainingBlock()->Style());
    case kContentBox:
      return BorderAndPaddingStartWithStyleForWritingMode(
          layout_box_, layout_box_.ContainingBlock()->Style());
    case kBoxMissing:
      break;
  }

  NOTREACHED();
  return LayoutUnit();
}

bool ShapeOutsideInfo::IsEnabledFor(const LayoutBox& box) {
  ShapeValue* shape_value = box.Style()->ShapeOutside();
  if (!box.IsFloating() || !shape_value)
    return false;

  switch (shape_value->GetType()) {
    case ShapeValue::kShape:
      return shape_value->Shape();
    case ShapeValue::kImage:
      return shape_value->IsImageValid() &&
             CheckShapeImageOrigin(box.GetDocument(),
                                   *(shape_value->GetImage()));
    case ShapeValue::kBox:
      return true;
  }

  return false;
}

ShapeOutsideDeltas ShapeOutsideInfo::ComputeDeltasForContainingBlockLine(
    const LineLayoutBlockFlow& containing_block,
    const FloatingObject& floating_object,
    LayoutUnit line_top,
    LayoutUnit line_height) {
  DCHECK_GE(line_height, 0);

  LayoutUnit border_box_top =
      containing_block.LogicalTopForFloat(floating_object) +
      containing_block.MarginBeforeForChild(layout_box_);
  LayoutUnit border_box_line_top = line_top - border_box_top;

  if (IsShapeDirty() ||
      !shape_outside_deltas_.IsForLine(border_box_line_top, line_height)) {
    LayoutUnit reference_box_line_top =
        border_box_line_top - LogicalTopOffset();
    LayoutUnit float_margin_box_width = std::max(
        containing_block.LogicalWidthForFloat(floating_object), LayoutUnit());

    if (ComputedShape().LineOverlapsShapeMarginBounds(reference_box_line_top,
                                                      line_height)) {
      LineSegment segment = ComputedShape().GetExcludedInterval(
          (border_box_line_top - LogicalTopOffset()),
          std::min(line_height, ShapeLogicalBottom() - border_box_line_top));
      if (segment.is_valid) {
        LayoutUnit logical_left_margin =
            containing_block.Style()->IsLeftToRightDirection()
                ? containing_block.MarginStartForChild(layout_box_)
                : containing_block.MarginEndForChild(layout_box_);
        LayoutUnit raw_left_margin_box_delta(
            segment.logical_left + LogicalLeftOffset() + logical_left_margin);
        LayoutUnit left_margin_box_delta = clampTo<LayoutUnit>(
            raw_left_margin_box_delta, LayoutUnit(), float_margin_box_width);

        LayoutUnit logical_right_margin =
            containing_block.Style()->IsLeftToRightDirection()
                ? containing_block.MarginEndForChild(layout_box_)
                : containing_block.MarginStartForChild(layout_box_);
        LayoutUnit raw_right_margin_box_delta(
            segment.logical_right + LogicalLeftOffset() -
            containing_block.LogicalWidthForChild(layout_box_) -
            logical_right_margin);
        LayoutUnit right_margin_box_delta = clampTo<LayoutUnit>(
            raw_right_margin_box_delta, -float_margin_box_width, LayoutUnit());

        shape_outside_deltas_ =
            ShapeOutsideDeltas(left_margin_box_delta, right_margin_box_delta,
                               true, border_box_line_top, line_height);
        return shape_outside_deltas_;
      }
    }

    // Lines that do not overlap the shape should act as if the float
    // wasn't there for layout purposes. So we set the deltas to remove the
    // entire width of the float.
    shape_outside_deltas_ =
        ShapeOutsideDeltas(float_margin_box_width, -float_margin_box_width,
                           false, border_box_line_top, line_height);
  }

  return shape_outside_deltas_;
}

LayoutRect ShapeOutsideInfo::ComputedShapePhysicalBoundingBox() const {
  LayoutRect physical_bounding_box =
      ComputedShape().ShapeMarginLogicalBoundingBox();
  physical_bounding_box.SetX(physical_bounding_box.X() + LogicalLeftOffset());

  if (layout_box_.Style()->IsFlippedBlocksWritingMode())
    physical_bounding_box.SetY(layout_box_.LogicalHeight() -
                               physical_bounding_box.MaxY());
  else
    physical_bounding_box.SetY(physical_bounding_box.Y() + LogicalTopOffset());

  if (!layout_box_.Style()->IsHorizontalWritingMode())
    physical_bounding_box = physical_bounding_box.TransposedRect();
  else
    physical_bounding_box.SetY(physical_bounding_box.Y() + LogicalTopOffset());

  return physical_bounding_box;
}

FloatPoint ShapeOutsideInfo::ShapeToLayoutObjectPoint(FloatPoint point) const {
  FloatPoint result = FloatPoint(point.X() + LogicalLeftOffset(),
                                 point.Y() + LogicalTopOffset());
  if (layout_box_.Style()->IsFlippedBlocksWritingMode())
    result.SetY(layout_box_.LogicalHeight() - result.Y());
  if (!layout_box_.Style()->IsHorizontalWritingMode())
    result = result.TransposedPoint();
  return result;
}

FloatSize ShapeOutsideInfo::ShapeToLayoutObjectSize(FloatSize size) const {
  if (!layout_box_.Style()->IsHorizontalWritingMode())
    return size.TransposedSize();
  return size;
}

}  // namespace blink
