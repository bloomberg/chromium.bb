// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "cc/document_transition/document_transition_request.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_flags.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/mutator_host.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/compositing/content_layer_client_impl.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scrollbar_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

// cc property trees make use of a sequence number to identify when tree
// topology changes. For now we naively increment the sequence number each time
// we update the property trees. We should explore optimizing our management of
// the sequence number through the use of a dirty bit or similar. See
// http://crbug.com/692842#c4.
static int g_s_property_tree_sequence_number = 1;

class PaintArtifactCompositor::OldPendingLayerMatcher {
 public:
  explicit OldPendingLayerMatcher(PendingLayers pending_layers)
      : pending_layers_(std::move(pending_layers)) {}

  // Finds the next PendingLayer that can be matched by |new_layer|.
  // It's efficient if most of the pending layers can be matched sequentially.
  PendingLayer* Find(const PendingLayer& new_layer) {
    if (pending_layers_.IsEmpty())
      return nullptr;
    if (!new_layer.FirstPaintChunk().CanMatchOldChunk())
      return nullptr;
    wtf_size_t i = next_index_;
    do {
      wtf_size_t next = (i + 1) % pending_layers_.size();
      if (new_layer.Matches(pending_layers_[i])) {
        next_index_ = next;
        return &pending_layers_[i];
      }
      i = next;
    } while (i != next_index_);
    return nullptr;
  }

 private:
  wtf_size_t next_index_ = 0;
  PendingLayers pending_layers_;
};

PaintArtifactCompositor::PaintArtifactCompositor(
    base::WeakPtr<CompositorScrollCallbacks> scroll_callbacks)
    : scroll_callbacks_(std::move(scroll_callbacks)),
      tracks_raster_invalidations_(VLOG_IS_ON(3)) {
  root_layer_ = cc::Layer::Create();
}

PaintArtifactCompositor::~PaintArtifactCompositor() {}

void PaintArtifactCompositor::SetTracksRasterInvalidations(bool should_track) {
  tracks_raster_invalidations_ = should_track || VLOG_IS_ON(3);
  for (auto& pending_layer : pending_layers_) {
    if (auto* client = pending_layer.GetContentLayerClient())
      client->GetRasterInvalidator().SetTracksRasterInvalidations(should_track);
  }
}

void PaintArtifactCompositor::WillBeRemovedFromFrame() {
  root_layer_->RemoveAllChildren();
}

void PaintArtifactCompositor::SetPrefersLCDText(bool prefers) {
  if (prefers_lcd_text_ == prefers)
    return;
  SetNeedsUpdate();
  prefers_lcd_text_ = prefers;
}

std::unique_ptr<JSONArray> PaintArtifactCompositor::GetPendingLayersAsJSON()
    const {
  std::unique_ptr<JSONArray> result = std::make_unique<JSONArray>();
  for (const PendingLayer& pending_layer : pending_layers_)
    result->PushObject(pending_layer.ToJSON());
  return result;
}

// Get a JSON representation of what layers exist for this PAC.
std::unique_ptr<JSONObject> PaintArtifactCompositor::GetLayersAsJSON(
    LayerTreeFlags flags) const {
  if (!tracks_raster_invalidations_) {
    flags &= ~(kLayerTreeIncludesInvalidations |
               kLayerTreeIncludesDetailedInvalidations);
  }

  LayersAsJSON layers_as_json(flags);
  for (const auto& layer : root_layer_->children()) {
    const LayerAsJSONClient* json_client = nullptr;
    const TransformPaintPropertyNode* transform = nullptr;
    for (const auto& pending_layer : pending_layers_) {
      if (layer.get() == &pending_layer.CcLayer()) {
        json_client = pending_layer.GetContentLayerClient();
        transform = &pending_layer.GetPropertyTreeState().Transform();
        break;
      }
    }
    if (!transform) {
      for (const auto& pending_layer : pending_layers_) {
        if (pending_layer.GetPropertyTreeState().Transform().CcNodeId(
                layer->property_tree_sequence_number()) ==
            layer->transform_tree_index()) {
          transform = &pending_layer.GetPropertyTreeState().Transform();
          break;
        }
      }
    }
    DCHECK(transform);
    layers_as_json.AddLayer(*layer, *transform, json_client);
  }
  return layers_as_json.Finalize();
}

const TransformPaintPropertyNode&
PaintArtifactCompositor::NearestScrollTranslationForLayer(
    const PendingLayer& pending_layer) {
  if (pending_layer.GetCompositingType() == PendingLayer::kScrollHitTestLayer)
    return pending_layer.ScrollTranslationForScrollHitTestLayer();

  const auto& transform = pending_layer.GetPropertyTreeState().Transform();
  // TODO(pdr): This could be a performance issue because it crawls up the
  // transform tree for each pending layer. If this is on profiles, we should
  // cache a lookup of transform node to scroll translation transform node.
  return transform.NearestScrollTranslationNode();
}

