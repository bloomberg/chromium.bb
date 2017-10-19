// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/compositing/CompositingLayerPropertyUpdater.h"

#include "core/layout/LayoutBoxModelObject.h"
#include "core/paint/FragmentData.h"
#include "core/paint/compositing/CompositedLayerMapping.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

void CompositingLayerPropertyUpdater::Update(const LayoutObject& object) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV175Enabled() ||
      RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  if (!object.HasLayer())
    return;
  CompositedLayerMapping* mapping =
      ToLayoutBoxModelObject(object).Layer()->GetCompositedLayerMapping();
  if (!mapping)
    return;

  const FragmentData* fragment_data = object.FirstFragment();
  DCHECK(fragment_data);
  DCHECK(fragment_data->LocalBorderBoxProperties());
  // SPv1 compositing forces single fragment for composited elements.
  DCHECK(!fragment_data->NextFragment());

  LayoutPoint layout_snapped_paint_offset =
      fragment_data->PaintOffset() - mapping->SubpixelAccumulation();
  IntPoint snapped_paint_offset = RoundedIntPoint(layout_snapped_paint_offset);
  DCHECK(layout_snapped_paint_offset == snapped_paint_offset);

  if (GraphicsLayer* main_layer = mapping->MainGraphicsLayer()) {
    main_layer->SetLayerState(
        PropertyTreeState(*fragment_data->LocalBorderBoxProperties()),
        snapped_paint_offset + main_layer->OffsetFromLayoutObject());
  }
  if (GraphicsLayer* scrolling_contents_layer =
          mapping->ScrollingContentsLayer()) {
    scrolling_contents_layer->SetLayerState(
        fragment_data->ContentsProperties(),
        snapped_paint_offset +
            scrolling_contents_layer->OffsetFromLayoutObject());
  }
  if (GraphicsLayer* squashing_layer = mapping->SquashingLayer()) {
    squashing_layer->SetLayerState(
        fragment_data->PreEffectProperties(),
        snapped_paint_offset + mapping->SquashingLayerOffsetFromLayoutObject());
  }
  if (GraphicsLayer* foreground_layer = mapping->ForegroundLayer()) {
    foreground_layer->SetLayerState(
        fragment_data->ContentsProperties(),
        snapped_paint_offset + foreground_layer->OffsetFromLayoutObject());
  }
  // TODO(trchen): Complete for all drawable layers.
}

}  // namespace blink
