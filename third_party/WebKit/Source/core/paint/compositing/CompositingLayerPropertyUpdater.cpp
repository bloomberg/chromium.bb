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
  const auto* paint_layer = ToLayoutBoxModelObject(object).Layer();
  const auto* mapping = paint_layer->GetCompositedLayerMapping();
  if (!mapping)
    return;

  const FragmentData& fragment_data = object.FirstFragment();
  DCHECK(fragment_data.HasLocalBorderBoxProperties());
  // SPv1 compositing forces single fragment for composited elements.
  DCHECK(!fragment_data.NextFragment());

  LayoutPoint layout_snapped_paint_offset =
      fragment_data.PaintOffset() - mapping->SubpixelAccumulation();
  IntPoint snapped_paint_offset = RoundedIntPoint(layout_snapped_paint_offset);
  DCHECK(layout_snapped_paint_offset == snapped_paint_offset);

  Optional<PropertyTreeState> container_layer_state;
  auto SetContainerLayerState =
      [&fragment_data, &snapped_paint_offset,
       &container_layer_state](GraphicsLayer* graphics_layer) {
        if (graphics_layer) {
          if (!container_layer_state) {
            container_layer_state = fragment_data.LocalBorderBoxProperties();
            if (const auto* properties = fragment_data.PaintProperties()) {
              // CSS clip should be applied within the layer.
              if (const auto* css_clip = properties->CssClip())
                container_layer_state->SetClip(css_clip->Parent());
            }
          }
          graphics_layer->SetLayerState(
              *container_layer_state,
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

  if (mapping->ScrollingContentsLayer()) {
    auto paint_offset = snapped_paint_offset;

    // In flipped blocks writing mode, if there is scrollbar on the right,
    // we move the contents to the left with extra amount of ScrollTranslation
    // (-VerticalScrollbarWidth, 0). However, ScrollTranslation doesn't apply
    // on ScrollingContentsLayer so we shift paint offset instead.
    if (object.IsBox() && object.HasFlippedBlocksWritingMode())
      paint_offset.Move(ToLayoutBox(object).VerticalScrollbarWidth(), 0);

    auto SetContentsLayerState =
        [&fragment_data, &paint_offset](GraphicsLayer* graphics_layer) {
          if (graphics_layer) {
            graphics_layer->SetLayerState(
                fragment_data.ContentsProperties(),
                paint_offset + graphics_layer->OffsetFromLayoutObject());
          }
        };
    SetContentsLayerState(mapping->ScrollingContentsLayer());
    SetContentsLayerState(mapping->ForegroundLayer());
  } else {
    SetContainerLayerState(mapping->ForegroundLayer());
  }

  if (auto* squashing_layer = mapping->SquashingLayer()) {
    auto state = fragment_data.PreEffectProperties();
    // The squashing layer's ClippingContainer is the common ancestor of clip
    // state of all squashed layers, so we should use its clip state. This skips
    // any control clips on the squashing layer's object which should not apply
    // on squashed layers.
    const auto* clipping_container = paint_layer->ClippingContainer();
    if (RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
      state.SetClip(
          clipping_container
              ? clipping_container->FirstFragment().ContentsProperties().Clip()
              : ClipPaintPropertyNode::Root());
    } else {
      state.SetClip(
          clipping_container
              ? clipping_container->FirstFragment().ContentsProperties().Clip()
              : paint_layer->GetLayoutObject().GetFrameView()->ContentClip());
    }
    squashing_layer->SetLayerState(
        state,
        snapped_paint_offset + mapping->SquashingLayerOffsetFromLayoutObject());
  }

  if (auto* mask_layer = mapping->MaskLayer()) {
    auto state = fragment_data.LocalBorderBoxProperties();
    const auto* properties = fragment_data.PaintProperties();
    DCHECK(properties && properties->Mask());
    state.SetEffect(properties->Mask());
    state.SetClip(properties->MaskClip());

    mask_layer->SetLayerState(
        state, snapped_paint_offset + mask_layer->OffsetFromLayoutObject());
  }

  if (auto* ancestor_clipping_mask_layer =
          mapping->AncestorClippingMaskLayer()) {
    PropertyTreeState state(
        fragment_data.PreTransform(),
        mapping->ClipInheritanceAncestor()
            ->GetLayoutObject()
            .FirstFragment()
            .PostOverflowClip(),
        // This is a hack to incorporate mask-based clip-path. Really should be
        // nullptr or some dummy.
        fragment_data.PreFilter());
    ancestor_clipping_mask_layer->SetLayerState(
        state, snapped_paint_offset +
                   ancestor_clipping_mask_layer->OffsetFromLayoutObject());
  }

  if (auto* child_clipping_mask_layer = mapping->ChildClippingMaskLayer()) {
    PropertyTreeState state = fragment_data.LocalBorderBoxProperties();
    // Same hack as for ancestor_clipping_mask_layer.
    state.SetEffect(fragment_data.PreFilter());
    child_clipping_mask_layer->SetLayerState(
        state, snapped_paint_offset +
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