namespace {

cc::Layer* ForeignLayer(const PaintChunk& chunk,
                        const PaintArtifact& artifact) {
  if (chunk.size() != 1)
    return nullptr;
  const auto& first_display_item =
      artifact.GetDisplayItemList()[chunk.begin_index];
  auto* foreign_layer = DynamicTo<ForeignLayerDisplayItem>(first_display_item);
  return foreign_layer ? foreign_layer->GetLayer() : nullptr;
}

// True if the paint chunk change affects the result of |Update|, such as the
// compositing decisions in |CollectPendingLayers|. This will return false for
// repaint updates that can be handled by |UpdateRepaintedLayers|, such as
// background color changes.
bool NeedsFullUpdateAfterPaintingChunk(
    const PaintChunk& previous,
    const PaintArtifact& previous_artifact,
    const PaintChunk& repainted,
    const PaintArtifact& repainted_artifact) {
  if (!repainted.Matches(previous))
    return true;

  if (repainted.is_moved_from_cached_subsequence) {
    DCHECK_EQ(previous.bounds, repainted.bounds);
    DCHECK_EQ(previous.DrawsContent(), repainted.DrawsContent());
    DCHECK_EQ(previous.rect_known_to_be_opaque,
              repainted.rect_known_to_be_opaque);
    DCHECK_EQ(previous.text_known_to_be_on_opaque_background,
              repainted.text_known_to_be_on_opaque_background);
    DCHECK_EQ(previous.has_text, repainted.has_text);

    // Debugging for https://crbug.com/1237389 and https://crbug.com/1230104.
    // Before returning that a full update is not needed, check that the
    // properties are changed, which would indicate a missing call to
    // SetNeedsUpdate.
    if (previous.properties != repainted.properties) {
      NOTREACHED();
      return true;
    }

    // Not checking ForeignLayer() here because the old ForeignDisplayItem
    // was set to 0 when we moved the cached subsequence. This is also the
    // reason why we check is_moved_from_cached_subsequence before checking
    // ForeignLayer().
    return false;
  }

  // Bounds are used in overlap testing.
  // TODO(pdr): If the bounds shrink, that does affect overlap testing but we
  // could return false to continue using less-than-optimal overlap testing in
  // order to save a full compositing update.
  if (previous.bounds != repainted.bounds)
    return true;

  // Changing foreign layers requires a full update to push the new cc::Layers.
  if (ForeignLayer(previous, previous_artifact) !=
      ForeignLayer(repainted, repainted_artifact)) {
    return true;
  }

  // Opaqueness of individual chunks is used to set the cc::Layer's contents
  // opaque property.
  if (previous.rect_known_to_be_opaque != repainted.rect_known_to_be_opaque)
    return true;
  // Similar to opaqueness, opaqueness for text is used to set the cc::Layer's
  // contents opaque for text property.
  if (previous.text_known_to_be_on_opaque_background !=
      repainted.text_known_to_be_on_opaque_background) {
    return true;
  }
  // |has_text| affects compositing decisions (see:
  // |PendingLayer::MergeInternal|).
  if (previous.has_text != repainted.has_text)
    return true;

  // |PaintChunk::DrawsContent()| affects whether a layer draws content which
  // affects whether mask layers are created (see:
  // |SwitchToEffectNodeWithSynthesizedClip|).
  if (previous.DrawsContent() != repainted.DrawsContent())
    return true;

  // Debugging for https://crbug.com/1237389 and https://crbug.com/1230104.
  // Before returning that a full update is not needed, check that the
  // properties are changed, which would indicate a missing call to
  // SetNeedsUpdate.
  if (previous.properties != repainted.properties) {
    NOTREACHED();
    return true;
  }

  return false;
}

}  // namespace

void PaintArtifactCompositor::SetNeedsFullUpdateAfterPaintIfNeeded(
    const PaintChunkSubset& previous,
    const PaintChunkSubset& repainted) {
  if (needs_update_)
    return;

  // Adding or removing chunks requires a full update to add/remove cc::layers.
  if (previous.size() != repainted.size()) {
    SetNeedsUpdate();
    return;
  }

  // Loop over both paint chunk subsets in order.
  auto previous_chunk_it = previous.begin();
  auto repainted_chunk_it = repainted.begin();
  for (; previous_chunk_it != previous.end();
       ++previous_chunk_it, ++repainted_chunk_it) {
    const auto& previous_chunk = *previous_chunk_it;
    const auto& repainted_chunk = *repainted_chunk_it;
    if (NeedsFullUpdateAfterPaintingChunk(
            previous_chunk, previous.GetPaintArtifact(), repainted_chunk,
            repainted.GetPaintArtifact())) {
      SetNeedsUpdate();
      return;
    }
  }
}

bool PaintArtifactCompositor::HasComposited(
    CompositorElementId element_id) const {
  // |Update| creates PropertyTrees on the LayerTreeHost to represent the
  // composited page state. Check if it has created a property tree node for
  // the given |element_id|.
  DCHECK(!NeedsUpdate()) << "This should only be called after an update";
  return root_layer_->layer_tree_host()->property_trees()->HasElement(
      element_id);
}

// Returns nullptr if 'ancestor' is not a strict ancestor of 'node'.
// Otherwise, return the child of 'ancestor' that is an ancestor of 'node' or
// 'node' itself.
static const EffectPaintPropertyNode* StrictUnaliasedChildOfAlongPath(
    const EffectPaintPropertyNode& ancestor,
    const EffectPaintPropertyNode& node) {
  const auto* n = &node;
  while (n) {
    const auto* parent = n->UnaliasedParent();
    if (parent == &ancestor)
      return n;
    n = parent;
  }
  return nullptr;
}

