// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintArtifactCompositor.h"

#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/display_item_list.h"
#include "cc/trees/layer_tree_host.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/compositing/ContentLayerClientImpl.h"
#include "platform/graphics/compositing/PaintChunksToCcLayer.h"
#include "platform/graphics/compositing/PropertyTreeManager.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/ForeignLayerDisplayItem.h"
#include "platform/graphics/paint/GeometryMapper.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/graphics/paint/PropertyTreeState.h"
#include "platform/graphics/paint/RasterInvalidationTracking.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

template <typename T>
static std::unique_ptr<JSONArray> PointAsJSONArray(const T& point) {
  std::unique_ptr<JSONArray> array = JSONArray::Create();
  array->PushDouble(point.X());
  array->PushDouble(point.Y());
  return array;
}

template <typename T>
static std::unique_ptr<JSONArray> SizeAsJSONArray(const T& size) {
  std::unique_ptr<JSONArray> array = JSONArray::Create();
  array->PushDouble(size.Width());
  array->PushDouble(size.Height());
  return array;
}

// cc property trees make use of a sequence number to identify when tree
// topology changes. For now we naively increment the sequence number each time
// we update the property trees. We should explore optimizing our management of
// the sequence number through the use of a dirty bit or similar. See
// http://crbug.com/692842#c4.
static int g_s_property_tree_sequence_number = 1;

PaintArtifactCompositor::PaintArtifactCompositor() {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;
  root_layer_ = cc::Layer::Create();
  web_layer_ = Platform::Current()->CompositorSupport()->CreateLayerFromCCLayer(
      root_layer_.get());
}

PaintArtifactCompositor::~PaintArtifactCompositor() {}

void PaintArtifactCompositor::ResetTrackedRasterInvalidations() {
  for (auto& client : content_layer_clients_)
    client->ResetTrackedRasterInvalidations();
}

std::unique_ptr<JSONObject> PaintArtifactCompositor::LayersAsJSON(
    LayerTreeFlags flags) const {
  std::unique_ptr<JSONArray> layers_json = JSONArray::Create();
  for (const auto& client : content_layer_clients_) {
    layers_json->PushObject(client->LayerAsJSON(flags));
  }
  std::unique_ptr<JSONObject> json = JSONObject::Create();
  json->SetArray("layers", std::move(layers_json));
  return json;
}

static scoped_refptr<cc::Layer> ForeignLayerForPaintChunk(
    const PaintArtifact& paint_artifact,
    const PaintChunk& paint_chunk,
    gfx::Vector2dF& layer_offset) {
  if (paint_chunk.size() != 1)
    return nullptr;

  const auto& display_item =
      paint_artifact.GetDisplayItemList()[paint_chunk.begin_index];
  if (!display_item.IsForeignLayer())
    return nullptr;

  const auto& foreign_layer_display_item =
      static_cast<const ForeignLayerDisplayItem&>(display_item);
  layer_offset = gfx::Vector2dF(foreign_layer_display_item.Location().X(),
                                foreign_layer_display_item.Location().Y());
  scoped_refptr<cc::Layer> layer = foreign_layer_display_item.GetLayer();
  layer->SetBounds(foreign_layer_display_item.Bounds());
  layer->SetIsDrawable(true);
  return layer;
}

std::unique_ptr<ContentLayerClientImpl>
PaintArtifactCompositor::ClientForPaintChunk(
    const PaintChunk& paint_chunk,
    const PaintArtifact& paint_artifact) {
  // TODO(chrishtr): for now, just using a linear walk. In the future we can
  // optimize this by using the same techniques used in PaintController for
  // display lists.
  for (auto& client : content_layer_clients_) {
    if (client && client->Matches(paint_chunk))
      return std::move(client);
  }

  return WTF::WrapUnique(new ContentLayerClientImpl(paint_chunk));
}

IntRect PaintArtifactCompositor::MapRasterInvalidationRectFromChunkToLayer(
    const FloatRect& r,
    const PaintChunk& paint_chunk,
    const PendingLayer& pending_layer,
    const gfx::Vector2dF& layer_offset) {
  FloatClipRect rect(
      r == FloatRect(LayoutRect::InfiniteIntRect()) ? paint_chunk.bounds : r);
  GeometryMapper::LocalToAncestorVisualRect(
      paint_chunk.properties.property_tree_state,
      pending_layer.property_tree_state, rect);
  if (rect.Rect().IsEmpty())
    return IntRect();
  // Now rect is in the space of the containing transform node of pending_layer,
  // so need to subtract off the layer offset.
  rect.Rect().Move(-layer_offset.x(), -layer_offset.y());
  rect.Rect().Inflate(paint_chunk.outset_for_raster_effects);
  return EnclosingIntRect(rect.Rect());
}

