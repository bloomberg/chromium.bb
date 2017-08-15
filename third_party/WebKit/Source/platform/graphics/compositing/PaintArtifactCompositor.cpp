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

// cc property trees make use of a sequence number to identify when tree
// topology changes. For now we naively increment the sequence number each time
// we update the property trees. We should explore optimizing our management of
// the sequence number through the use of a dirty bit or similar. See
// http://crbug.com/692842#c4.
static int g_s_property_tree_sequence_number = 1;

PaintArtifactCompositor::PaintArtifactCompositor()
    : tracks_raster_invalidations_(false) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;
  root_layer_ = cc::Layer::Create();
  web_layer_ = Platform::Current()->CompositorSupport()->CreateLayerFromCCLayer(
      root_layer_.get());
}

PaintArtifactCompositor::~PaintArtifactCompositor() {
  for (auto child : root_layer_->children())
    DCHECK(!child->element_id());
}

void PaintArtifactCompositor::EnableExtraDataForTesting() {
  extra_data_for_testing_enabled_ = true;
  extra_data_for_testing_ = WTF::WrapUnique(new ExtraDataForTesting);
}

void PaintArtifactCompositor::SetTracksRasterInvalidations(bool should_track) {
  for (auto& client : content_layer_clients_)
    client->SetTracksRasterInvalidations(should_track);
}

void PaintArtifactCompositor::WillBeRemovedFromFrame() {
  RemoveChildLayers();
}