bool PaintArtifactCompositor::DecompositeEffect(
    const EffectPaintPropertyNode& parent_effect,
    wtf_size_t first_layer_in_parent_group_index,
    const EffectPaintPropertyNode& effect,
    wtf_size_t layer_index) {
  // The layer must be the last layer in pending_layers_.
  DCHECK_EQ(layer_index, pending_layers_.size() - 1);

  // If the effect associated with the layer is deeper than than the effect
  // we are attempting to decomposite, than implies some previous decision
  // did not allow to decomposite intermediate effects.
  PendingLayer& layer = pending_layers_[layer_index];
  if (&layer.GetPropertyTreeState().Effect() != &effect)
    return false;
  if (layer.ChunkRequiresOwnLayer())
    return false;
  if (effect.HasDirectCompositingReasons())
    return false;

  PropertyTreeState group_state(effect.LocalTransformSpace().Unalias(),
                                effect.OutputClip()
                                    ? effect.OutputClip()->Unalias()
                                    : layer.GetPropertyTreeState().Clip(),
                                effect);
  absl::optional<PropertyTreeState> upcast_state =
      group_state.CanUpcastWith(layer.GetPropertyTreeState());
  if (!upcast_state)
    return false;

  upcast_state->SetEffect(parent_effect);

  // Exotic blending layer can be decomposited only if its parent group
  // (which defines the scope of the blending) has zero or one layer before it,
  // and it can be merged into that layer. However, a layer not drawing content
  // at the beginning of the parent group doesn't count, as the blending mode
  // doesn't apply to it.
  if (effect.BlendMode() != SkBlendMode::kSrcOver) {
    auto num_previous_siblings =
        layer_index - first_layer_in_parent_group_index;
    if (num_previous_siblings) {
      if (num_previous_siblings > 2)
        return false;
      if (num_previous_siblings == 2 &&
          pending_layers_[first_layer_in_parent_group_index].DrawsContent())
        return false;
      const auto& previous_sibling = pending_layers_[layer_index - 1];
      if (previous_sibling.DrawsContent() &&
          !previous_sibling.CanMerge(layer, *upcast_state, prefers_lcd_text_))
        return false;
    }
  }

  layer.Upcast(*upcast_state);
  return true;
}

void PaintArtifactCompositor::LayerizeGroup(
    const PaintChunkSubset& chunks,
    const EffectPaintPropertyNode& current_group,
    PaintChunkIterator& chunk_cursor,
    bool force_draws_content) {
  wtf_size_t first_layer_in_current_group = pending_layers_.size();
  // The worst case time complexity of the algorithm is O(pqd), where
  // p = the number of paint chunks.
  // q = average number of trials to find a squash layer or rejected
  //     for overlapping.
  // d = (sum of) the depth of property trees.
  // The analysis as follows:
  // Every paint chunk will be visited by the main loop below for exactly
  // once, except for chunks that enter or exit groups (case B & C below). For
  // normal chunk visit (case A), the only cost is determining squash, which
  // costs O(qd), where d came from |CanUpcastWith| and geometry mapping.
  // Subtotal: O(pqd)
  // For group entering and exiting, it could cost O(d) for each group, for
  // searching the shallowest subgroup (StrictChildOfAlongPath), thus O(d^2)
  // in total.
  // Also when exiting group, the group may be decomposited and squashed to a
  // previous layer. Again finding the host costs O(qd). Merging would cost
  // O(p) due to copying the chunk list. Subtotal: O((qd + p)d) = O(qd^2 + pd)
  // Assuming p > d, the total complexity would be O(pqd + qd^2 + pd) = O(pqd)
  while (chunk_cursor != chunks.end()) {
    // Look at the effect node of the next chunk. There are 3 possible cases:
    // A. The next chunk belongs to the current group but no subgroup.
    // B. The next chunk does not belong to the current group.
    // C. The next chunk belongs to some subgroup of the current group.
    const auto& chunk_effect = chunk_cursor->properties.Effect().Unalias();
    if (&chunk_effect == &current_group) {
      pending_layers_.emplace_back(chunks, chunk_cursor);
      ++chunk_cursor;
      // force_draws_content doesn't apply to pending layers that require own
      // layer, specifically scrollbar layers, foreign layers, scroll hit
      // testing layers.
      if (pending_layers_.back().ChunkRequiresOwnLayer())
        continue;
    } else {
      const EffectPaintPropertyNode* subgroup =
          StrictUnaliasedChildOfAlongPath(current_group, chunk_effect);
      // Case B: This means we need to close the current group without
      //         processing the next chunk.
      if (!subgroup)
        break;
      // Case C: The following chunks belong to a subgroup. Process them by
      //         a recursion call.
      wtf_size_t first_layer_in_subgroup = pending_layers_.size();
      LayerizeGroup(chunks, *subgroup, chunk_cursor,
                    force_draws_content || subgroup->DrawsContent());
      // The above LayerizeGroup generated new layers in pending_layers_
      // [first_layer_in_subgroup .. pending_layers.size() - 1]. If it
      // generated 2 or more layer that we already know can't be merged
      // together, we should not decomposite and try to merge any of them into
      // the previous layers.
      if (first_layer_in_subgroup != pending_layers_.size() - 1)
        continue;
      if (!DecompositeEffect(current_group, first_layer_in_current_group,
                             *subgroup, first_layer_in_subgroup))
        continue;
    }
    // At this point pending_layers_.back() is the either a layer from a
    // "decomposited" subgroup or a layer created from a chunk we just
    // processed. Now determine whether it could be merged into a previous
    // layer.
    PendingLayer& new_layer = pending_layers_.back();
    DCHECK(!new_layer.ChunkRequiresOwnLayer());
    DCHECK_EQ(&current_group, &new_layer.GetPropertyTreeState().Effect());
    if (force_draws_content)
      new_layer.ForceDrawsContent();
    // This iterates pending_layers_[first_layer_in_current_group:-1] in
    // reverse.
    for (wtf_size_t candidate_index = pending_layers_.size() - 1;
         candidate_index-- > first_layer_in_current_group;) {
      PendingLayer& candidate_layer = pending_layers_[candidate_index];
      if (candidate_layer.Merge(new_layer, prefers_lcd_text_)) {
        pending_layers_.pop_back();
        break;
      }
      if (new_layer.MightOverlap(candidate_layer)) {
        new_layer.SetCompositingType(PendingLayer::kOverlap);
        break;
      }
    }
  }
}

void PaintArtifactCompositor::CollectPendingLayers(
    scoped_refptr<const PaintArtifact> artifact) {
  PaintChunkSubset subset(artifact);
  auto cursor = subset.begin();
  LayerizeGroup(subset, EffectPaintPropertyNode::Root(), cursor,
                /*force_draws_content*/ false);
  DCHECK(cursor == subset.end());
  pending_layers_.ShrinkToReasonableCapacity();
}