scoped_refptr<cc::Layer>
PaintArtifactCompositor::CompositedLayerForPendingLayer(
    const PaintArtifact& paint_artifact,
    const PendingLayer& pending_layer,
    gfx::Vector2dF& layer_offset,
    Vector<std::unique_ptr<ContentLayerClientImpl>>& new_content_layer_clients,
    bool store_debug_info) {
  DCHECK(pending_layer.paint_chunks.size());
  const PaintChunk& first_paint_chunk = *pending_layer.paint_chunks[0];
  DCHECK(first_paint_chunk.size());

  // If the paint chunk is a foreign layer, just return that layer.
  if (scoped_refptr<cc::Layer> foreign_layer = ForeignLayerForPaintChunk(
          paint_artifact, first_paint_chunk, layer_offset)) {
    DCHECK_EQ(pending_layer.paint_chunks.size(), 1u);
    return foreign_layer;
  }

  // The common case: create or reuse a PictureLayer for painted content.
  std::unique_ptr<ContentLayerClientImpl> content_layer_client =
      ClientForPaintChunk(first_paint_chunk, paint_artifact);

  gfx::Rect cc_combined_bounds(EnclosingIntRect(pending_layer.bounds));

  layer_offset = cc_combined_bounds.OffsetFromOrigin();
  scoped_refptr<cc::PictureLayer> cc_picture_layer =
      content_layer_client->CcPictureLayer();
  cc_picture_layer->SetBounds(cc_combined_bounds.size());
  cc_picture_layer->SetIsDrawable(true);
  cc_picture_layer->SetContentsOpaque(pending_layer.known_to_be_opaque);
  content_layer_client->ClearPaintChunkDebugData();

  for (const auto& paint_chunk : pending_layer.paint_chunks) {
    if (store_debug_info) {
      content_layer_client->AddPaintChunkDebugData(
          paint_artifact.GetDisplayItemList().SubsequenceAsJSON(
              paint_chunk->begin_index, paint_chunk->end_index,
              DisplayItemList::kSkipNonDrawings |
                  DisplayItemList::kShownOnlyDisplayItemTypes));
    }

    DCHECK(paint_chunk->raster_invalidation_tracking.IsEmpty() ||
           paint_chunk->raster_invalidation_rects.size() ==
               paint_chunk->raster_invalidation_tracking.size());

    for (size_t i = 0; i < paint_chunk->raster_invalidation_rects.size(); ++i) {
      auto rect = MapRasterInvalidationRectFromChunkToLayer(
          paint_chunk->raster_invalidation_rects[i], *paint_chunk,
          pending_layer, layer_offset);
      if (rect.IsEmpty())
        continue;
      content_layer_client->SetNeedsDisplayRect(rect);

      if (paint_chunk->raster_invalidation_tracking.IsEmpty())
        continue;
      auto& cc_tracking =
          content_layer_client->EnsureRasterInvalidationTracking();
      auto info = paint_chunk->raster_invalidation_tracking[i];
      info.rect = rect;
      cc_tracking.invalidations.push_back(info);
      cc_tracking.invalidation_region_since_last_paint.Unite(rect);
    }
  }

  Optional<RasterUnderInvalidationCheckingParams> params;
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    params.emplace(
        content_layer_client->EnsureRasterInvalidationTracking(),
        IntRect(0, 0, cc_combined_bounds.width(), cc_combined_bounds.height()),
        content_layer_client->DebugName());
  }

  auto display_list = PaintChunksToCcLayer::Convert(
      pending_layer.paint_chunks, pending_layer.property_tree_state,
      layer_offset, paint_artifact.GetDisplayItemList(),
      params ? &*params : nullptr);
  content_layer_client->SetDisplayList(std::move(display_list));
  content_layer_client->SetPaintableRegion(
      gfx::Rect(cc_combined_bounds.size()));

  new_content_layer_clients.push_back(std::move(content_layer_client));
  return cc_picture_layer;
}