void PaintArtifactCompositor::RemoveChildLayers() {
  // Unregister element ids for all layers. For now we rely on the
  // element id being set on the layer, but we'll be removing that for
  // SPv2 soon. We may also shift to having multiple element ids per
  // layer. When we do either of these, we'll need to keep around the
  // element ids for unregistering in some other manner.
  cc::LayerTreeHost* host = root_layer_->layer_tree_host();
  if (!host)
    return;
  for (auto child : root_layer_->children()) {
    host->UnregisterElement(child->element_id(), cc::ElementListType::ACTIVE,
                            child.get());
  }
  root_layer_->RemoveAllChildren();
  if (extra_data_for_testing_enabled_)
    extra_data_for_testing_->content_layers.clear();
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
PaintArtifactCompositor::ClientForPaintChunk(const PaintChunk& paint_chunk) {
  // TODO(chrishtr): for now, just using a linear walk. In the future we can
  // optimize this by using the same techniques used in PaintController for
  // display lists.
  for (auto& client : content_layer_clients_) {
    if (client && client->Matches(paint_chunk))
      return std::move(client);
  }

  auto client = WTF::MakeUnique<ContentLayerClientImpl>();
  client->SetTracksRasterInvalidations(tracks_raster_invalidations_);
  return client;
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
      ClientForPaintChunk(first_paint_chunk);

  gfx::Rect cc_combined_bounds(EnclosingIntRect(pending_layer.bounds));
  layer_offset = cc_combined_bounds.OffsetFromOrigin();

  auto cc_layer = content_layer_client->UpdateCcPictureLayer(
      paint_artifact, cc_combined_bounds, pending_layer.paint_chunks,
      pending_layer.property_tree_state, store_debug_info);
  new_content_layer_clients.push_back(std::move(content_layer_client));
  return cc_layer;
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

// This class maintains a persistent mask layer and unique stable cc effect IDs
// for reuse across compositing cycles. The mask layer paints a rounded rect,
// which is an updatable parameter of the class. The caller is responsible for
// inserting the layer into layer list and associating with property nodes.
//
// The typical application of the mask layer is to create an isolating effect
// node to paint the clipped contents, and at the end draw the mask layer with
// a kDstIn blend effect. This is why two stable cc effect IDs are provided for
// the convenience of the caller, although they are not directly related to the
// class functionality.
class SynthesizedClip : private cc::ContentLayerClient {
 public:
  SynthesizedClip() : layer_(cc::PictureLayer::Create(this)) {
    static SyntheticEffectId serial;
    mask_isolation_id_ = CompositorElementIdFromSyntheticEffectId(++serial);
    mask_effect_id_ = CompositorElementIdFromSyntheticEffectId(++serial);
    layer_->SetIsDrawable(true);
  }

  void Update(const FloatRoundedRect& rrect) {
    IntRect layer_bounds = EnclosingIntRect(rrect.Rect());
    layer_->set_offset_to_transform_parent(
        gfx::Vector2dF(layer_bounds.X(), layer_bounds.Y()));
    layer_->SetBounds(gfx::Size(layer_bounds.Width(), layer_bounds.Height()));

    SkRRect new_rrect(rrect);
    new_rrect.offset(-layer_bounds.X(), -layer_bounds.Y());
    if (rrect_ == new_rrect)
      return;
    rrect_ = new_rrect;
    layer_->SetNeedsDisplay();
  }

  cc::Layer* GetLayer() const { return layer_.get(); }
  CompositorElementId GetMaskIsolationId() const { return mask_isolation_id_; }
  CompositorElementId GetMaskEffectId() const { return mask_effect_id_; }

 private:
  // ContentLayerClient implementation.
  gfx::Rect PaintableRegion() final { return gfx::Rect(layer_->bounds()); }
  bool FillsBoundsCompletely() const final { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const final { return 0; }

  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting) final {
    auto cc_list = base::MakeRefCounted<cc::DisplayItemList>(
        cc::DisplayItemList::kTopLevelDisplayItemList);
    cc_list->StartPaint();
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    cc_list->push<cc::DrawRRectOp>(rrect_, flags);
    cc_list->EndPaintOfUnpaired(gfx::Rect(layer_->bounds()));
    cc_list->Finalize();
    return cc_list;
  }

 private:
  scoped_refptr<cc::PictureLayer> layer_;
  SkRRect rrect_ = SkRRect::MakeEmpty();
  CompositorElementId mask_isolation_id_;
  CompositorElementId mask_effect_id_;
};

cc::Layer* PaintArtifactCompositor::CreateOrReuseSynthesizedClipLayer(
    const ClipPaintPropertyNode* node,
    CompositorElementId& mask_isolation_id,
    CompositorElementId& mask_effect_id) {
  auto entry = std::find_if(
      synthesized_clip_cache_.begin(), synthesized_clip_cache_.end(),
      [node](const auto& entry) { return entry.key == node && !entry.in_use; });
  if (entry == synthesized_clip_cache_.end()) {
    entry = synthesized_clip_cache_.insert(
        entry,
        SynthesizedClipEntry{node, std::make_unique<SynthesizedClip>(), false});
  }

  entry->in_use = true;
  SynthesizedClip& synthesized_clip = *entry->synthesized_clip;
  synthesized_clip.Update(node->ClipRect());
  mask_isolation_id = synthesized_clip.GetMaskIsolationId();
  mask_effect_id = synthesized_clip.GetMaskEffectId();
  return synthesized_clip.GetLayer();
}

void PaintArtifactCompositor::Update(
    const PaintArtifact& paint_artifact,
    CompositorElementIdSet& composited_element_ids) {
  DCHECK(root_layer_);

  // The tree will be null after detaching and this update can be ignored.
  // See: WebViewImpl::detachPaintArtifactCompositor().
  cc::LayerTreeHost* host = root_layer_->layer_tree_host();
  if (!host)
    return;

  if (extra_data_for_testing_enabled_)
    extra_data_for_testing_.reset(new ExtraDataForTesting);

  RemoveChildLayers();
  root_layer_->set_property_tree_sequence_number(
      g_s_property_tree_sequence_number);

  PropertyTreeManager property_tree_manager(*this, *host->property_trees(),
                                            root_layer_.get(),
                                            g_s_property_tree_sequence_number);
  Vector<PendingLayer, 0> pending_layers;
  CollectPendingLayers(paint_artifact, pending_layers);

  Vector<std::unique_ptr<ContentLayerClientImpl>> new_content_layer_clients;
  new_content_layer_clients.ReserveCapacity(pending_layers.size());

  for (auto& entry : synthesized_clip_cache_)
    entry.in_use = false;

  bool store_debug_info = false;
#ifndef NDEBUG
  store_debug_info = true;
#endif

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
    int effect_id = property_tree_manager.SwitchToEffectNodeWithSynthesizedClip(
        *pending_layer.property_tree_state.Effect(),
        *pending_layer.property_tree_state.Clip());

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

    layer->SetContentsOpaque(pending_layer.known_to_be_opaque);
    layer->SetShouldCheckBackfaceVisibility(pending_layer.backface_hidden);

    if (extra_data_for_testing_enabled_)
      extra_data_for_testing_->content_layers.push_back(layer);
  }
  property_tree_manager.Finalize();
  content_layer_clients_.swap(new_content_layer_clients);

  synthesized_clip_cache_.erase(
      std::remove_if(synthesized_clip_cache_.begin(),
                     synthesized_clip_cache_.end(),
                     [](const auto& entry) { return !entry.in_use; }),
      synthesized_clip_cache_.end());
  if (extra_data_for_testing_enabled_) {
    for (const auto& entry : synthesized_clip_cache_) {
      extra_data_for_testing_->synthesized_clip_layers.push_back(
          entry.synthesized_clip->GetLayer());
    }
  }

  // Mark the property trees as having been rebuilt.
  host->property_trees()->sequence_number = g_s_property_tree_sequence_number;
  host->property_trees()->needs_rebuild = false;
  host->property_trees()->ResetCachedData();

  g_s_property_tree_sequence_number++;

  for (const auto& chunk : paint_artifact.PaintChunks()) {
    chunk.properties.property_tree_state.ClearChangedToRoot();
    // TODO(wangxianzhu): This will be unnecessary if we don't call
    // PaintArtifactCompositor::Update() when paint artifact is unchanged.
    chunk.client_is_just_created = false;
    chunk.raster_invalidation_rects.clear();
    chunk.raster_invalidation_tracking.clear();
  }
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