void SynthesizedClip::UpdateLayer(bool needs_layer,
                                  const ClipPaintPropertyNode& clip,
                                  const TransformPaintPropertyNode& transform) {
  if (!needs_layer) {
    layer_.reset();
    return;
  }
  if (!layer_) {
    layer_ = cc::PictureLayer::Create(this);
    layer_->SetIsDrawable(true);
    // The clip layer must be hit testable because the compositor may not know
    // whether the hit test is clipped out.
    // See: cc::LayerTreeHostImpl::IsInitialScrollHitTestReliable().
    layer_->SetHitTestable(true);
  }

  const RefCountedPath* path = clip.ClipPath();
  SkRRect new_rrect(clip.PaintClipRect());
  gfx::Rect layer_bounds = gfx::ToEnclosingRect(clip.PaintClipRect().Rect());
  bool needs_display = false;

  auto new_translation_2d_or_matrix =
      GeometryMapper::SourceToDestinationProjection(clip.LocalTransformSpace(),
                                                    transform);
  new_translation_2d_or_matrix.MapRect(layer_bounds);
  new_translation_2d_or_matrix.PostTranslate(-layer_bounds.x(),
                                             -layer_bounds.y());

  if (!path && new_translation_2d_or_matrix.IsIdentityOr2DTranslation()) {
    const auto& translation = new_translation_2d_or_matrix.Translation2D();
    new_rrect.offset(translation.x(), translation.y());
    needs_display = !rrect_is_local_ || new_rrect != rrect_;
    translation_2d_or_matrix_ = GeometryMapper::Translation2DOrMatrix();
    rrect_is_local_ = true;
  } else {
    needs_display = rrect_is_local_ || new_rrect != rrect_ ||
                    new_translation_2d_or_matrix != translation_2d_or_matrix_ ||
                    (path_ != path && (!path_ || !path || *path_ != *path));
    translation_2d_or_matrix_ = new_translation_2d_or_matrix;
    rrect_is_local_ = false;
  }

  if (needs_display)
    layer_->SetNeedsDisplay();

  layer_->SetOffsetToTransformParent(
      gfx::Vector2dF(layer_bounds.OffsetFromOrigin()));
  layer_->SetBounds(layer_bounds.size());
  rrect_ = new_rrect;
  path_ = path;
}

scoped_refptr<cc::DisplayItemList>
SynthesizedClip::PaintContentsToDisplayList() {
  auto cc_list = base::MakeRefCounted<cc::DisplayItemList>(
      cc::DisplayItemList::kTopLevelDisplayItemList);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  cc_list->StartPaint();
  if (rrect_is_local_) {
    cc_list->push<cc::DrawRRectOp>(rrect_, flags);
  } else {
    cc_list->push<cc::SaveOp>();
    if (translation_2d_or_matrix_.IsIdentityOr2DTranslation()) {
      const auto& translation = translation_2d_or_matrix_.Translation2D();
      cc_list->push<cc::TranslateOp>(translation.x(), translation.y());
    } else {
      cc_list->push<cc::ConcatOp>(translation_2d_or_matrix_.ToSkM44());
    }
    if (path_) {
      cc_list->push<cc::ClipPathOp>(path_->GetSkPath(), SkClipOp::kIntersect,
                                    true);
    }
    cc_list->push<cc::DrawRRectOp>(rrect_, flags);
    cc_list->push<cc::RestoreOp>();
  }
  cc_list->EndPaintOfUnpaired(gfx::Rect(layer_->bounds()));
  cc_list->Finalize();
  return cc_list;
}

SynthesizedClip& PaintArtifactCompositor::CreateOrReuseSynthesizedClipLayer(
    const ClipPaintPropertyNode& clip,
    const TransformPaintPropertyNode& transform,
    bool needs_layer,
    CompositorElementId& mask_isolation_id,
    CompositorElementId& mask_effect_id) {
  auto* entry =
      std::find_if(synthesized_clip_cache_.begin(),
                   synthesized_clip_cache_.end(), [&clip](const auto& entry) {
                     return entry.key == &clip && !entry.in_use;
                   });
  if (entry == synthesized_clip_cache_.end()) {
    synthesized_clip_cache_.push_back(SynthesizedClipEntry{
        &clip, std::make_unique<SynthesizedClip>(), false});
    entry = synthesized_clip_cache_.end() - 1;
  }

  entry->in_use = true;
  SynthesizedClip& synthesized_clip = *entry->synthesized_clip;
  if (needs_layer) {
    synthesized_clip.UpdateLayer(needs_layer, clip, transform);
    synthesized_clip.Layer()->SetLayerTreeHost(root_layer_->layer_tree_host());
    if (layer_debug_info_enabled_ && !synthesized_clip.Layer()->debug_info())
      synthesized_clip.Layer()->SetDebugName("Synthesized Clip");
  }
  mask_isolation_id = synthesized_clip.GetMaskIsolationId();
  mask_effect_id = synthesized_clip.GetMaskEffectId();
  return synthesized_clip;
}