PaintArtifactCompositor::PendingLayer::PendingLayer(
    const PaintChunk& first_paint_chunk,
    bool chunk_is_foreign)
    : bounds(first_paint_chunk.bounds),
      known_to_be_opaque(first_paint_chunk.known_to_be_opaque),
      backface_hidden(first_paint_chunk.properties.backface_hidden),
      property_tree_state(first_paint_chunk.properties.property_tree_state),
      is_foreign(chunk_is_foreign) {
  paint_chunks.push_back(&first_paint_chunk);
}

void PaintArtifactCompositor::PendingLayer::Merge(const PendingLayer& guest) {
  DCHECK(!is_foreign && !guest.is_foreign);
  DCHECK_EQ(backface_hidden, guest.backface_hidden);

  paint_chunks.AppendVector(guest.paint_chunks);
  FloatClipRect guest_bounds_in_home(guest.bounds);
  GeometryMapper::LocalToAncestorVisualRect(
      guest.property_tree_state, property_tree_state, guest_bounds_in_home);
  FloatRect old_bounds = bounds;
  bounds.Unite(guest_bounds_in_home.Rect());
  if (bounds != old_bounds)
    known_to_be_opaque = false;
  // TODO(crbug.com/701991): Upgrade GeometryMapper.
  // If we knew the new bounds is enclosed by the mapped opaque region of
  // the guest layer, we can deduce the merged layer being opaque too.
}

static bool CanUpcastTo(const PropertyTreeState& guest,
                        const PropertyTreeState& home);
bool PaintArtifactCompositor::PendingLayer::CanMerge(
    const PendingLayer& guest) const {
  if (is_foreign || guest.is_foreign)
    return false;
  if (backface_hidden != guest.backface_hidden)
    return false;
  if (property_tree_state.Effect() != guest.property_tree_state.Effect())
    return false;
  return CanUpcastTo(guest.property_tree_state, property_tree_state);
}

void PaintArtifactCompositor::PendingLayer::Upcast(
    const PropertyTreeState& new_state) {
  DCHECK(!is_foreign);
  FloatClipRect float_clip_rect(bounds);
  GeometryMapper::LocalToAncestorVisualRect(property_tree_state, new_state,
                                            float_clip_rect);
  bounds = float_clip_rect.Rect();

  property_tree_state = new_state;
  // TODO(crbug.com/701991): Upgrade GeometryMapper.
  // A local visual rect mapped to an ancestor space may become a polygon
  // (e.g. consider transformed clip), also effects may affect the opaque
  // region. To determine whether the layer is still opaque, we need to
  // query conservative opaque rect after mapping to an ancestor space,
  // which is not supported by GeometryMapper yet.
  known_to_be_opaque = false;
}

static bool IsNonCompositingAncestorOf(
    const TransformPaintPropertyNode* ancestor,
    const TransformPaintPropertyNode* node) {
  for (; node != ancestor; node = node->Parent()) {
    if (!node || node->HasDirectCompositingReasons())
      return false;
  }
  return true;
}

// Determines whether drawings based on the 'guest' state can be painted into
// a layer with the 'home' state. A number of criteria need to be met:
// 1. The guest effect must be a descendant of the home effect. However this
//    check is enforced by the layerization recursion. Here we assume the guest
//    has already been upcasted to the same effect.
// 2. The guest clip must be a descendant of the home clip.
// 3. The local space of each clip and effect node on the ancestor chain must
//    be within compositing boundary of the home transform space.
// 4. The guest transform space must be within compositing boundary of the home
//    transform space.
static bool CanUpcastTo(const PropertyTreeState& guest,
                        const PropertyTreeState& home) {
  DCHECK_EQ(home.Effect(), guest.Effect());

  for (const ClipPaintPropertyNode* current_clip = guest.Clip();
       current_clip != home.Clip(); current_clip = current_clip->Parent()) {
    if (!current_clip || current_clip->HasDirectCompositingReasons())
      return false;
    if (!IsNonCompositingAncestorOf(home.Transform(),
                                    current_clip->LocalTransformSpace()))
      return false;
  }

  return IsNonCompositingAncestorOf(home.Transform(), guest.Transform());
}

