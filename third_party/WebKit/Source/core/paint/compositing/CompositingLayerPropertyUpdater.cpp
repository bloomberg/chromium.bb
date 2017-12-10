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

  const FragmentData& fragment_data = object.FirstFragment();
  const RarePaintData* rare_paint_data = fragment_data.GetRarePaintData();
  DCHECK(rare_paint_data);
  DCHECK(rare_paint_data->LocalBorderBoxProperties());
  // SPv1 compositing forces single fragment for composited elements.
  DCHECK(!fragment_data.NextFragment());

  LayoutPoint layout_snapped_paint_offset =
      fragment_data.PaintOffset() - mapping->SubpixelAccumulation();
  IntPoint snapped_paint_offset = RoundedIntPoint(layout_snapped_paint_offset);
  DCHECK(layout_snapped_paint_offset == snapped_paint_offset);

  auto SetContainerLayerState =
      [rare_paint_data, &snapped_paint_offset](GraphicsLayer* graphics_layer) {
        if (graphics_layer) {
          graphics_layer->SetLayerState(
              PropertyTreeState(*rare_paint_data->LocalBorderBoxProperties()),
              snapped_paint_offset + graphics_layer->OffsetFromLayoutObject());
        }
      };
  SetContainerLayerState(mapping->MainGraphicsLayer());
  SetContainerLayerState(mapping->LayerForHorizontalScrollbar());
  SetContainerLayerState(mapping->LayerForVerticalScrollbar());
  SetContainerLayerState(mapping->LayerForScrollCorner());
  SetContainerLayerState(mapping->DecorationOutlineLayer());
  SetContainerLayerState(mapping->BackgroundLayer());
  SetContainerLayerState(mapping->ChildClippingMaskLayer());

  auto SetContentsLayerState =
      [rare_paint_data, &snapped_paint_offset](GraphicsLayer* graphics_layer) {
        if (graphics_layer) {
          graphics_layer->SetLayerState(
              rare_paint_data->ContentsProperties(),
              snapped_paint_offset + graphics_layer->OffsetFromLayoutObject());
        }
      };
  SetContentsLayerState(mapping->ScrollingContentsLayer());
  SetContentsLayerState(mapping->ForegroundLayer());

  if (auto* squashing_layer = mapping->SquashingLayer()) {
    squashing_layer->SetLayerState(
        rare_paint_data->PreEffectProperties(),
        snapped_paint_offset + mapping->SquashingLayerOffsetFromLayoutObject());
  }

  if (auto* mask_layer = mapping->MaskLayer()) {
    auto state = *rare_paint_data->LocalBorderBoxProperties();
    const auto* properties = rare_paint_data->PaintProperties();
    DCHECK(properties && properties->Mask());
    state.SetEffect(properties->Mask());
    mask_layer->SetLayerState(
        std::move(state),
        snapped_paint_offset + mask_layer->OffsetFromLayoutObject());
  }

  if (auto* ancestor_clipping_mask_layer =
          mapping->AncestorClippingMaskLayer()) {
    PropertyTreeState state(
        rare_paint_data->GetPreTransform(),
        mapping->ClipInheritanceAncestor()
            ->GetLayoutObject()
            .FirstFragment()
            .GetRarePaintData()
            ->GetPostOverflowClip(),
        // This is a hack to incorporate mask-based clip-path. Really should be
        // nullptr or some dummy.
        rare_paint_data->GetPreFilter());
    ancestor_clipping_mask_layer->SetLayerState(
        std::move(state),
        snapped_paint_offset +
            ancestor_clipping_mask_layer->OffsetFromLayoutObject());
  }

  if (auto* child_clipping_mask_layer = mapping->ChildClippingMaskLayer()) {
    PropertyTreeState state = *fragment_data.LocalBorderBoxProperties();
    // Same hack as for ancestor_clipping_mask_layer.
    state.SetEffect(rare_paint_data->GetPreFilter());
    child_clipping_mask_layer->SetLayerState(
        std::move(state),
        snapped_paint_offset +
            child_clipping_mask_layer->OffsetFromLayoutObject());
  }
}

void CompositingLayerPropertyUpdater::Update(const LocalFrameView& frame_view) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV175Enabled() ||
      RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
      RuntimeEnabledFeatures::RootLayerScrollingEnabled())
    return;

  auto SetOverflowControlLayerState =
      [&frame_view](GraphicsLayer* graphics_layer) {
        if (graphics_layer) {
          graphics_layer->SetLayerState(
              frame_view.PreContentClipProperties(),
              IntPoint(graphics_layer->OffsetFromLayoutObject()));
        }
      };
  SetOverflowControlLayerState(frame_view.LayerForHorizontalScrollbar());
  SetOverflowControlLayerState(frame_view.LayerForVerticalScrollbar());
  SetOverflowControlLayerState(frame_view.LayerForScrollCorner());
}

}  // namespace blink