static void UpdateCompositorViewportProperties(
    const PaintArtifactCompositor::ViewportProperties& properties,
    PropertyTreeManager& property_tree_manager,
    cc::LayerTreeHost* layer_tree_host) {
  // The inner and outer viewports' existence is linked. That is, either they're
  // both null or they both exist.
  DCHECK_EQ(static_cast<bool>(properties.outer_scroll_translation),
            static_cast<bool>(properties.inner_scroll_translation));
  DCHECK(!properties.outer_clip ||
         static_cast<bool>(properties.inner_scroll_translation));

  cc::ViewportPropertyIds ids;
  if (properties.overscroll_elasticity_transform) {
    ids.overscroll_elasticity_transform =
        property_tree_manager.EnsureCompositorTransformNode(
            *properties.overscroll_elasticity_transform);
  }
  if (properties.overscroll_elasticity_effect) {
    ids.overscroll_elasticity_effect =
        properties.overscroll_elasticity_effect->GetCompositorElementId();
  }
  if (properties.page_scale) {
    ids.page_scale_transform =
        property_tree_manager.EnsureCompositorPageScaleTransformNode(
            *properties.page_scale);
  }
  if (properties.inner_scroll_translation) {
    ids.inner_scroll = property_tree_manager.EnsureCompositorInnerScrollNode(
        *properties.inner_scroll_translation);
    if (properties.outer_clip) {
      ids.outer_clip = property_tree_manager.EnsureCompositorClipNode(
          *properties.outer_clip);
    }
    if (properties.outer_scroll_translation) {
      ids.outer_scroll = property_tree_manager.EnsureCompositorOuterScrollNode(
          *properties.outer_scroll_translation);
    }
  }

  if (RuntimeEnabledFeatures::FixedElementsDontOverscrollEnabled()) {
    property_tree_manager.SetOverscrollTransformNodeId(
        ids.overscroll_elasticity_transform);
    property_tree_manager.SetFixedElementsDontOverscroll(true);
  }
  layer_tree_host->RegisterViewportPropertyIds(ids);
}

void PaintArtifactCompositor::Update(
    scoped_refptr<const PaintArtifact> artifact,
    const ViewportProperties& viewport_properties,
    const Vector<const TransformPaintPropertyNode*>& scroll_translation_nodes,
    Vector<std::unique_ptr<cc::DocumentTransitionRequest>>
        transition_requests) {
  // See: |UpdateRepaintedLayers| for repaint updates.
  DCHECK(needs_update_);
  DCHECK(scroll_translation_nodes.IsEmpty() ||
         RuntimeEnabledFeatures::ScrollUnificationEnabled());
  DCHECK(root_layer_);

  TRACE_EVENT0("blink", "PaintArtifactCompositor::Update");

  // The tree will be null after detaching and this update can be ignored.
  // See: WebViewImpl::detachPaintArtifactCompositor().
  cc::LayerTreeHost* host = root_layer_->layer_tree_host();
  if (!host)
    return;

  for (auto& request : transition_requests)
    host->AddDocumentTransitionRequest(std::move(request));

  host->property_trees()->scroll_tree_mutable().SetScrollCallbacks(
      scroll_callbacks_);
  root_layer_->set_property_tree_sequence_number(
      g_s_property_tree_sequence_number);

  wtf_size_t old_size = pending_layers_.size();
  OldPendingLayerMatcher old_pending_layer_matcher(std::move(pending_layers_));
  pending_layers_.ReserveCapacity(old_size);

  // Make compositing decisions, storing the result in |pending_layers_|.
  CollectPendingLayers(artifact);
  PendingLayer::DecompositeTransforms(pending_layers_);

  LayerListBuilder layer_list_builder;
  PropertyTreeManager property_tree_manager(*this, *host->property_trees(),
                                            *root_layer_, layer_list_builder,
                                            g_s_property_tree_sequence_number);

  UpdateCompositorViewportProperties(viewport_properties, property_tree_manager,
                                     host);

  // With ScrollUnification, we ensure a cc::ScrollNode for all
  // |scroll_translation_nodes|.
  if (RuntimeEnabledFeatures::ScrollUnificationEnabled())
    property_tree_manager.EnsureCompositorScrollNodes(scroll_translation_nodes);

  for (auto& entry : synthesized_clip_cache_)
    entry.in_use = false;

  host->property_trees()
      ->effect_tree_mutable()
      .ClearTransitionPseudoElementEffectNodes();
  cc::LayerSelection layer_selection;
  for (auto& pending_layer : pending_layers_) {
    pending_layer.UpdateCompositedLayer(
        old_pending_layer_matcher.Find(pending_layer), layer_selection,
        tracks_raster_invalidations_, root_layer_->layer_tree_host());

    cc::Layer& layer = pending_layer.CcLayer();
    const auto& property_state = pending_layer.GetPropertyTreeState();
    const auto& transform = property_state.Transform();
    const auto& clip = property_state.Clip();
    const auto& effect = property_state.Effect();
    int transform_id =
        property_tree_manager.EnsureCompositorTransformNode(transform);
    int clip_id = property_tree_manager.EnsureCompositorClipNode(clip);
    int effect_id = property_tree_manager.SwitchToEffectNodeWithSynthesizedClip(
        effect, clip, layer.draws_content());

    if (RuntimeEnabledFeatures::FixedElementsDontOverscrollEnabled() &&
        transform.RequiresCompositingForFixedToViewport())
      property_tree_manager.SetOverscrollClipNodeId(clip_id);

    // We need additional bookkeeping for backdrop-filter mask.
    if (effect.RequiresCompositingForBackdropFilterMask() &&
        effect.CcNodeId(g_s_property_tree_sequence_number) == effect_id) {
      static_cast<cc::PictureLayer&>(layer).SetIsBackdropFilterMask(true);
      layer.SetElementId(effect.GetCompositorElementId());
      auto& effect_tree = host->property_trees()->effect_tree_mutable();
      auto* cc_node = effect_tree.Node(effect_id);
      effect_tree.Node(cc_node->parent_id)->backdrop_mask_element_id =
          effect.GetCompositorElementId();
    }

    // The compositor scroll node is not directly stored in the property tree
    // state but can be created via the scroll offset translation node.
    const auto& scroll_translation =
        NearestScrollTranslationForLayer(pending_layer);
    // TODO(ScrollUnification): We may combine the following two calls to
    // property_tree_manager.SetCcScrollNodeIsComposited(scroll_translation);
    int scroll_id =
        property_tree_manager.EnsureCompositorScrollNode(scroll_translation);
    if (RuntimeEnabledFeatures::ScrollUnificationEnabled())
      property_tree_manager.SetCcScrollNodeIsComposited(scroll_id);

    layer_list_builder.Add(&layer);

    layer.set_property_tree_sequence_number(
        root_layer_->property_tree_sequence_number());
    layer.SetTransformTreeIndex(transform_id);
    layer.SetScrollTreeIndex(scroll_id);
    layer.SetClipTreeIndex(clip_id);
    layer.SetEffectTreeIndex(effect_id);
    bool backface_hidden = transform.IsBackfaceHidden();
    layer.SetShouldCheckBackfaceVisibility(backface_hidden);

    if (layer.subtree_property_changed())
      root_layer_->SetNeedsCommit();

    auto shared_element_id = layer.DocumentTransitionResourceId();
    if (shared_element_id.IsValid()) {
      host->property_trees()
          ->effect_tree_mutable()
          .AddTransitionPseudoElementEffectId(effect_id);
    }
  }

  root_layer_->layer_tree_host()->RegisterSelection(layer_selection);

  property_tree_manager.Finalize();

  auto* new_end = std::remove_if(
      synthesized_clip_cache_.begin(), synthesized_clip_cache_.end(),
      [](const auto& entry) { return !entry.in_use; });
  synthesized_clip_cache_.Shrink(
      static_cast<wtf_size_t>(new_end - synthesized_clip_cache_.begin()));

  // This should be done before
  // property_tree_manager.UpdateConditionalRenderSurfaceReasons() for which to
  // get property tree node ids from the layers.
  host->property_trees()->set_sequence_number(
      g_s_property_tree_sequence_number);

  auto layers = layer_list_builder.Finalize();
  property_tree_manager.UpdateConditionalRenderSurfaceReasons(layers);
  root_layer_->SetChildLayerList(std::move(layers));

  // Mark the property trees as having been rebuilt.
  host->property_trees()->set_needs_rebuild(false);
  host->property_trees()->ResetCachedData();
  previous_update_for_testing_ = PreviousUpdateType::kFull;
  needs_update_ = false;

  UpdateDebugInfo();

  g_s_property_tree_sequence_number++;

  DVLOG(2) << "PaintArtifactCompositor::Update() done\n"
           << "Composited layers:\n"
           << GetLayersAsJSON(VLOG_IS_ON(3) ? 0xffffffff : 0)
                  ->ToPrettyJSONString()
                  .Utf8();
}