// Returns nullptr if 'ancestor' is not a strict ancestor of 'node'.
// Otherwise, return the child of 'ancestor' that is an ancestor of 'node' or
// 'node' itself.
static const EffectPaintPropertyNode* StrictChildOfAlongPath(
    const EffectPaintPropertyNode* ancestor,
    const EffectPaintPropertyNode* node) {
  for (; node; node = node->Parent()) {
    if (node->Parent() == ancestor)
      return node;
  }
  return nullptr;
}

bool PaintArtifactCompositor::MightOverlap(const PendingLayer& layer_a,
                                           const PendingLayer& layer_b) {
  PropertyTreeState root_property_tree_state(TransformPaintPropertyNode::Root(),
                                             ClipPaintPropertyNode::Root(),
                                             EffectPaintPropertyNode::Root());

  FloatClipRect bounds_a(layer_a.bounds);
  GeometryMapper::LocalToAncestorVisualRect(layer_a.property_tree_state,
                                            root_property_tree_state, bounds_a);
  FloatClipRect bounds_b(layer_b.bounds);
  GeometryMapper::LocalToAncestorVisualRect(layer_b.property_tree_state,
                                            root_property_tree_state, bounds_b);

  return bounds_a.Rect().Intersects(bounds_b.Rect());
}

bool PaintArtifactCompositor::CanDecompositeEffect(
    const EffectPaintPropertyNode* effect,
    const PendingLayer& layer) {
  // If the effect associated with the layer is deeper than than the effect
  // we are attempting to decomposite, than implies some previous decision
  // did not allow to decomposite intermediate effects.
  if (layer.property_tree_state.Effect() != effect)
    return false;
  if (layer.is_foreign)
    return false;
  // TODO(trchen): Exotic blending layer may be decomposited only if it could
  // be merged into the first layer of the current group.
  if (effect->BlendMode() != SkBlendMode::kSrcOver)
    return false;
  if (effect->HasDirectCompositingReasons())
    return false;
  if (!CanUpcastTo(layer.property_tree_state,
                   PropertyTreeState(effect->LocalTransformSpace(),
                                     effect->OutputClip(), effect)))
    return false;
  return true;
}

static bool EffectGroupContainsChunk(
    const EffectPaintPropertyNode& group_effect,
    const PaintChunk& chunk) {
  const EffectPaintPropertyNode* effect =
      chunk.properties.property_tree_state.Effect();
  return effect == &group_effect ||
         StrictChildOfAlongPath(&group_effect, effect);
}

static bool SkipGroupIfEffectivelyInvisible(
    const PaintArtifact& paint_artifact,
    const EffectPaintPropertyNode& current_group,
    Vector<PaintChunk>::const_iterator& chunk_it) {
  // The lower bound of visibility is considered to be 0.0004f < 1/2048. With
  // 10-bit color channels (only available on the newest Macs as of 2016;
  // otherwise it's 8-bit), we see that an alpha of 1/2048 or less leads to a
  // color output of less than 0.5 in all channels, hence not visible.
  static const float kMinimumVisibleOpacity = 0.0004f;
  if (current_group.Opacity() >= kMinimumVisibleOpacity ||
      current_group.HasDirectCompositingReasons()) {
    return false;
  }

  // Fast-forward to just past the end of the chunk sequence within this
  // effect group.
  DCHECK(EffectGroupContainsChunk(current_group, *chunk_it));
  while (++chunk_it != paint_artifact.PaintChunks().end()) {
    if (!EffectGroupContainsChunk(current_group, *chunk_it))
      break;
  }
  return true;
}

