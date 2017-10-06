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

constexpr gfx::Rect g_large_rect(-200000, -200000, 400000, 400000);
void AppendDisplayItemToCcDisplayItemList(const DisplayItem& display_item,
                                          cc::DisplayItemList& list) {
  DCHECK(display_item.IsDrawing());

  sk_sp<const PaintRecord> record =
      static_cast<const DrawingDisplayItem&>(display_item).GetPaintRecord();
  if (!record)
    return;
  list.StartPaint();
  list.push<cc::DrawRecordOp>(std::move(record));
  // TODO(trchen): Pass correct visual rect here.
  // The visual rect of the item can be used by cc to skip replaying items
  // that can't be seen. To workaround a space conversion bug, the optimization
  // is suppressed by passing a large rect.
  list.EndPaintOfUnpaired(g_large_rect);
}

void AppendRestore(cc::DisplayItemList& list, size_t n) {
  list.StartPaint();
  while (n--)
    list.push<cc::RestoreOp>();
  list.EndPaintOfPairedEnd();
}

class ConversionContext {
 public:
  ConversionContext(const PropertyTreeState& layer_state,
                    cc::DisplayItemList& cc_list)
      : current_transform_(layer_state.Transform()),
        current_clip_(layer_state.Clip()),
        current_effect_(layer_state.Effect()),
        cc_list_(cc_list) {}
  ~ConversionContext();

  // The main function of this class. It converts a list of paint chunks into
  // non-pair display items, and paint properties associated with them are
  // implemented by paired display items.
  // This is done by closing and opening paired items to adjust the current
  // property state to the chunk's state when each chunk is consumed.
  // Note that the clip/effect state is "lazy" in the sense that it stays
  // in whatever state the last chunk left with, and only adjusted when
  // a new chunk is consumed. The class implemented a few helpers to manage
  // state switching so that paired display items are nested properly.
  //
  // State management example (transform tree omitted).
  // Corresponds to unit test PaintChunksToCcLayerTest.InterleavedClipEffect:
  //   Clip tree: C0 <-- C1 <-- C2 <-- C3 <-- C4
  //   Effect tree: E0(clip=C0) <-- E1(clip=C2) <-- E2(clip=C4)
  //   Layer state: C0, E0
  //   Paint chunks: P0(C3, E0), P1(C4, E2), P2(C3, E1), P3(C4, E0)
  // Initialization:
  //   The current state is initalized with the layer state, and starts with
  //   an empty state stack.
  //   current_clip = C0
  //   current_effect = E0
  //   state_stack = []
  // When P0 is consumed, C1, C2 and C3 need to be applied to the state:
  //   Output: Begin_C1 Begin_C2 Begin_C3 Draw_P0
  //   current_clip = C3
  //   state_stack = [C0, C1, C2]
  // When P1 is consumed, C3 needs to be closed before E1 can be entered,
  // then C3 and C4 need to be entered before E2 can be entered:
  //   Output: End_C3 Begin_E1 Begin_C3 Begin_C4 Begin_E2 Draw_P1
  //   current_clip = C4
  //   current_effect = E2
  //   state_stack = [C0, C1, E0, C2, C3, E1]
  // When P2 is consumed, E2 then C4 need to be exited:
  //   Output: End_E2 End_C4 Draw_P2
  //   current_clip = C3
  //   current_effect = E1
  //   state_stack = [C0, C1, E0, C2]
  // When P3 is consumed, C3 must exit before E1 can be exited, then we can
  // enter C3 and C4:
  //   Output: End_C3 End_E1 Enter_C3 Enter_C4 Draw_P3
  //   current_clip = C4
  //   current_effect = E0
  //   state_stack = [C0, C1, C2, C3]
  // At last, close all pushed states to balance pairs (this happens when the
  // context object is destructed):
  //   Output: End_C4 End_C3 End_C2 End_C1
  void Convert(const Vector<const PaintChunk*>&, const DisplayItemList&);

 private:
  // Switch the current clip to the target state, staying in the same effect.
  // It is no-op if the context is already in the target state.
  // Otherwise zero or more clips will be popped from or pushed onto the
  // current state stack.
  // INPUT:
  // The target clip must be a descendant of the input clip of current effect.
  // OUTPUT:
  // The current transform may be changed.
  // The current clip will change to the target clip.
  // The current effect will not change.
  void SwitchToClip(const ClipPaintPropertyNode*);