void PaintArtifactCompositor::UpdateRepaintedLayers(
    scoped_refptr<const PaintArtifact> repainted_artifact) {
  // |Update| should be used for full updates.
  DCHECK(!needs_update_);

  const auto& repainted_chunks = repainted_artifact->PaintChunks();
#if DCHECK_IS_ON()
  // Any property tree state change should have caused a full update.
  for (const auto& chunk : repainted_chunks) {
    // If this fires, a property tree value has changed but we are missing a
    // call to |PaintArtifactCompositor::SetNeedsUpdate|.
    DCHECK(!chunk.properties.GetPropertyTreeState().Unalias().ChangedToRoot(
        PaintPropertyChangeType::kChangedOnlyNonRerasterValues));
  }
#endif

  cc::LayerSelection layer_selection;

  // The loop below iterates over the existing PendingLayers and issues updates.
  auto* repainted_chunk_iterator = repainted_chunks.begin();
  for (auto& pending_layer : pending_layers_) {
    // We need to both copy the repainted paint chunks and update the cc::Layer.
    // To do this, we need the previous PaintChunks (from the PendingLayer) and
    // the matching repainted PaintChunks (from |repainted_chunks|). Because
    // repaint-only updates cannot add, remove, or re-order PaintChunks,
    // |repainted_chunk_iterator| searches forward in |repainted_chunks| for
    // the matching paint chunk, ensuring this function is O(chunks).
    const PaintChunk& first = *pending_layer.Chunks().begin();
    while (repainted_chunk_iterator != repainted_chunks.end()) {
      if (repainted_chunk_iterator->Matches(first))
        break;
      ++repainted_chunk_iterator;
    }
    // If we do not find a matching PaintChunk, PaintChunks must have been
    // added, removed, or re-ordered, and we should be doing a full update
    // instead of a repaint update.
    CHECK(repainted_chunk_iterator != repainted_chunks.end());

    pending_layer.UpdateCompositedLayerForRepaint(repainted_artifact,
                                                  layer_selection);
  }

  root_layer_->layer_tree_host()->RegisterSelection(layer_selection);

  UpdateDebugInfo();

  previous_update_for_testing_ = PreviousUpdateType::kRepaint;
  needs_update_ = false;
}

bool PaintArtifactCompositor::CanDirectlyUpdateProperties() const {
  // Don't try to retrieve property trees if we need an update. The full
  // update will update all of the nodes, so a direct update doesn't need to
  // do anything.
  if (needs_update_)
    return false;

  return root_layer_ && root_layer_->layer_tree_host();
}

bool PaintArtifactCompositor::DirectlyUpdateCompositedOpacityValue(
    const EffectPaintPropertyNode& effect) {
  // We can only directly-update compositor values if all content associated
  // with the node is known to be composited.
  DCHECK(effect.HasDirectCompositingReasons());
  if (CanDirectlyUpdateProperties()) {
    return PropertyTreeManager::DirectlyUpdateCompositedOpacityValue(
        *root_layer_->layer_tree_host(), effect);
  }
  return false;
}

