// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PAINT_ARTIFACT_COMPOSITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PAINT_ARTIFACT_COMPOSITOR_H_

#include <memory>

#include "base/dcheck_is_on.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/property_tree.h"
#include "third_party/blink/renderer/platform/graphics/compositing/layers_as_json.h"
#include "third_party/blink/renderer/platform/graphics/compositing/pending_layer.h"
#include "third_party/blink/renderer/platform/graphics/compositing/property_tree_manager.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#if DCHECK_IS_ON()
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#endif

namespace cc {
class ScrollbarLayerBase;
class DocumentTransitionRequest;
}

namespace blink {

class ContentLayerClientImpl;
class JSONObject;
class SynthesizedClip;

using CompositorScrollCallbacks = cc::ScrollCallbacks;

class LayerListBuilder {
 public:
  void Add(scoped_refptr<cc::Layer>);
  cc::LayerList Finalize();

 private:
  // The list becomes invalid once |Finalize| is called.
  bool list_valid_ = true;
  cc::LayerList list_;
  HashSet<int> layer_ids_;
};

// This class maintains unique stable cc effect IDs (and optionally a persistent
// mask layer) for reuse across compositing cycles. The mask layer paints a
// rounded rect, which is an updatable parameter of the class. The caller is
// responsible for inserting the mask layer into layer list and associating with
// property nodes. The mask layer may be omitted if the caller determines it is
// not necessary (e.g. because there is no content to mask).
//
// The typical application of the mask layer is to create an isolating effect
// node to paint the clipped contents, and at the end draw the mask layer with
// a kDstIn blend effect. This is why two stable cc effect IDs are provided.
// Even if the mask layer is not present, it's important for the isolation
// effect node to be stable, to minimize render surface damage.
class SynthesizedClip : private cc::ContentLayerClient {
 public:
  SynthesizedClip() : layer_(nullptr) {
    mask_isolation_id_ =
        CompositorElementIdFromUniqueObjectId(NewUniqueObjectId());
    mask_effect_id_ =
        CompositorElementIdFromUniqueObjectId(NewUniqueObjectId());
  }
  ~SynthesizedClip() override {
    if (layer_)
      layer_->ClearClient();
  }

  void UpdateLayer(bool needs_layer,
                   const ClipPaintPropertyNode&,
                   const TransformPaintPropertyNode&);

  cc::PictureLayer* Layer() { return layer_.get(); }
  CompositorElementId GetMaskIsolationId() const { return mask_isolation_id_; }
  CompositorElementId GetMaskEffectId() const { return mask_effect_id_; }

 private:
  // ContentLayerClient implementation.
  gfx::Rect PaintableRegion() const final {
    return gfx::Rect(layer_->bounds());
  }
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList() final;
  bool FillsBoundsCompletely() const final { return false; }