  // Switch the current effect to the target state.
  // It is no-op if the context is already in the target state.
  // Otherwise zero or more effect effects will be popped from or pushed onto
  // the state stack. As effects getting popped from the stack, clips applied
  // on top of them will be popped as well. Also clips will be pushed at
  // appropriate steps to apply output clip to newly pushed effects.
  // INPUT:
  // The target effect must be a descendant of the layer's effect.
  // OUTPUT:
  // The current transform may be changed.
  // The current clip may be changed, and is guaranteed to be a descendant of
  // the output clip of the target effect.
  // The current effect will change to the target effect.
  void SwitchToEffect(const EffectPaintPropertyNode*);

  const TransformPaintPropertyNode* current_transform_;
  const ClipPaintPropertyNode* current_clip_;
  const EffectPaintPropertyNode* current_effect_;

  // State stack.
  // The size of the stack is the number of nested paired items that are
  // currently nested. Note that this is a "restore stack", i.e. the top
  // element does not represent the current state, but the state prior to
  // applying the last paired begin.
  struct StateEntry {
    // Remembers the type of paired begin that caused a state to be saved.
    // This is useful for emitting corresponding paired end.
    enum class PairedType : char { kClip, kEffect } type;

    const TransformPaintPropertyNode* transform;
    const ClipPaintPropertyNode* clip;
    const EffectPaintPropertyNode* effect;
  };
  Vector<StateEntry> state_stack_;

  cc::DisplayItemList& cc_list_;
};

ConversionContext::~ConversionContext() {
  for (size_t i = state_stack_.size(); i--;) {
    if (state_stack_[i].type == StateEntry::PairedType::kClip) {
      AppendRestore(cc_list_, 1);
    } else {
      DCHECK_EQ(StateEntry::PairedType::kEffect, state_stack_[i].type);
      AppendRestore(cc_list_, 2);
    }
  }
}

void ConversionContext::SwitchToClip(const ClipPaintPropertyNode* target_clip) {
  if (target_clip == current_clip_)
    return;

  // Step 1: Exit all clips until the lowest common ancestor is found.
  const ClipPaintPropertyNode* lca_clip =
      &LowestCommonAncestor(*target_clip, *current_clip_);
  while (current_clip_ != lca_clip) {
    DCHECK(state_stack_.size() &&
           state_stack_.back().type == StateEntry::PairedType::kClip)
        << "Error: Chunk has a clip that escaped its effect's clip.";
    if (!state_stack_.size() ||
        state_stack_.back().type != StateEntry::PairedType::kClip)
      break;
    StateEntry& previous_state = state_stack_.back();
    current_transform_ = previous_state.transform;
    current_clip_ = previous_state.clip;
    DCHECK_EQ(previous_state.effect, current_effect_);
    state_stack_.pop_back();
    AppendRestore(cc_list_, 1);
  }

  // Step 2: Collect all clips between the target clip and the current clip.
  // At this point the current clip must be an ancestor of the target.
  Vector<const ClipPaintPropertyNode*, 1u> pending_clips;
  for (const ClipPaintPropertyNode* clip = target_clip; clip != current_clip_;
       clip = clip->Parent()) {
    // This should never happen unless the DCHECK in step 1 failed.
    if (!clip)
      break;
    pending_clips.push_back(clip);
  }

  // Step 3: Now apply the list of clips in top-down order.
  for (size_t i = pending_clips.size(); i--;) {
    const ClipPaintPropertyNode* sub_clip = pending_clips[i];
    DCHECK_EQ(current_clip_, sub_clip->Parent());

    // Step 3a: Switch CTM to the clip's local space then apply clip.
    cc_list_.StartPaint();
    cc_list_.push<cc::SaveOp>();
    const TransformPaintPropertyNode* target_transform =
        sub_clip->LocalTransformSpace();
    if (current_transform_ != target_transform) {
      cc_list_.push<cc::ConcatOp>(
          static_cast<SkMatrix>(TransformationMatrix::ToSkMatrix44(
              GeometryMapper::SourceToDestinationProjection(
                  target_transform, current_transform_))));
    }
    cc_list_.push<cc::ClipRectOp>(
        static_cast<SkRect>(sub_clip->ClipRect().Rect()), SkClipOp::kIntersect,
        false);
    if (sub_clip->ClipRect().IsRounded()) {
      cc_list_.push<cc::ClipRRectOp>(static_cast<SkRRect>(sub_clip->ClipRect()),
                                     SkClipOp::kIntersect, true);
    }
    cc_list_.EndPaintOfPairedBegin();

    // Step 3b: Adjust state and push previous state onto clip stack.
    state_stack_.emplace_back(StateEntry{StateEntry::PairedType::kClip,
                                         current_transform_, current_clip_,
                                         current_effect_});
    current_transform_ = target_transform;
    current_clip_ = sub_clip;
  }
}