bool PaintArtifactCompositor::DirectlyUpdateScrollOffsetTransform(
    const TransformPaintPropertyNode& transform) {
  // We can only directly-update compositor values if all content associated
  // with the node is known to be composited.
  DCHECK(transform.HasDirectCompositingReasons());
  if (CanDirectlyUpdateProperties()) {
    return PropertyTreeManager::DirectlyUpdateScrollOffsetTransform(
        *root_layer_->layer_tree_host(), transform);
  }
  return false;
}

bool PaintArtifactCompositor::DirectlyUpdateTransform(
    const TransformPaintPropertyNode& transform) {
  // We can only directly-update compositor values if all content associated
  // with the node is known to be composited.
  DCHECK(transform.HasDirectCompositingReasons());
  // We only assume worst-case overlap testing due to animations (see:
  // |PendingLayer::VisualRectForOverlapTesting|) so we can only use the direct
  // transform update (which skips checking for compositing changes) when
  // animations are present.
  DCHECK(transform.HasActiveTransformAnimation());
  if (CanDirectlyUpdateProperties()) {
    return PropertyTreeManager::DirectlyUpdateTransform(
        *root_layer_->layer_tree_host(), transform);
  }
  return false;
}

bool PaintArtifactCompositor::DirectlyUpdatePageScaleTransform(
    const TransformPaintPropertyNode& transform) {
  // We can only directly-update compositor values if all content associated
  // with the node is known to be composited.
  DCHECK(transform.HasDirectCompositingReasons());
  if (CanDirectlyUpdateProperties()) {
    return PropertyTreeManager::DirectlyUpdatePageScaleTransform(
        *root_layer_->layer_tree_host(), transform);
  }
  return false;
}

bool PaintArtifactCompositor::DirectlySetScrollOffset(
    CompositorElementId element_id,
    const gfx::PointF& scroll_offset) {
  if (!root_layer_ || !root_layer_->layer_tree_host())
    return false;
  auto* property_trees = root_layer_->layer_tree_host()->property_trees();
  if (!property_trees->scroll_tree().FindNodeFromElementId(element_id))
    return false;
  PropertyTreeManager::DirectlySetScrollOffset(*root_layer_->layer_tree_host(),
                                               element_id, scroll_offset);
  return true;
}

void PaintArtifactCompositor::SetLayerDebugInfoEnabled(bool enabled) {
  if (enabled == layer_debug_info_enabled_)
    return;

  DCHECK(needs_update_);
  layer_debug_info_enabled_ = enabled;

  if (enabled) {
    root_layer_->SetDebugName("root");
  } else {
    root_layer_->ClearDebugInfo();
    for (auto& layer : root_layer_->children())
      layer->ClearDebugInfo();
  }
}

static void UpdateLayerDebugInfo(
    cc::Layer& layer,
    const PaintChunk::Id& id,
    const String& name,
    DOMNodeId owner_node_id,
    CompositingReasons compositing_reasons,
    RasterInvalidationTracking* raster_invalidation_tracking) {
  cc::LayerDebugInfo& debug_info = layer.EnsureDebugInfo();

  debug_info.name = name.Utf8();
  debug_info.compositing_reasons =
      CompositingReason::Descriptions(compositing_reasons);
  debug_info.compositing_reason_ids =
      CompositingReason::ShortNames(compositing_reasons);
  debug_info.owner_node_id = owner_node_id;

  if (RasterInvalidationTracking::IsTracingRasterInvalidations() &&
      raster_invalidation_tracking) {
    raster_invalidation_tracking->AddToLayerDebugInfo(debug_info);
    raster_invalidation_tracking->ClearInvalidations();
  }
}

static void UpdateLayerDebugInfo(
    cc::Layer& layer,
    const PaintChunk::Id& id,
    const PaintArtifact& paint_artifact,
    CompositingReasons compositing_reasons,
    RasterInvalidationTracking* raster_invalidation_tracking) {
  UpdateLayerDebugInfo(layer, id, paint_artifact.ClientDebugName(id.client_id),
                       paint_artifact.ClientOwnerNodeId(id.client_id),
                       compositing_reasons, raster_invalidation_tracking);
}

void PaintArtifactCompositor::UpdateDebugInfo() const {
  if (!layer_debug_info_enabled_)
    return;

  const PendingLayer* previous_pending_layer = nullptr;
  for (const auto& pending_layer : pending_layers_) {
    cc::Layer& layer = pending_layer.CcLayer();
    RasterInvalidationTracking* tracking = nullptr;
    if (auto* client = pending_layer.GetContentLayerClient())
      tracking = client->GetRasterInvalidator().GetTracking();
    UpdateLayerDebugInfo(
        layer, pending_layer.FirstPaintChunk().id,
        pending_layer.Chunks().GetPaintArtifact(),
        GetCompositingReasons(pending_layer, previous_pending_layer), tracking);
    previous_pending_layer = &pending_layer;
  }
}