void PaintArtifactCompositor::LayerizeGroup(
    const PaintArtifact& paint_artifact,
    Vector<PendingLayer>& pending_layers,
    const EffectPaintPropertyNode& current_group,
    Vector<PaintChunk>::const_iterator& chunk_it) {
  // Skip paint chunks that are effectively invisible due to opacity and don't
  // have a direct compositing reason.
  if (SkipGroupIfEffectivelyInvisible(paint_artifact, current_group, chunk_it))
    return;

  size_t first_layer_in_current_group = pending_layers.size();
  // The worst case time complexity of the algorithm is O(pqd), where
  // p = the number of paint chunks.
  // q = average number of trials to find a squash layer or rejected
  //     for overlapping.
  // d = (sum of) the depth of property trees.
  // The analysis as follows:
  // Every paint chunk will be visited by the main loop below for exactly once,
  // except for chunks that enter or exit groups (case B & C below).
  // For normal chunk visit (case A), the only cost is determining squash,
  // which costs O(qd), where d came from "canUpcastTo" and geometry mapping.
  // Subtotal: O(pqd)
  // For group entering and exiting, it could cost O(d) for each group, for
  // searching the shallowest subgroup (strictChildOfAlongPath), thus O(d^2)
  // in total.
  // Also when exiting group, the group may be decomposited and squashed to a
  // previous layer. Again finding the host costs O(qd). Merging would cost O(p)
  // due to copying the chunk list. Subtotal: O((qd + p)d) = O(qd^2 + pd)
  // Assuming p > d, the total complexity would be O(pqd + qd^2 + pd) = O(pqd)
  while (chunk_it != paint_artifact.PaintChunks().end()) {
    // Look at the effect node of the next chunk. There are 3 possible cases:
    // A. The next chunk belongs to the current group but no subgroup.
    // B. The next chunk does not belong to the current group.
    // C. The next chunk belongs to some subgroup of the current group.
    const EffectPaintPropertyNode* chunk_effect =
        chunk_it->properties.property_tree_state.Effect();
    if (chunk_effect == &current_group) {
      // Case A: The next chunk belongs to the current group but no subgroup.
      bool is_foreign =
          paint_artifact.GetDisplayItemList()[chunk_it->begin_index]
              .IsForeignLayer();
      pending_layers.push_back(PendingLayer(*chunk_it++, is_foreign));
      if (is_foreign)
        continue;
    } else {
      const EffectPaintPropertyNode* subgroup =
          StrictChildOfAlongPath(&current_group, chunk_effect);
      // Case B: This means we need to close the current group without
      //         processing the next chunk.
      if (!subgroup)
        break;
      // Case C: The following chunks belong to a subgroup. Process them by
      //         a recursion call.
      size_t first_layer_in_subgroup = pending_layers.size();
      LayerizeGroup(paint_artifact, pending_layers, *subgroup, chunk_it);
      // Now the chunk iterator stepped over the subgroup we just saw.
      // If the subgroup generated 2 or more layers then the subgroup must be
      // composited to satisfy grouping requirement.
      // i.e. Grouping effects generally must be applied atomically,
      // for example,  Opacity(A+B) != Opacity(A) + Opacity(B), thus an effect
      // either applied 100% within a layer, or not at all applied within layer
      // (i.e. applied by compositor render surface instead).
      if (pending_layers.size() != first_layer_in_subgroup + 1)
        continue;
      // Now attempt to "decomposite" subgroup.
      PendingLayer& subgroup_layer = pending_layers[first_layer_in_subgroup];
      if (!CanDecompositeEffect(subgroup, subgroup_layer))
        continue;
      subgroup_layer.Upcast(PropertyTreeState(subgroup->LocalTransformSpace(),
                                              subgroup->OutputClip(),
                                              &current_group));
    }
    // At this point pendingLayers.back() is the either a layer from a
    // "decomposited" subgroup or a layer created from a chunk we just
    // processed. Now determine whether it could be merged into a previous
    // layer.
    const PendingLayer& new_layer = pending_layers.back();
    DCHECK(!new_layer.is_foreign);
    DCHECK_EQ(&current_group, new_layer.property_tree_state.Effect());
    // This iterates pendingLayers[firstLayerInCurrentGroup:-1] in reverse.
    for (size_t candidate_index = pending_layers.size() - 1;
         candidate_index-- > first_layer_in_current_group;) {
      PendingLayer& candidate_layer = pending_layers[candidate_index];
      if (candidate_layer.CanMerge(new_layer)) {
        candidate_layer.Merge(new_layer);
        pending_layers.pop_back();
        break;
      }
      if (MightOverlap(new_layer, candidate_layer))
        break;
    }
  }
}

void PaintArtifactCompositor::CollectPendingLayers(
    const PaintArtifact& paint_artifact,
    Vector<PendingLayer>& pending_layers) {
  Vector<PaintChunk>::const_iterator cursor =
      paint_artifact.PaintChunks().begin();
  LayerizeGroup(paint_artifact, pending_layers,
                *EffectPaintPropertyNode::Root(), cursor);
  DCHECK_EQ(paint_artifact.PaintChunks().end(), cursor);
}

