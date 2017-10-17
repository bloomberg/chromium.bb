// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DrawingRecorder.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

#if DCHECK_IS_ON()
static bool g_list_modification_check_disabled = false;
DisableListModificationCheck::DisableListModificationCheck()
    : disabler_(&g_list_modification_check_disabled, true) {}
#endif

DrawingRecorder::DrawingRecorder(GraphicsContext& context,
                                 const DisplayItemClient& display_item_client,
                                 DisplayItem::Type display_item_type,
                                 const IntRect& bounds)
    : context_(context),
      client_(display_item_client),
      type_(display_item_type),
      known_to_be_opaque_(false),
      bounds_(bounds)
#if DCHECK_IS_ON()
      ,
      initial_display_item_list_size_(
          context_.GetPaintController().NewDisplayItemList().size())
#endif
{
  if (context.GetPaintController().DisplayItemConstructionIsDisabled())
    return;

  // Must check DrawingRecorder::useCachedDrawingIfPossible before creating the
  // DrawingRecorder.
  DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
         !UseCachedDrawingIfPossible(context_, client_, type_));

  DCHECK(DisplayItem::IsDrawingType(display_item_type));

#if DCHECK_IS_ON()
  context.SetInDrawingRecorder(true);
#endif

  context.BeginRecording(bounds);

#if DCHECK_IS_ON()
  if (RuntimeEnabledFeatures::SlimmingPaintStrictCullRectClippingEnabled()) {
    // Skia depends on the cull rect containing all of the display item
    // commands. When strict cull rect clipping is enabled, make this explicit.
    // This allows us to identify potential incorrect cull rects that might
    // otherwise be masked due to Skia internal optimizations.
    context.Save();
    // Expand the verification clip by one pixel to account for Skia's
    // SkCanvas::getClipBounds() expansion, used in testing cull rects.
    // TODO(schenney) This is not the best place to do this. Ideally, we would
    // expand by one pixel in device (pixel) space, but to do that we would need
    // to add the verification mode to Skia.
    IntRect clip_rect = bounds;
    clip_rect.Inflate(1);
    context.ClipRect(clip_rect, kNotAntiAliased, SkClipOp::kIntersect);
  }
#endif
}

DrawingRecorder::~DrawingRecorder() {
  if (context_.GetPaintController().DisplayItemConstructionIsDisabled())
    return;

#if DCHECK_IS_ON()
  if (RuntimeEnabledFeatures::SlimmingPaintStrictCullRectClippingEnabled())
    context_.Restore();

  context_.SetInDrawingRecorder(false);

  if (!g_list_modification_check_disabled) {
    DCHECK(initial_display_item_list_size_ ==
           context_.GetPaintController().NewDisplayItemList().size());
  }
#endif

  sk_sp<const PaintRecord> picture = context_.EndRecording();

#if DCHECK_IS_ON()
  if (!RuntimeEnabledFeatures::SlimmingPaintStrictCullRectClippingEnabled() &&
      !context_.GetPaintController().IsForPaintRecordBuilder() &&
      client_.PaintedOutputOfObjectHasNoEffectRegardlessOfSize()) {
    DCHECK_EQ(0u, picture->size()) << client_.DebugName();
  }
#endif

  context_.GetPaintController().CreateAndAppend<DrawingDisplayItem>(
      client_, type_, picture, bounds_, known_to_be_opaque_);
}

}  // namespace blink