void ConversionContext::SwitchToEffect(
    const EffectPaintPropertyNode* target_effect) {
  if (target_effect == current_effect_)
    return;

  // Step 1: Exit all effects until the lowest common ancestor is found.
  const EffectPaintPropertyNode* lca_effect =
      &LowestCommonAncestor(*target_effect, *current_effect_);
  while (current_effect_ != lca_effect) {
    DCHECK(state_stack_.size()) << "Error: Chunk layerized into a layer with "
                                   "an effect that's too deep.";
    if (!state_stack_.size())
      break;

    StateEntry& previous_state = state_stack_.back();
    if (previous_state.type == StateEntry::PairedType::kClip) {
      AppendRestore(cc_list_, 1);
    } else {
      DCHECK_EQ(StateEntry::PairedType::kEffect, previous_state.type);
      AppendRestore(cc_list_, 2);
    }
    current_transform_ = previous_state.transform;
    current_clip_ = previous_state.clip;
    current_effect_ = previous_state.effect;
    state_stack_.pop_back();
  }

  // Step 2: Collect all effects between the target effect and the current
  // effect. At this point the current effect must be an ancestor of the target.
  Vector<const EffectPaintPropertyNode*, 1u> pending_effects;
  for (const EffectPaintPropertyNode* effect = target_effect;
       effect != current_effect_; effect = effect->Parent()) {
    // This should never happen unless the DCHECK in step 1 failed.
    if (!effect)
      break;
    pending_effects.push_back(effect);
  }

  // Step 3: Now apply the list of effects in top-down order.
  for (size_t i = pending_effects.size(); i--;) {
    const EffectPaintPropertyNode* sub_effect = pending_effects[i];
    DCHECK_EQ(current_effect_, sub_effect->Parent());

    // Step 3a: Before each effect can be applied, we must enter its output
    // clip first, or exit all clips if it doesn't have one.
    if (sub_effect->OutputClip()) {
      SwitchToClip(sub_effect->OutputClip());
    } else {
      while (state_stack_.size() &&
             state_stack_.back().type == StateEntry::PairedType::kClip) {
        StateEntry& previous_state = state_stack_.back();
        current_transform_ = previous_state.transform;
        current_clip_ = previous_state.clip;
        DCHECK_EQ(previous_state.effect, current_effect_);
        state_stack_.pop_back();
        AppendRestore(cc_list_, 1);
      }
    }

    // Step 3b: Apply non-spatial effects first, adjust CTM, then apply spatial
    // effects. Strictly speaking the CTM shall be appled first, it is done
    // in this particular order only to save one SaveOp.
    // TODO(trchen): Omit one of the SaveLayerOp if no-op.
    cc_list_.StartPaint();

    cc::PaintFlags flags;
    flags.setBlendMode(sub_effect->BlendMode());
    // TODO(ajuma): This should really be rounding instead of flooring the
    // alpha value, but that breaks slimming paint reftests.
    flags.setAlpha(
        static_cast<uint8_t>(gfx::ToFlooredInt(255 * sub_effect->Opacity())));
    flags.setColorFilter(GraphicsContext::WebCoreColorFilterToSkiaColorFilter(
        sub_effect->GetColorFilter()));
    cc_list_.push<cc::SaveLayerOp>(nullptr, &flags);

    const TransformPaintPropertyNode* target_transform =
        sub_effect->LocalTransformSpace();
    if (current_transform_ != target_transform) {
      cc_list_.push<cc::ConcatOp>(
          static_cast<SkMatrix>(TransformationMatrix::ToSkMatrix44(
              GeometryMapper::SourceToDestinationProjection(
                  target_transform, current_transform_))));
    }

    // TODO(chrishtr): specify origin of the filter.
    FloatPoint filter_origin;
    cc_list_.push<cc::TranslateOp>(filter_origin.X(), filter_origin.Y());
    // The size parameter is only used to computed the origin of zoom
    // operation, which we never generate.
    gfx::SizeF empty;
    cc::PaintFlags filter_flags;
    filter_flags.setImageFilter(cc::RenderSurfaceFilters::BuildImageFilter(
        sub_effect->Filter().AsCcFilterOperations(), empty));
    cc_list_.push<cc::SaveLayerOp>(nullptr, &filter_flags);
    cc_list_.push<cc::TranslateOp>(-filter_origin.X(), -filter_origin.Y());

    cc_list_.EndPaintOfPairedBegin();

    // Step 3c: Adjust state and push previous state onto effect stack.
    // TODO(trchen): Change input clip to expansion hint once implemented.
    const ClipPaintPropertyNode* input_clip = current_clip_;
    state_stack_.emplace_back(StateEntry{StateEntry::PairedType::kEffect,
                                         current_transform_, current_clip_,
                                         current_effect_});
    current_transform_ = target_transform;
    current_clip_ = input_clip;
    current_effect_ = sub_effect;
  }
}