void PaintArtifactCompositor::Update(
    const PaintArtifact& paint_artifact,
    bool store_debug_info,
    CompositorElementIdSet& composited_element_ids) {
#ifndef NDEBUG
  store_debug_info = true;
#endif

  DCHECK(root_layer_);

  // The tree will be null after detaching and this update can be ignored.
  // See: WebViewImpl::detachPaintArtifactCompositor().
  cc::LayerTreeHost* host = root_layer_->layer_tree_host();
  if (!host)
    return;

  if (extra_data_for_testing_enabled_)
    extra_data_for_testing_ = WTF::WrapUnique(new ExtraDataForTesting);

  // Unregister element ids for all layers. For now we rely on the
  // element id being set on the layer, but we'll both be removing
  // that for SPv2 soon. We may also shift to having multiple element
  // ids per layer. When we do either of these, we'll need to keep
  // around the element ids for unregistering in some other manner.
  for (auto child : root_layer_->children()) {
    host->UnregisterElement(child->element_id(), cc::ElementListType::ACTIVE,
                            child.get());
  }
  root_layer_->RemoveAllChildren();

  root_layer_->set_property_tree_sequence_number(
      g_s_property_tree_sequence_number);

  PropertyTreeManager property_tree_manager(*host->property_trees(),
                                            root_layer_.get(),
                                            g_s_property_tree_sequence_number);

  Vector<PendingLayer, 0> pending_layers;
  CollectPendingLayers(paint_artifact, pending_layers);

  Vector<std::unique_ptr<ContentLayerClientImpl>> new_content_layer_clients;
  new_content_layer_clients.ReserveCapacity(
      paint_artifact.PaintChunks().size());
  for (const PendingLayer& pending_layer : pending_layers) {
    gfx::Vector2dF layer_offset;
    scoped_refptr<cc::Layer> layer = CompositedLayerForPendingLayer(
        paint_artifact, pending_layer, layer_offset, new_content_layer_clients,
        store_debug_info);

    const auto* transform = pending_layer.property_tree_state.Transform();
    int transform_id =
        property_tree_manager.EnsureCompositorTransformNode(transform);
    int clip_id = property_tree_manager.EnsureCompositorClipNode(
        pending_layer.property_tree_state.Clip());
    int effect_id = property_tree_manager.SwitchToEffectNode(
        *pending_layer.property_tree_state.Effect());

    layer->set_offset_to_transform_parent(layer_offset);
    CompositorElementId element_id =
        pending_layer.property_tree_state.GetCompositorElementId(
            composited_element_ids);
    if (element_id) {
      // TODO(wkorman): Cease setting element id on layer once
      // animation subsystem no longer requires element id to layer
      // map. http://crbug.com/709137
      layer->SetElementId(element_id);
      composited_element_ids.insert(element_id);
    }

    root_layer_->AddChild(layer);
    // TODO(wkorman): Once we've removed all uses of
    // LayerTreeHost::{LayerByElementId,element_layers_map} we can
    // revise element register/unregister to cease passing layer and
    // only register/unregister element id with the mutator host.
    if (element_id) {
      host->RegisterElement(element_id, cc::ElementListType::ACTIVE,
                            layer.get());
    }

    layer->set_property_tree_sequence_number(g_s_property_tree_sequence_number);
    layer->SetTransformTreeIndex(transform_id);
    layer->SetClipTreeIndex(clip_id);
    layer->SetEffectTreeIndex(effect_id);
    property_tree_manager.UpdateLayerScrollMapping(layer.get(), transform);

    layer->SetShouldCheckBackfaceVisibility(pending_layer.backface_hidden);

    if (extra_data_for_testing_enabled_)
      extra_data_for_testing_->content_layers.push_back(layer);
  }
  content_layer_clients_.clear();
  content_layer_clients_.swap(new_content_layer_clients);

  // Mark the property trees as having been rebuilt.
  host->property_trees()->sequence_number = g_s_property_tree_sequence_number;
  host->property_trees()->needs_rebuild = false;
  host->property_trees()->ResetCachedData();

  g_s_property_tree_sequence_number++;
}

#ifndef NDEBUG
void PaintArtifactCompositor::ShowDebugData() {
  LOG(ERROR) << LayersAsJSON(kLayerTreeIncludesDebugInfo)
                    ->ToPrettyJSONString()
                    .Utf8()
                    .data();
}
#endif

}  // namespace blink
