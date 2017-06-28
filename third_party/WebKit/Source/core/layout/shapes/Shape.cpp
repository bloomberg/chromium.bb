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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/shapes/Shape.h"

#include <memory>
#include "core/css/BasicShapeFunctions.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMTypedArray.h"
#include "core/layout/shapes/BoxShape.h"
#include "core/layout/shapes/PolygonShape.h"
#include "core/layout/shapes/RasterShape.h"
#include "core/layout/shapes/RectangleShape.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/LengthFunctions.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/typed_arrays/ArrayBufferContents.h"

namespace blink {

static std::unique_ptr<Shape> CreateInsetShape(const FloatRoundedRect& bounds) {
  DCHECK_GE(bounds.Rect().Width(), 0);
  DCHECK_GE(bounds.Rect().Height(), 0);
  return WTF::MakeUnique<BoxShape>(bounds);
}

static std::unique_ptr<Shape> CreateCircleShape(const FloatPoint& center,
                                                float radius) {
  DCHECK_GE(radius, 0);
  return WTF::WrapUnique(
      new RectangleShape(FloatRect(center.X() - radius, center.Y() - radius,
                                   radius * 2, radius * 2),
                         FloatSize(radius, radius)));
}

static std::unique_ptr<Shape> CreateEllipseShape(const FloatPoint& center,
                                                 const FloatSize& radii) {
  DCHECK_GE(radii.Width(), 0);
  DCHECK_GE(radii.Height(), 0);
  return WTF::WrapUnique(new RectangleShape(
      FloatRect(center.X() - radii.Width(), center.Y() - radii.Height(),
                radii.Width() * 2, radii.Height() * 2),
      radii));
}

static std::unique_ptr<Shape> CreatePolygonShape(
    std::unique_ptr<Vector<FloatPoint>> vertices,
    WindRule fill_rule) {
  return WTF::WrapUnique(new PolygonShape(std::move(vertices), fill_rule));
}

static inline FloatRect PhysicalRectToLogical(const FloatRect& rect,
                                              float logical_box_height,
                                              WritingMode writing_mode) {
  if (IsHorizontalWritingMode(writing_mode))
    return rect;
  if (IsFlippedBlocksWritingMode(writing_mode))
    return FloatRect(rect.Y(), logical_box_height - rect.MaxX(), rect.Height(),
                     rect.Width());
  return rect.TransposedRect();
}

static inline FloatPoint PhysicalPointToLogical(const FloatPoint& point,
                                                float logical_box_height,
                                                WritingMode writing_mode) {
  if (IsHorizontalWritingMode(writing_mode))
    return point;
  if (IsFlippedBlocksWritingMode(writing_mode))
    return FloatPoint(point.Y(), logical_box_height - point.X());
  return point.TransposedPoint();
}

static inline FloatSize PhysicalSizeToLogical(const FloatSize& size,
                                              WritingMode writing_mode) {
  if (IsHorizontalWritingMode(writing_mode))
    return size;
  return size.TransposedSize();
}

std::unique_ptr<Shape> Shape::CreateShape(const BasicShape* basic_shape,
                                          const LayoutSize& logical_box_size,
                                          WritingMode writing_mode,
                                          float margin) {
  DCHECK(basic_shape);

  bool horizontal_writing_mode = IsHorizontalWritingMode(writing_mode);
  float box_width = horizontal_writing_mode
                        ? logical_box_size.Width().ToFloat()
                        : logical_box_size.Height().ToFloat();
  float box_height = horizontal_writing_mode
                         ? logical_box_size.Height().ToFloat()
                         : logical_box_size.Width().ToFloat();
  std::unique_ptr<Shape> shape;

  switch (basic_shape->GetType()) {
    case BasicShape::kBasicShapeCircleType: {
      const BasicShapeCircle* circle = ToBasicShapeCircle(basic_shape);
      FloatPoint center =
          FloatPointForCenterCoordinate(circle->CenterX(), circle->CenterY(),
                                        FloatSize(box_width, box_height));
      float radius =
          circle->FloatValueForRadiusInBox(FloatSize(box_width, box_height));
      FloatPoint logical_center = PhysicalPointToLogical(
          center, logical_box_size.Height().ToFloat(), writing_mode);

      shape = CreateCircleShape(logical_center, radius);
      break;
    }

    case BasicShape::kBasicShapeEllipseType: {
      const BasicShapeEllipse* ellipse = ToBasicShapeEllipse(basic_shape);
      FloatPoint center =
          FloatPointForCenterCoordinate(ellipse->CenterX(), ellipse->CenterY(),
                                        FloatSize(box_width, box_height));
      float radius_x = ellipse->FloatValueForRadiusInBox(ellipse->RadiusX(),
                                                         center.X(), box_width);
      float radius_y = ellipse->FloatValueForRadiusInBox(
          ellipse->RadiusY(), center.Y(), box_height);
      FloatPoint logical_center = PhysicalPointToLogical(
          center, logical_box_size.Height().ToFloat(), writing_mode);

      shape = CreateEllipseShape(logical_center, FloatSize(radius_x, radius_y));
      break;
    }

    case BasicShape::kBasicShapePolygonType: {
      const BasicShapePolygon* polygon = ToBasicShapePolygon(basic_shape);
      const Vector<Length>& values = polygon->Values();
      size_t values_size = values.size();
      DCHECK(!(values_size % 2));
      std::unique_ptr<Vector<FloatPoint>> vertices =
          WTF::WrapUnique(new Vector<FloatPoint>(values_size / 2));
      for (unsigned i = 0; i < values_size; i += 2) {
        FloatPoint vertex(FloatValueForLength(values.at(i), box_width),
                          FloatValueForLength(values.at(i + 1), box_height));
        (*vertices)[i / 2] = PhysicalPointToLogical(
            vertex, logical_box_size.Height().ToFloat(), writing_mode);
      }
      shape = CreatePolygonShape(std::move(vertices), polygon->GetWindRule());
      break;
    }

    case BasicShape::kBasicShapeInsetType: {
      const BasicShapeInset& inset = *ToBasicShapeInset(basic_shape);
      float left = FloatValueForLength(inset.Left(), box_width);
      float top = FloatValueForLength(inset.Top(), box_height);
      float right = FloatValueForLength(inset.Right(), box_width);
      float bottom = FloatValueForLength(inset.Bottom(), box_height);
      FloatRect rect(left, top, std::max<float>(box_width - left - right, 0),
                     std::max<float>(box_height - top - bottom, 0));
      FloatRect logical_rect = PhysicalRectToLogical(
          rect, logical_box_size.Height().ToFloat(), writing_mode);

      FloatSize box_size(box_width, box_height);
      FloatSize top_left_radius = PhysicalSizeToLogical(
          FloatSizeForLengthSize(inset.TopLeftRadius(), box_size),
          writing_mode);
      FloatSize top_right_radius = PhysicalSizeToLogical(
          FloatSizeForLengthSize(inset.TopRightRadius(), box_size),
          writing_mode);
      FloatSize bottom_left_radius = PhysicalSizeToLogical(
          FloatSizeForLengthSize(inset.BottomLeftRadius(), box_size),
          writing_mode);
      FloatSize bottom_right_radius = PhysicalSizeToLogical(
          FloatSizeForLengthSize(inset.BottomRightRadius(), box_size),
          writing_mode);
      FloatRoundedRect::Radii corner_radii(top_left_radius, top_right_radius,
                                           bottom_left_radius,
                                           bottom_right_radius);

      FloatRoundedRect final_rect(logical_rect, corner_radii);
      final_rect.ConstrainRadii();

      shape = CreateInsetShape(final_rect);
      break;
    }

    default:
      NOTREACHED();
  }

  shape->writing_mode_ = writing_mode;
  shape->margin_ = margin;

  return shape;
}

std::unique_ptr<Shape> Shape::CreateEmptyRasterShape(WritingMode writing_mode,
                                                     float margin) {
  std::unique_ptr<RasterShapeIntervals> intervals =
      WTF::MakeUnique<RasterShapeIntervals>(0, 0);
  std::unique_ptr<RasterShape> raster_shape =
      WTF::WrapUnique(new RasterShape(std::move(intervals), IntSize()));
  raster_shape->writing_mode_ = writing_mode;
  raster_shape->margin_ = margin;
  return std::move(raster_shape);
}

std::unique_ptr<Shape> Shape::CreateRasterShape(Image* image,
                                                float threshold,
                                                const LayoutRect& image_r,
                                                const LayoutRect& margin_r,
                                                WritingMode writing_mode,
                                                float margin) {
  IntRect image_rect = PixelSnappedIntRect(image_r);
  IntRect margin_rect = PixelSnappedIntRect(margin_r);

  std::unique_ptr<RasterShapeIntervals> intervals = WTF::WrapUnique(
      new RasterShapeIntervals(margin_rect.Height(), -margin_rect.Y()));
  std::unique_ptr<ImageBuffer> image_buffer =
      ImageBuffer::Create(image_rect.Size());

  if (image && image_buffer) {
    // FIXME: This is not totally correct but it is needed to prevent shapes
    // that loads SVG Images during paint invalidations to mark layoutObjects
    // for layout, which is not allowed. See https://crbug.com/429346
    ImageObserverDisabler disabler(image);
    PaintFlags flags;
    IntRect image_source_rect(IntPoint(), image->Size());
    IntRect image_dest_rect(IntPoint(), image_rect.Size());
    // TODO(ccameron): No color conversion is required here.
    image->Draw(image_buffer->Canvas(), flags, image_dest_rect,
                image_source_rect, kDoNotRespectImageOrientation,
                Image::kDoNotClampImageToSourceRect);

    WTF::ArrayBufferContents contents;
    image_buffer->GetImageData(
        kUnmultiplied, IntRect(IntPoint(), image_rect.Size()), contents);
    DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(contents);
    DOMUint8ClampedArray* pixel_array = DOMUint8ClampedArray::Create(
        array_buffer, 0, array_buffer->ByteLength());
    unsigned pixel_array_offset = 3;  // Each pixel is four bytes: RGBA.
    uint8_t alpha_pixel_threshold = threshold * 255;

    DCHECK_EQ(
        static_cast<unsigned>(image_rect.Width() * image_rect.Height() * 4),
        pixel_array->length());

    int min_buffer_y = std::max(0, margin_rect.Y() - image_rect.Y());
    int max_buffer_y =
        std::min(image_rect.Height(), margin_rect.MaxY() - image_rect.Y());

    for (int y = min_buffer_y; y < max_buffer_y; ++y) {
      int start_x = -1;
      for (int x = 0; x < image_rect.Width(); ++x, pixel_array_offset += 4) {
        uint8_t alpha = pixel_array->Item(pixel_array_offset);
        bool alpha_above_threshold = alpha > alpha_pixel_threshold;
        if (start_x == -1 && alpha_above_threshold) {
          start_x = x;
        } else if (start_x != -1 &&
                   (!alpha_above_threshold || x == image_rect.Width() - 1)) {
          int end_x = alpha_above_threshold ? x + 1 : x;
          intervals->IntervalAt(y + image_rect.Y())
              .Unite(IntShapeInterval(start_x + image_rect.X(),
                                      end_x + image_rect.X()));
          start_x = -1;
        }
      }
    }
  }

  std::unique_ptr<RasterShape> raster_shape = WTF::WrapUnique(
      new RasterShape(std::move(intervals), margin_rect.Size()));
  raster_shape->writing_mode_ = writing_mode;
  raster_shape->margin_ = margin;
  return std::move(raster_shape);
}

std::unique_ptr<Shape> Shape::CreateLayoutBoxShape(
    const FloatRoundedRect& rounded_rect,
    WritingMode writing_mode,
    float margin) {
  FloatRect rect(0, 0, rounded_rect.Rect().Width(),
                 rounded_rect.Rect().Height());
  FloatRoundedRect bounds(rect, rounded_rect.GetRadii());
  std::unique_ptr<Shape> shape = CreateInsetShape(bounds);
  shape->writing_mode_ = writing_mode;
  shape->margin_ = margin;

  return shape;
}

}  // namespace blink
