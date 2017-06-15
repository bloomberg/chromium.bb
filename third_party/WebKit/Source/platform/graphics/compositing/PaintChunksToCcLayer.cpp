// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintChunksToCcLayer.h"

#include "cc/base/render_surface_filters.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/GeometryMapper.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/PropertyTreeState.h"
#include "platform/graphics/paint/RasterInvalidationTracking.h"

namespace blink {

namespace {

// Applies the clips between |localState| and |ancestorState| into a single
// combined cc::FloatClipDisplayItem on |ccList|.
static void ApplyClipsBetweenStates(const PropertyTreeState& local_state,
                                    const PropertyTreeState& ancestor_state,
                                    cc::DisplayItemList& cc_list,
                                    Vector<int>& needed_restores) {
  DCHECK(local_state.Transform() == ancestor_state.Transform());
#if DCHECK_IS_ON()
  const TransformPaintPropertyNode* transform_node =
      local_state.Clip()->LocalTransformSpace();
  if (transform_node != ancestor_state.Transform()) {
    const TransformationMatrix& local_to_ancestor_matrix =
        GeometryMapper::LocalToAncestorMatrix(transform_node,
                                              ancestor_state.Transform());
    // Clips are only in descendant spaces that are transformed by one
    // or more scrolls.
    DCHECK(local_to_ancestor_matrix.IsIdentityOrTranslation());
  }
#endif

  const FloatClipRect& combined_clip =
      GeometryMapper::LocalToAncestorClipRect(local_state, ancestor_state);
  bool antialias = false;

  {
    cc::PaintOpBuffer* buffer = cc_list.StartPaint();
    buffer->push<cc::SaveOp>();
    buffer->push<cc::ClipRectOp>(combined_clip.Rect(), SkClipOp::kIntersect,
                                 antialias);
    cc_list.EndPaintOfPairedBegin();
  }
  needed_restores.push_back(1);
}

static void RecordPairedBeginDisplayItems(
    const Vector<PropertyTreeState>& paired_states,
    const PropertyTreeState& pending_layer_state,
    cc::DisplayItemList& cc_list,
    Vector<int>& needed_restores) {
  PropertyTreeState mapped_clip_destination_space = pending_layer_state;
  PropertyTreeState clip_space = pending_layer_state;
  bool has_clip = false;

  for (Vector<PropertyTreeState>::const_reverse_iterator paired_state =
           paired_states.rbegin();
       paired_state != paired_states.rend(); ++paired_state) {
    switch (paired_state->GetInnermostNode()) {
      case PropertyTreeState::kTransform: {
        if (has_clip) {
          ApplyClipsBetweenStates(clip_space, mapped_clip_destination_space,
                                  cc_list, needed_restores);
          has_clip = false;
        }
        mapped_clip_destination_space = *paired_state;
        clip_space = *paired_state;

        TransformationMatrix matrix = paired_state->Transform()->Matrix();
        matrix.ApplyTransformOrigin(paired_state->Transform()->Origin());

        SkMatrix skmatrix =
            static_cast<SkMatrix>(TransformationMatrix::ToSkMatrix44(matrix));
        {
          cc::PaintOpBuffer* buffer = cc_list.StartPaint();
          buffer->push<cc::SaveOp>();
          buffer->push<cc::ConcatOp>(skmatrix);
          cc_list.EndPaintOfPairedBegin();
        }
        needed_restores.push_back(1);
        break;
      }
      case PropertyTreeState::kClip: {
        // Clips are handled in |applyClips| when ending the iterator, or
        // transitioning between transform spaces. Here we store off the
        // PropertyTreeState of the first found clip, under the transform of
        // pairedState->transform(). All subsequent clips before applying the
        // transform will be applied in applyClips.
        clip_space = *paired_state;
        has_clip = true;
#if DCHECK_IS_ON()
        if (paired_state->Clip()->LocalTransformSpace() !=
            paired_state->Transform()) {
          const TransformationMatrix& local_transform_matrix =
              paired_state->Effect()->LocalTransformSpace()->Matrix();
          // Clips are only in descendant spaces that are transformed by scroll.
          DCHECK(local_transform_matrix.IsIdentityOrTranslation());
        }
#endif
        break;
      }
      case PropertyTreeState::kEffect: {
        // TODO(chrishtr): skip effect and/or compositing display items if
        // not necessary.

        FloatRect clip_rect =
            paired_state->Effect()->OutputClip()->ClipRect().Rect();
        // TODO(chrishtr): specify origin of the filter.
        FloatPoint filter_origin;
        if (paired_state->Effect()->LocalTransformSpace() !=
            paired_state->Transform()) {
          const TransformPaintPropertyNode* transform_node =
              paired_state->Effect()->LocalTransformSpace();
          const TransformationMatrix& local_to_ancestor_matrix =
              GeometryMapper::LocalToAncestorMatrix(transform_node,
                                                    paired_state->Transform());
          // Effects are only in descendant spaces that are transformed by one
          // or more scrolls.
          DCHECK(local_to_ancestor_matrix.IsIdentityOrTranslation());

          clip_rect = local_to_ancestor_matrix.MapRect(clip_rect);
          filter_origin = local_to_ancestor_matrix.MapPoint(filter_origin);
        }

        {
          cc::PaintFlags flags;
          flags.setBlendMode(paired_state->Effect()->BlendMode());
          // TODO(ajuma): This should really be rounding instead of flooring the
          // alpha value, but that breaks slimming paint reftests.
          flags.setAlpha(static_cast<uint8_t>(
              gfx::ToFlooredInt(255 * paired_state->Effect()->Opacity())));
          flags.setColorFilter(
              GraphicsContext::WebCoreColorFilterToSkiaColorFilter(
                  paired_state->Effect()->GetColorFilter()));

          cc::PaintOpBuffer* buffer = cc_list.StartPaint();
          // TODO(chrishtr): compute bounds as necessary.
          buffer->push<cc::SaveLayerOp>(nullptr, &flags);
          cc_list.EndPaintOfPairedBegin();
        }
        needed_restores.push_back(1);

        {
          cc::PaintOpBuffer* buffer = cc_list.StartPaint();

          buffer->push<cc::SaveOp>();
          buffer->push<cc::TranslateOp>(filter_origin.X(), filter_origin.Y());

          cc::PaintFlags flags;
          flags.setImageFilter(cc::RenderSurfaceFilters::BuildImageFilter(
              paired_state->Effect()->Filter().AsCcFilterOperations(),
              gfx::SizeF(clip_rect.Width(), clip_rect.Height())));

          SkRect layer_bounds = clip_rect;
          layer_bounds.offset(-filter_origin.X(), -filter_origin.Y());
          buffer->push<cc::SaveLayerOp>(&layer_bounds, &flags);
          buffer->push<cc::TranslateOp>(-filter_origin.X(), -filter_origin.Y());

          cc_list.EndPaintOfPairedBegin();
        }
        // The SaveOp+SaveLayerOp above are grouped such that they share a
        // visual rect, so group the two restores in the same way so we don't
        // have a mismatch in the number of EndPaintOfPairedBegin() vs
        // EndPaintOfPairedEnd().
        needed_restores.push_back(2);
        break;
      }
      case PropertyTreeState::kNone:
        break;
    }
  }

  if (has_clip) {
    ApplyClipsBetweenStates(clip_space, mapped_clip_destination_space, cc_list,
                            needed_restores);
  }
}

static void RecordPairedEndDisplayItems(const Vector<int>& needed_restores,
                                        cc::DisplayItemList& cc_list) {
  // TODO(danakj): This loop could use base::Reversed once it's allowed here.
  for (auto it = needed_restores.rbegin(); it != needed_restores.rend(); ++it) {
    cc::PaintOpBuffer* buffer = cc_list.StartPaint();
    int num_restores = *it;
    for (int i = 0; i < num_restores; ++i)
      buffer->push<cc::RestoreOp>();
    cc_list.EndPaintOfPairedEnd();
  }
}

static gfx::Rect g_large_rect(-200000, -200000, 400000, 400000);
static void AppendDisplayItemToCcDisplayItemList(
    const DisplayItem& display_item,
    cc::DisplayItemList& cc_list) {
  DCHECK(DisplayItem::IsDrawingType(display_item.GetType()));
  if (DisplayItem::IsDrawingType(display_item.GetType())) {
    const auto& drawing_display_item =
        static_cast<const DrawingDisplayItem&>(display_item);
    sk_sp<const cc::PaintOpBuffer> record =
        drawing_display_item.GetPaintRecord();
    if (!record)
      return;
    // In theory we would pass the bounds of the record, previously done as:
    // gfx::Rect bounds = gfx::SkIRectToRect(record->cullRect().roundOut());
    // or use the visual rect directly. However, clip content layers attempt
    // to raster in a different space than that of the visual rects. We'll be
    // reworking visual rects further for SPv2, so for now we just pass a
    // visual rect large enough to make sure items raster.
    {
      cc::PaintOpBuffer* buffer = cc_list.StartPaint();
      buffer->push<cc::DrawRecordOp>(std::move(record));
      cc_list.EndPaintOfUnpaired(g_large_rect);
    }
  }
}

}  // unnamed namespace

scoped_refptr<cc::DisplayItemList> PaintChunksToCcLayer::Convert(
    const Vector<const PaintChunk*>& paint_chunks,
    const PropertyTreeState& layer_state,
    const gfx::Vector2dF& layer_offset,
    const DisplayItemList& display_items,
    RasterUnderInvalidationCheckingParams* under_invalidation_checking_params) {
  auto cc_list = make_scoped_refptr(new cc::DisplayItemList);

  bool need_translate = !layer_offset.IsZero();
  if (need_translate) {
    cc::PaintOpBuffer* buffer = cc_list->StartPaint();
    buffer->push<cc::SaveOp>();
    buffer->push<cc::TranslateOp>(-layer_offset.x(), -layer_offset.y());
    cc_list->EndPaintOfPairedBegin();
  }

  for (const auto* paint_chunk : paint_chunks) {
    const PropertyTreeState* state =
        &paint_chunk->properties.property_tree_state;
    PropertyTreeStateIterator iterator(*state);
    Vector<PropertyTreeState> paired_states;
    for (; state && *state != layer_state; state = iterator.Next()) {
      if (state->GetInnermostNode() != PropertyTreeState::kNone)
        paired_states.push_back(*state);
    }

    // TODO(chrishtr): we can avoid some extra paired display items if
    // multiple PaintChunks share them. We can also collapse clips between
    // transforms into single clips in the same way that PaintLayerClipper does.
    Vector<int> needed_restores;

    RecordPairedBeginDisplayItems(paired_states, layer_state, *cc_list,
                                  needed_restores);

    for (const auto& display_item :
         display_items.ItemsInPaintChunk(*paint_chunk))
      AppendDisplayItemToCcDisplayItemList(display_item, *cc_list);

    RecordPairedEndDisplayItems(needed_restores, *cc_list);
  }

  if (need_translate) {
    cc::PaintOpBuffer* buffer = cc_list->StartPaint();
    buffer->push<cc::RestoreOp>();
    cc_list->EndPaintOfPairedEnd();
  }

  if (under_invalidation_checking_params) {
    auto& params = *under_invalidation_checking_params;
    PaintRecorder recorder;
    recorder.beginRecording(params.interest_rect);
    // Create a complete cloned list for under-invalidation checking. We can't
    // use cc_list because it is not finalized yet.
    auto list_clone =
        Convert(paint_chunks, layer_state, layer_offset, display_items);
    recorder.getRecordingCanvas()->drawPicture(list_clone->ReleaseAsRecord());
    params.tracking.CheckUnderInvalidations(params.debug_name,
                                            recorder.finishRecordingAsPicture(),
                                            params.interest_rect);
    if (auto record = params.tracking.under_invalidation_record) {
      cc::PaintOpBuffer* buffer = cc_list->StartPaint();
      buffer->push<cc::DrawRecordOp>(record);
      cc_list->EndPaintOfUnpaired(g_large_rect);
    }
  }

  cc_list->Finalize();
  return cc_list;
}

}  // namespace blink