CompositingReasons PaintArtifactCompositor::GetCompositingReasons(
    const PendingLayer& layer,
    const PendingLayer* previous_layer) const {
  DCHECK(layer_debug_info_enabled_);

  if (layer.ChunkRequiresOwnLayer()) {
    if (layer.GetCompositingType() == PendingLayer::kScrollHitTestLayer)
      return CompositingReason::kOverflowScrolling;
    switch (layer.FirstDisplayItem().GetType()) {
      case DisplayItem::kForeignLayerCanvas:
        return CompositingReason::kCanvas;
      case DisplayItem::kForeignLayerPlugin:
        return CompositingReason::kPlugin;
      case DisplayItem::kForeignLayerVideo:
        return CompositingReason::kVideo;
      case DisplayItem::kScrollbarHorizontal:
        return CompositingReason::kLayerForHorizontalScrollbar;
      case DisplayItem::kScrollbarVertical:
        return CompositingReason::kLayerForVerticalScrollbar;
      default:
        return CompositingReason::kLayerForOther;
    }
  }

  CompositingReasons reasons = CompositingReason::kNone;
  if (!previous_layer ||
      &layer.GetPropertyTreeState().Transform() !=
          &previous_layer->GetPropertyTreeState().Transform()) {
    reasons |= layer.GetPropertyTreeState()
                   .Transform()
                   .DirectCompositingReasonsForDebugging();
    if (!layer.GetPropertyTreeState()
             .Transform()
             .BackfaceVisibilitySameAsParent())
      reasons |= CompositingReason::kBackfaceVisibilityHidden;
  }

  if (!previous_layer || &layer.GetPropertyTreeState().Effect() !=
                             &previous_layer->GetPropertyTreeState().Effect()) {
    const auto& effect = layer.GetPropertyTreeState().Effect();
    if (effect.HasDirectCompositingReasons())
      reasons |= effect.DirectCompositingReasonsForDebugging();
    if (reasons == CompositingReason::kNone &&
        layer.GetCompositingType() == PendingLayer::kOther) {
      if (effect.Opacity() != 1.0f)
        reasons |= CompositingReason::kOpacityWithCompositedDescendants;
      if (!effect.Filter().IsEmpty())
        reasons |= CompositingReason::kFilterWithCompositedDescendants;
      if (effect.BlendMode() == SkBlendMode::kDstIn)
        reasons |= CompositingReason::kMaskWithCompositedDescendants;
      else if (effect.BlendMode() != SkBlendMode::kSrcOver)
        reasons |= CompositingReason::kBlendingWithCompositedDescendants;
    }
  }

  if (reasons == CompositingReason::kNone &&
      layer.GetCompositingType() == PendingLayer::kOverlap)
    reasons = CompositingReason::kOverlap;

  return reasons;
}

Vector<cc::Layer*> PaintArtifactCompositor::SynthesizedClipLayersForTesting()
    const {
  Vector<cc::Layer*> synthesized_clip_layers;
  for (const auto& entry : synthesized_clip_cache_)
    synthesized_clip_layers.push_back(entry.synthesized_clip->Layer());
  return synthesized_clip_layers;
}

void PaintArtifactCompositor::ClearPropertyTreeChangedState() {
  // For information about |sequence_number|, see:
  // PaintPropertyNode::changed_sequence_number_|;
  static int changed_sequence_number = 1;

  for (auto& layer : pending_layers_) {
    // The chunks ref-counted property tree state keeps the |layer|'s non-ref
    // property tree pointers alive and all chunk property tree states should
    // be descendants of the |layer|'s. Therefore, we can just CHECK that the
    // first chunk's references are keeping the |layer|'s property tree state
    // alive.
    CHECK(!layer.Chunks().IsEmpty());
    const auto& layer_state = layer.GetPropertyTreeState();
    const auto& first_chunk_state =
        layer.Chunks().begin()->properties.GetPropertyTreeState();
    CHECK(layer_state.Transform().IsAncestorOf(first_chunk_state.Transform()));
    CHECK(layer_state.Clip().IsAncestorOf(first_chunk_state.Clip()));
    CHECK(layer_state.Effect().IsAncestorOf(first_chunk_state.Effect()));

    for (auto& chunk : layer.Chunks()) {
      chunk.properties.GetPropertyTreeState().ClearChangedToRoot(
          changed_sequence_number);
    }
  }
  changed_sequence_number++;
}

size_t PaintArtifactCompositor::ApproximateUnsharedMemoryUsage() const {
  size_t result = sizeof(*this) + synthesized_clip_cache_.CapacityInBytes() +
                  pending_layers_.CapacityInBytes();

  for (auto& layer : pending_layers_) {
    if (auto* client = layer.GetContentLayerClient())
      result += client->ApproximateUnsharedMemoryUsage();
    size_t chunks_size = layer.Chunks().ApproximateUnsharedMemoryUsage();
    DCHECK_GE(chunks_size, sizeof(layer.Chunks()));
    result += chunks_size - sizeof(layer.Chunks());
  }

  return result;
}

void PaintArtifactCompositor::SetScrollbarNeedsDisplay(
    CompositorElementId element_id) {
  for (auto& pending_layer : pending_layers_) {
    if (pending_layer.GetCompositingType() == PendingLayer::kScrollbarLayer &&
        pending_layer.CcLayer().element_id() == element_id) {
      pending_layer.CcLayer().SetNeedsDisplay();
      return;
    }
  }
}

void LayerListBuilder::Add(scoped_refptr<cc::Layer> layer) {
  DCHECK(list_valid_);
  // Duplicated layers may happen when a foreign layer is fragmented.
  // TODO(wangxianzhu): Change this to DCHECK when we ensure all foreign layers
  // are monolithic (i.e. LayoutNGBlockFragmentation is fully launched).
  if (layer_ids_.insert(layer->id()).is_new_entry)
    list_.push_back(layer);
}

cc::LayerList LayerListBuilder::Finalize() {
  DCHECK(list_valid_);
  list_valid_ = false;
  return std::move(list_);
}

#if DCHECK_IS_ON()
void PaintArtifactCompositor::ShowDebugData() {
  LOG(INFO) << GetLayersAsJSON(kLayerTreeIncludesDebugInfo |
                               kLayerTreeIncludesDetailedInvalidations)
                   ->ToPrettyJSONString()
                   .Utf8();
}
#endif

ContentLayerClientImpl* PaintArtifactCompositor::ContentLayerClientForTesting(
    wtf_size_t i) const {
  for (auto& pending_layer : pending_layers_) {
    if (auto* client = pending_layer.GetContentLayerClient()) {
      if (i == 0)
        return client;
      --i;
    }
  }
  return nullptr;
}

}  // namespace blink