 private:
  scoped_refptr<cc::PictureLayer> layer_;
  GeometryMapper::Translation2DOrMatrix translation_2d_or_matrix_;
  bool rrect_is_local_ = false;
  SkRRect rrect_;
  scoped_refptr<const RefCountedPath> path_;
  CompositorElementId mask_isolation_id_;
  CompositorElementId mask_effect_id_;
};

// Responsible for managing compositing in terms of a PaintArtifact.
//
// Owns a subtree of the compositor layer tree, and updates it in response to
// changes in the paint artifact.
class PLATFORM_EXPORT PaintArtifactCompositor final
    : private PropertyTreeManagerClient {
  USING_FAST_MALLOC(PaintArtifactCompositor);

 public:
  PaintArtifactCompositor(
      base::WeakPtr<CompositorScrollCallbacks> scroll_callbacks);
  PaintArtifactCompositor(const PaintArtifactCompositor&) = delete;
  PaintArtifactCompositor& operator=(const PaintArtifactCompositor&) = delete;
  ~PaintArtifactCompositor() override;

  struct ViewportProperties {
    const TransformPaintPropertyNode* overscroll_elasticity_transform = nullptr;
    const EffectPaintPropertyNode* overscroll_elasticity_effect = nullptr;
    const TransformPaintPropertyNode* page_scale = nullptr;
    const TransformPaintPropertyNode* inner_scroll_translation = nullptr;
    const ClipPaintPropertyNode* outer_clip = nullptr;
    const TransformPaintPropertyNode* outer_scroll_translation = nullptr;
  };

  // Updates the cc layer list and property trees to match those provided in
  // |paint_chunks|.
  //
  // |scroll_translation_nodes| is the complete set of scroll nodes, including
  // noncomposited nodes, and is used for Scroll Unification to generate scroll
  // nodes for noncomposited scrollers to complete the compositor's scroll
  // property tree.
  void Update(
      scoped_refptr<const PaintArtifact> artifact,
      const ViewportProperties& viewport_properties,
      const Vector<const TransformPaintPropertyNode*>& scroll_translation_nodes,
      Vector<std::unique_ptr<cc::DocumentTransitionRequest>> requests);

  // Fast-path update where the painting of existing composited layers changed,
  // but property trees and compositing decisions remain the same. See:
  // |Update| for full updates.
  //
  // When this update can be used is tightly coupled with |Update|, see
  // |SetNeedsFullUpdateAfterPaintIfNeeded| for details. For example, this
  // update can be used when the color of a display item is updated. This update
  // can not be used if the size of a display item increases because that could
  // require different cc::layers due to changes in overlap. This update also
  // can not be used if property trees change (with the exception of fast-path
  // direct updates that do not change compositing such as
  // |DirectlyUpdateCompositedOpacityValue|) because property tree values in
  // effect and clip nodes create cc::layers (e.g., clip mask layers).
  //
  // This copies over the newly-painted PaintChunks to existing
  // |pending_layers_|, issues raster invalidations, and updates the existing
  // cc::Layer properties such as background color.
  void UpdateRepaintedLayers(scoped_refptr<const PaintArtifact>);

  bool DirectlyUpdateCompositedOpacityValue(const EffectPaintPropertyNode&);
  bool DirectlyUpdateScrollOffsetTransform(const TransformPaintPropertyNode&);
  bool DirectlyUpdateTransform(const TransformPaintPropertyNode&);
  bool DirectlyUpdatePageScaleTransform(const TransformPaintPropertyNode&);

  // Directly sets cc::ScrollTree::current_scroll_offset. This doesn't affect
  // cc::TransformNode::scroll_offset (which will be synched with blink
  // transform node in DirectlyUpdateScrollOffsetTransform() or Update()).
  bool DirectlySetScrollOffset(CompositorElementId,
                               const gfx::PointF& scroll_offset);

  // The root layer of the tree managed by this object.
  cc::Layer* RootLayer() const { return root_layer_.get(); }

  void SetTracksRasterInvalidations(bool);

  // Called when the local frame view that owns this compositor is
  // going to be removed from its frame.
  void WillBeRemovedFromFrame();

  std::unique_ptr<JSONArray> GetPendingLayersAsJSON() const;

  std::unique_ptr<JSONObject> GetLayersAsJSON(LayerTreeFlags) const;

#if DCHECK_IS_ON()
  void ShowDebugData();
#endif

  const Vector<std::unique_ptr<ContentLayerClientImpl>>&
  ContentLayerClientsForTesting() const {
    return content_layer_clients_;
  }

  // Mark this as needing a full compositing update. Repaint-only updates that
  // do not affect compositing can use a fast-path in |UpdateRepaintedLayers|
  // (see comment above that function for more information), and should not call
  // SetNeedsUpdate.
  void SetNeedsUpdate() { needs_update_ = true; }
  bool NeedsUpdate() const { return needs_update_; }
  void ClearNeedsUpdateForTesting() { needs_update_ = false; }

  void SetPrefersLCDText(bool);

  // There is no mechanism for doing a paint lifecycle phase without running
  // PaintArtifactCompositor::Update so this is exposed so tests can check the
  // last update type.
  enum class PreviousUpdateType { kNone, kRepaint, kFull };
  PreviousUpdateType PreviousUpdateForTesting() const {
    return previous_update_for_testing_;
  }
  void ClearPreviousUpdateForTesting() {
    previous_update_for_testing_ = PreviousUpdateType::kNone;
  }

  void SetNeedsFullUpdateAfterPaintIfNeeded(const PaintChunkSubset& previous,
                                            const PaintChunkSubset& repainted);

  // Returns true if a property tree node associated with |element_id| exists
  // on any of the PropertyTrees constructed by |Update|.
  bool HasComposited(CompositorElementId element_id) const;

  void SetLayerDebugInfoEnabled(bool);

  Vector<cc::Layer*> SynthesizedClipLayersForTesting() const;

  void ClearPropertyTreeChangedState();

  size_t ApproximateUnsharedMemoryUsage() const;

  void SetScrollbarNeedsDisplay(CompositorElementId element_id);

 private:
  static void UpdateLayerProperties(cc::Layer&, const PendingLayer&);
  static void UpdateLayerSelection(cc::Layer&,
                                   const PendingLayer&,
                                   cc::LayerSelection& layer_selection);

  // Updates |content_layer_client| associated with a |pending_layer| following
  // a paint. This includes updating the drawings and raster invalidation.
  void UpdateRepaintedContentLayerClient(
      const PendingLayer& pending_layer,
      bool pending_layer_chunks_unchanged,
      ContentLayerClientImpl& content_layer_client);

  // Collects the PaintChunks into groups which will end up in the same
  // cc layer. This is the entry point of the layerization algorithm.
  void CollectPendingLayers(scoped_refptr<const PaintArtifact>);

  // This is the internal recursion of collectPendingLayers. This function
  // loops over the list of paint chunks, scoped by an isolated group
  // (i.e. effect node). Inside of the loop, chunks are tested for overlap
  // and merge compatibility. Subgroups are handled by recursion, and will
  // be tested for "decompositing" upon return.
  // Merge compatibility means consecutive chunks may be layerized into the
  // same backing (i.e. merged) if their property states don't cross
  // direct-compositing boundary.
  // Non-consecutive chunks that are nevertheless compatible may still be
  // merged, if reordering of the chunks won't affect the ultimate result.
  // This is determined by overlap testing such that chunks can be safely
  // reordered if their effective bounds in screen space can't overlap.
  // The recursion only tests merge & overlap for chunks scoped by the same
  // group. This is where "decompositing" came in. Upon returning from a
  // recursion, the layerization of the subgroup may be tested for merge &
  // overlap with other chunks in the parent group, if grouping requirement
  // can be satisfied (and the effect node has no direct reason).
  void LayerizeGroup(const PaintChunkSubset&,
                     const EffectPaintPropertyNode&,
                     PaintChunkIterator& chunk_cursor);
  bool DecompositeEffect(const EffectPaintPropertyNode& parent_effect,
                         wtf_size_t first_layer_in_parent_group_index,
                         const EffectPaintPropertyNode& effect,
                         wtf_size_t layer_index);

  // Builds a leaf layer that represents a single paint chunk.
  scoped_refptr<cc::Layer> CompositedLayerForPendingLayer(
      const PendingLayer&,
      Vector<std::unique_ptr<ContentLayerClientImpl>>&
          new_content_layer_clients,
      Vector<scoped_refptr<cc::Layer>>& new_scroll_hit_test_layers,
      Vector<scoped_refptr<cc::ScrollbarLayerBase>>& new_scrollbar_layers);

  const TransformPaintPropertyNode& NearestScrollTranslationForLayer(
      const PendingLayer&);

  // Returns the cc::Layer if the pending layer contains a foreign layer or a
  // wrapper of a GraphicsLayer. If it's the latter and the graphics layer has
  // been repainted, also updates the layer properties.
  scoped_refptr<cc::Layer> WrappedCcLayerForPendingLayer(const PendingLayer&);

  // Finds an existing or creates a new scroll hit test layer for the pending
  // layer, returning nullptr if the layer is not a scroll hit test layer.
  scoped_refptr<cc::Layer> ScrollHitTestLayerForPendingLayer(
      const PendingLayer&);

  // Finds an existing or creates a new scrollbar layer for the pending layer,
  // returning nullptr if the layer is not a scrollbar layer.
  scoped_refptr<cc::ScrollbarLayerBase> ScrollbarLayerForPendingLayer(
      const PendingLayer&);

  // Finds a client among the current vector of clients that matches the paint
  // chunk's id, or otherwise allocates a new one.
  std::unique_ptr<ContentLayerClientImpl> ClientForPaintChunk(
      const PaintChunk&);

  // if |needs_layer| is false, no cc::Layer is created, |mask_effect_id| is
  // not set, and the Layer() method on the returned SynthesizedClip returns
  // nullptr.
  // However, |mask_isolation_id| is always set.
  SynthesizedClip& CreateOrReuseSynthesizedClipLayer(
      const ClipPaintPropertyNode&,
      const TransformPaintPropertyNode&,
      bool needs_layer,
      CompositorElementId& mask_isolation_id,
      CompositorElementId& mask_effect_id) final;

  static void UpdateRenderSurfaceForEffects(
      cc::EffectTree&,
      const cc::LayerList&,
      const Vector<const EffectPaintPropertyNode*>&);

  bool CanDirectlyUpdateProperties() const;

  CompositingReasons GetCompositingReasons(
      const PendingLayer& layer,
      const PendingLayer* previous_layer) const;

  void UpdateDebugInfo() const;

  // For notifying blink of composited scrolling.
  base::WeakPtr<CompositorScrollCallbacks> scroll_callbacks_;

  bool tracks_raster_invalidations_;
  bool needs_update_ = true;
  PreviousUpdateType previous_update_for_testing_ = PreviousUpdateType::kNone;
  bool layer_debug_info_enabled_ = false;
  bool prefers_lcd_text_ = false;

  scoped_refptr<cc::Layer> root_layer_;
  Vector<std::unique_ptr<ContentLayerClientImpl>> content_layer_clients_;
  struct SynthesizedClipEntry {
    const ClipPaintPropertyNode* key;
    std::unique_ptr<SynthesizedClip> synthesized_clip;
    bool in_use;
  };
  Vector<SynthesizedClipEntry> synthesized_clip_cache_;

  Vector<scoped_refptr<cc::Layer>> scroll_hit_test_layers_;
  Vector<scoped_refptr<cc::ScrollbarLayerBase>> scrollbar_layers_;

  Vector<PendingLayer, 0> pending_layers_;

  friend class StubChromeClientForCAP;
  friend class PaintArtifactCompositorTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PAINT_ARTIFACT_COMPOSITOR_H_
