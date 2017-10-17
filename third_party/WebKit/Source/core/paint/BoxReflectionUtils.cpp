// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxReflectionUtils.h"

#include "core/layout/LayoutBox.h"
#include "core/paint/NinePieceImagePainter.h"
#include "core/paint/PaintLayer.h"
#include "platform/LengthFunctions.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/BoxReflection.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintRecordBuilder.h"

namespace blink {

BoxReflection BoxReflectionForPaintLayer(const PaintLayer& layer,
                                         const ComputedStyle& style) {
  const StyleReflection* reflect_style = style.BoxReflect();

  LayoutRect frame_layout_rect =
      ToLayoutBox(layer.GetLayoutObject()).FrameRect();
  FloatRect frame_rect(frame_layout_rect);
  BoxReflection::ReflectionDirection direction =
      BoxReflection::kVerticalReflection;
  float offset = 0;
  switch (reflect_style->Direction()) {
    case kReflectionAbove:
      direction = BoxReflection::kVerticalReflection;
      offset =
          -FloatValueForLength(reflect_style->Offset(), frame_rect.Height());
      break;
    case kReflectionBelow:
      direction = BoxReflection::kVerticalReflection;
      offset =
          2 * frame_rect.Height() +
          FloatValueForLength(reflect_style->Offset(), frame_rect.Height());
      break;
    case kReflectionLeft:
      direction = BoxReflection::kHorizontalReflection;
      offset =
          -FloatValueForLength(reflect_style->Offset(), frame_rect.Width());
      break;
    case kReflectionRight:
      direction = BoxReflection::kHorizontalReflection;
      offset = 2 * frame_rect.Width() +
               FloatValueForLength(reflect_style->Offset(), frame_rect.Width());
      break;
  }

  const NinePieceImage& mask_nine_piece = reflect_style->Mask();
  if (!mask_nine_piece.HasImage())
    return BoxReflection(direction, offset, nullptr, FloatRect());

  LayoutRect mask_rect(LayoutPoint(), frame_layout_rect.Size());
  LayoutRect mask_bounding_rect(mask_rect);
  mask_bounding_rect.Expand(style.ImageOutsets(mask_nine_piece));
  FloatRect mask_bounding_float_rect(mask_bounding_rect);

  // TODO(jbroman): PaintRecordBuilder + DrawingRecorder seems excessive.
  // If NinePieceImagePainter operated on SkCanvas, we'd only need a
  // PictureRecorder here.
  PaintRecordBuilder builder(mask_bounding_float_rect);
  {
    GraphicsContext& context = builder.Context();
    DrawingRecorder recorder(context, layer.GetLayoutObject(),
                             DisplayItem::kReflectionMask,
                             mask_bounding_float_rect);
    Node* node = nullptr;
    const LayoutObject* layout_object = &layer.GetLayoutObject();
    for (; layout_object && !node; layout_object = layout_object->Parent())
      node = layout_object->GeneratingNode();
    NinePieceImagePainter::Paint(builder.Context(), layer.GetLayoutObject(),
                                 layer.GetLayoutObject().GetDocument(), node,
                                 mask_rect, style, mask_nine_piece,
                                 SkBlendMode::kSrcOver);
  }
  return BoxReflection(direction, offset, builder.EndRecording(),
                       mask_bounding_float_rect);
}

}  // namespace blink