void ConversionContext::Convert(const Vector<const PaintChunk*>& paint_chunks,
                                const DisplayItemList& display_items) {
  for (auto chunk_it = paint_chunks.begin(); chunk_it != paint_chunks.end();
       chunk_it++) {
    const PaintChunk& chunk = **chunk_it;
    const PropertyTreeState& chunk_state = chunk.properties.property_tree_state;
    SwitchToEffect(chunk_state.Effect());
    SwitchToClip(chunk_state.Clip());
    bool transformed = chunk_state.Transform() != current_transform_;
    if (transformed) {
      cc_list_.StartPaint();
      cc_list_.push<cc::SaveOp>();
      cc_list_.push<cc::ConcatOp>(
          static_cast<SkMatrix>(TransformationMatrix::ToSkMatrix44(
              GeometryMapper::SourceToDestinationProjection(
                  chunk_state.Transform(), current_transform_))));
      cc_list_.EndPaintOfPairedBegin();
    }
    for (const auto& item : display_items.ItemsInPaintChunk(chunk))
      AppendDisplayItemToCcDisplayItemList(item, cc_list_);
    if (transformed)
      AppendRestore(cc_list_, 1);
  }
}

}  // unnamed namespace

void PaintChunksToCcLayer::ConvertInto(
    const Vector<const PaintChunk*>& paint_chunks,
    const PropertyTreeState& layer_state,
    const gfx::Vector2dF& layer_offset,
    const DisplayItemList& display_items,
    cc::DisplayItemList& cc_list) {
  bool need_translate = !layer_offset.IsZero();
  if (need_translate) {
    cc_list.StartPaint();
    cc_list.push<cc::SaveOp>();
    cc_list.push<cc::TranslateOp>(-layer_offset.x(), -layer_offset.y());
    cc_list.EndPaintOfPairedBegin();
  }

  ConversionContext(layer_state, cc_list).Convert(paint_chunks, display_items);

  if (need_translate)
    AppendRestore(cc_list, 1);
}

scoped_refptr<cc::DisplayItemList> PaintChunksToCcLayer::Convert(
    const Vector<const PaintChunk*>& paint_chunks,
    const PropertyTreeState& layer_state,
    const gfx::Vector2dF& layer_offset,
    const DisplayItemList& display_items,
    cc::DisplayItemList::UsageHint hint,
    RasterUnderInvalidationCheckingParams* under_invalidation_checking_params) {
  auto cc_list = base::MakeRefCounted<cc::DisplayItemList>(hint);
  ConvertInto(paint_chunks, layer_state, layer_offset, display_items, *cc_list);

  if (under_invalidation_checking_params) {
    auto& params = *under_invalidation_checking_params;
    PaintRecorder recorder;
    recorder.beginRecording(params.interest_rect);
    // Create a complete cloned list for under-invalidation checking. We can't
    // use cc_list because it is not finalized yet.
    auto list_clone = base::MakeRefCounted<cc::DisplayItemList>(
        cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer);
    ConvertInto(paint_chunks, layer_state, layer_offset, display_items,
                *list_clone);
    recorder.getRecordingCanvas()->drawPicture(list_clone->ReleaseAsRecord());
    params.tracking.CheckUnderInvalidations(params.debug_name,
                                            recorder.finishRecordingAsPicture(),
                                            params.interest_rect);
    if (auto record = params.tracking.UnderInvalidationRecord()) {
      cc_list->StartPaint();
      cc_list->push<cc::DrawRecordOp>(std::move(record));
      cc_list->EndPaintOfUnpaired(g_large_rect);
    }
  }

  cc_list->Finalize();
  return cc_list;
}

}  // namespace blink
