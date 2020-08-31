// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_mask_painter.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

SVGMaskPainter::SVGMaskPainter(GraphicsContext& context,
                               const LayoutObject& layout_object,
                               const DisplayItemClient& display_item_client)
    : context_(context),
      layout_object_(layout_object),
      display_item_client_(display_item_client) {
  DCHECK(layout_object_.StyleRef().SvgStyle().MaskerResource());
}

SVGMaskPainter::~SVGMaskPainter() {
  const auto* properties = layout_object_.FirstFragment().PaintProperties();
  // TODO(crbug.com/814815): This condition should be a DCHECK, but for now
  // we may paint the object for filters during PrePaint before the
  // properties are ready.
  if (!properties || !properties->Mask())
    return;
  ScopedPaintChunkProperties scoped_paint_chunk_properties(
      context_.GetPaintController(), *properties->Mask(), display_item_client_,
      DisplayItem::kSVGMask);

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          context_, display_item_client_, DisplayItem::kSVGMask))
    return;
  DrawingRecorder recorder(context_, display_item_client_,
                           DisplayItem::kSVGMask);

  const SVGComputedStyle& svg_style = layout_object_.StyleRef().SvgStyle();
  auto* masker =
      GetSVGResourceAsType<LayoutSVGResourceMasker>(svg_style.MaskerResource());
  DCHECK(masker);
  SECURITY_DCHECK(!masker->NeedsLayout());
  masker->ClearInvalidationMask();

  AffineTransform content_transformation;
  sk_sp<const PaintRecord> record = masker->CreatePaintRecord(
      content_transformation,
      SVGResources::ReferenceBoxForEffects(layout_object_), context_);

  context_.Save();
  context_.ConcatCTM(content_transformation);
  context_.DrawRecord(std::move(record));
  context_.Restore();
}

}  // namespace blink
