// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintArtifactCompositor.h"

#include <algorithm>
#include <memory>
#include <utility>
#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/compositing_display_item.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/drawing_display_item.h"
#include "cc/paint/filter_display_item.h"
#include "cc/paint/float_clip_display_item.h"
#include "cc/paint/transform_display_item.h"
#include "cc/trees/layer_tree_host.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/compositing/PaintChunksToCcLayer.h"
#include "platform/graphics/compositing/PropertyTreeManager.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
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
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/skia_util.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"

namespace blink {

template class RasterInvalidationTrackingMap<const cc::Layer>;
static RasterInvalidationTrackingMap<const cc::Layer>&
ccLayersRasterInvalidationTrackingMap() {
  DEFINE_STATIC_LOCAL(RasterInvalidationTrackingMap<const cc::Layer>, map, ());
  return map;
}

template <typename T>
static std::unique_ptr<JSONArray> sizeAsJSONArray(const T& size) {
  std::unique_ptr<JSONArray> array = JSONArray::create();
  array->pushDouble(size.width());
  array->pushDouble(size.height());
  return array;
}

// cc property trees make use of a sequence number to identify when tree
// topology changes. For now we naively increment the sequence number each time
// we update the property trees. We should explore optimizing our management of
// the sequence number through the use of a dirty bit or similar. See
// http://crbug.com/692842#c4.
static int sPropertyTreeSequenceNumber = 1;

class PaintArtifactCompositor::ContentLayerClientImpl
    : public cc::ContentLayerClient {
  WTF_MAKE_NONCOPYABLE(ContentLayerClientImpl);
  USING_FAST_MALLOC(ContentLayerClientImpl);

 public:
  ContentLayerClientImpl(DisplayItem::Id paintChunkId)
      : m_id(paintChunkId),
        m_debugName(paintChunkId.client.debugName()),
        m_ccPictureLayer(cc::PictureLayer::Create(this)) {}

  void SetDisplayList(scoped_refptr<cc::DisplayItemList> ccDisplayItemList) {
    m_ccDisplayItemList = std::move(ccDisplayItemList);
  }
  void SetPaintableRegion(gfx::Rect region) { m_paintableRegion = region; }

  void addPaintChunkDebugData(std::unique_ptr<JSONArray> json) {
    m_paintChunkDebugData.push_back(std::move(json));
  }
  void clearPaintChunkDebugData() { m_paintChunkDebugData.clear(); }

  // cc::ContentLayerClient
  gfx::Rect PaintableRegion() override { return m_paintableRegion; }
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting) override {
    return m_ccDisplayItemList;
  }
  bool FillsBoundsCompletely() const override { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const override {
    // TODO(jbroman): Actually calculate memory usage.
    return 0;
  }

  void resetTrackedRasterInvalidations() {
    RasterInvalidationTracking* tracking =
        ccLayersRasterInvalidationTrackingMap().find(m_ccPictureLayer.get());
    if (!tracking)
      return;

    if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled())
      tracking->trackedRasterInvalidations.clear();
    else
      ccLayersRasterInvalidationTrackingMap().remove(m_ccPictureLayer.get());
  }

  bool hasTrackedRasterInvalidations() const {
    RasterInvalidationTracking* tracking =
        ccLayersRasterInvalidationTrackingMap().find(m_ccPictureLayer.get());
    if (tracking)
      return !tracking->trackedRasterInvalidations.isEmpty();
    return false;
  }

  void setNeedsDisplayRect(const gfx::Rect& rect,
                           RasterInvalidationInfo* rasterInvalidationInfo) {
    m_ccPictureLayer->SetNeedsDisplayRect(rect);

    if (!rasterInvalidationInfo || rect.IsEmpty())
      return;

    RasterInvalidationTracking& tracking =
        ccLayersRasterInvalidationTrackingMap().add(m_ccPictureLayer.get());

    tracking.trackedRasterInvalidations.push_back(*rasterInvalidationInfo);

    if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled()) {
      // TODO(crbug.com/496260): Some antialiasing effects overflow the paint
      // invalidation rect.
      IntRect r = rasterInvalidationInfo->rect;
      r.inflate(1);
      tracking.rasterInvalidationRegionSinceLastPaint.unite(r);
    }
  }

  std::unique_ptr<JSONObject> layerAsJSON(LayerTreeFlags flags) {
    std::unique_ptr<JSONObject> json = JSONObject::create();
    json->setString("name", m_debugName);
    IntSize bounds(m_ccPictureLayer->bounds().width(),
                   m_ccPictureLayer->bounds().height());
    if (!bounds.isEmpty())
      json->setArray("bounds", sizeAsJSONArray(bounds));
    json->setBoolean("contentsOpaque", m_ccPictureLayer->contents_opaque());
    json->setBoolean("drawsContent", m_ccPictureLayer->DrawsContent());

    if (flags & LayerTreeIncludesDebugInfo) {
      std::unique_ptr<JSONArray> paintChunkContentsArray = JSONArray::create();
      for (const auto& debugData : m_paintChunkDebugData) {
        paintChunkContentsArray->pushValue(debugData->clone());
      }
      json->setArray("paintChunkContents", std::move(paintChunkContentsArray));
    }

    ccLayersRasterInvalidationTrackingMap().asJSON(m_ccPictureLayer.get(),
                                                   json.get());
    return json;
  }

  scoped_refptr<cc::PictureLayer> ccPictureLayer() { return m_ccPictureLayer; }

  bool matches(const PaintChunk& paintChunk) {
    return paintChunk.id && m_id == *paintChunk.id;
  }

 private:
  PaintChunk::Id m_id;
  String m_debugName;
  scoped_refptr<cc::PictureLayer> m_ccPictureLayer;
  scoped_refptr<cc::DisplayItemList> m_ccDisplayItemList;
  gfx::Rect m_paintableRegion;
  Vector<std::unique_ptr<JSONArray>> m_paintChunkDebugData;
};

PaintArtifactCompositor::PaintArtifactCompositor() {
  if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;
  m_rootLayer = cc::Layer::Create();
  m_webLayer = Platform::current()->compositorSupport()->createLayerFromCCLayer(
      m_rootLayer.get());
  m_isTrackingRasterInvalidations = false;
}

PaintArtifactCompositor::~PaintArtifactCompositor() {}

void PaintArtifactCompositor::setTracksRasterInvalidations(
    bool tracksPaintInvalidations) {
  resetTrackedRasterInvalidations();
  m_isTrackingRasterInvalidations = tracksPaintInvalidations;
}

void PaintArtifactCompositor::resetTrackedRasterInvalidations() {
  for (auto& client : m_contentLayerClients)
    client->resetTrackedRasterInvalidations();
}

bool PaintArtifactCompositor::hasTrackedRasterInvalidations() const {
  for (auto& client : m_contentLayerClients) {
    if (client->hasTrackedRasterInvalidations())
      return true;
  }
  return false;
}

std::unique_ptr<JSONObject> PaintArtifactCompositor::layersAsJSON(
    LayerTreeFlags flags) const {
  std::unique_ptr<JSONArray> layersJSON = JSONArray::create();
  for (const auto& client : m_contentLayerClients) {
    layersJSON->pushObject(client->layerAsJSON(flags));
  }
  std::unique_ptr<JSONObject> json = JSONObject::create();
  json->setArray("layers", std::move(layersJSON));
  return json;
}

static scoped_refptr<cc::Layer> foreignLayerForPaintChunk(
    const PaintArtifact& paintArtifact,
    const PaintChunk& paintChunk,
    gfx::Vector2dF& layerOffset) {
  if (paintChunk.size() != 1)
    return nullptr;

  const auto& displayItem =
      paintArtifact.getDisplayItemList()[paintChunk.beginIndex];
  if (!displayItem.isForeignLayer())
    return nullptr;

  const auto& foreignLayerDisplayItem =
      static_cast<const ForeignLayerDisplayItem&>(displayItem);
  layerOffset = gfx::Vector2dF(foreignLayerDisplayItem.location().x(),
                               foreignLayerDisplayItem.location().y());
  scoped_refptr<cc::Layer> layer = foreignLayerDisplayItem.layer();
  layer->SetBounds(foreignLayerDisplayItem.bounds());
  layer->SetIsDrawable(true);
  return layer;
}

std::unique_ptr<PaintArtifactCompositor::ContentLayerClientImpl>
PaintArtifactCompositor::clientForPaintChunk(
    const PaintChunk& paintChunk,
    const PaintArtifact& paintArtifact) {
  // TODO(chrishtr): for now, just using a linear walk. In the future we can
  // optimize this by using the same techniques used in PaintController for
  // display lists.
  for (auto& client : m_contentLayerClients) {
    if (client && client->matches(paintChunk))
      return std::move(client);
  }

  return WTF::wrapUnique(new ContentLayerClientImpl(
      paintChunk.id
          ? *paintChunk.id
          : paintArtifact.getDisplayItemList()[paintChunk.beginIndex].getId()));
}

scoped_refptr<cc::Layer>
PaintArtifactCompositor::compositedLayerForPendingLayer(
    const PaintArtifact& paintArtifact,
    const PendingLayer& pendingLayer,
    gfx::Vector2dF& layerOffset,
    Vector<std::unique_ptr<ContentLayerClientImpl>>& newContentLayerClients,
    RasterInvalidationTrackingMap<const PaintChunk>* trackingMap,
    bool storeDebugInfo,
    GeometryMapper& geometryMapper) {
  DCHECK(pendingLayer.paintChunks.size());
  const PaintChunk& firstPaintChunk = *pendingLayer.paintChunks[0];
  DCHECK(firstPaintChunk.size());

  // If the paint chunk is a foreign layer, just return that layer.
  if (scoped_refptr<cc::Layer> foreignLayer = foreignLayerForPaintChunk(
          paintArtifact, firstPaintChunk, layerOffset)) {
    DCHECK_EQ(pendingLayer.paintChunks.size(), 1u);
    return foreignLayer;
  }

  // The common case: create or reuse a PictureLayer for painted content.
  std::unique_ptr<ContentLayerClientImpl> contentLayerClient =
      clientForPaintChunk(firstPaintChunk, paintArtifact);

  gfx::Rect ccCombinedBounds(enclosingIntRect(pendingLayer.bounds));

  layerOffset = ccCombinedBounds.OffsetFromOrigin();
  scoped_refptr<cc::DisplayItemList> displayList =
      PaintChunksToCcLayer::convert(
          pendingLayer.paintChunks, pendingLayer.propertyTreeState, layerOffset,
          paintArtifact.getDisplayItemList(), geometryMapper);
  contentLayerClient->SetDisplayList(std::move(displayList));
  contentLayerClient->SetPaintableRegion(gfx::Rect(ccCombinedBounds.size()));

  scoped_refptr<cc::PictureLayer> ccPictureLayer =
      contentLayerClient->ccPictureLayer();
  ccPictureLayer->SetBounds(ccCombinedBounds.size());
  ccPictureLayer->SetIsDrawable(true);
  ccPictureLayer->SetContentsOpaque(pendingLayer.knownToBeOpaque);
  contentLayerClient->clearPaintChunkDebugData();

  for (const auto& paintChunk : pendingLayer.paintChunks) {
    RasterInvalidationTracking* rasterTracking =
        trackingMap ? trackingMap->find(paintChunk) : nullptr;
    DCHECK(!rasterTracking ||
           rasterTracking->trackedRasterInvalidations.size() ==
               paintChunk->rasterInvalidationRects.size());

    if (storeDebugInfo) {
      contentLayerClient->addPaintChunkDebugData(
          paintArtifact.getDisplayItemList().subsequenceAsJSON(
              paintChunk->beginIndex, paintChunk->endIndex,
              DisplayItemList::SkipNonDrawings |
                  DisplayItemList::ShownOnlyDisplayItemTypes));
    }

    for (unsigned index = 0; index < paintChunk->rasterInvalidationRects.size();
         ++index) {
      IntRect rect(
          enclosingIntRect(paintChunk->rasterInvalidationRects[index]));
      gfx::Rect ccInvalidationRect(rect.x(), rect.y(),
                                   std::max(0, rect.width()),
                                   std::max(0, rect.height()));
      if (ccInvalidationRect.IsEmpty())
        continue;
      // Raster paintChunk.rasterInvalidationRects is in the space of the
      // containing transform node, so need to subtract off the layer offset.
      ccInvalidationRect.Offset(-ccCombinedBounds.OffsetFromOrigin());
      contentLayerClient->setNeedsDisplayRect(
          ccInvalidationRect,
          rasterTracking ? &rasterTracking->trackedRasterInvalidations[index]
                         : nullptr);
    }
  }

  newContentLayerClients.push_back(std::move(contentLayerClient));
  return ccPictureLayer;
}

PaintArtifactCompositor::PendingLayer::PendingLayer(
    const PaintChunk& firstPaintChunk,
    bool chunkIsForeign)
    : bounds(firstPaintChunk.bounds),
      knownToBeOpaque(firstPaintChunk.knownToBeOpaque),
      backfaceHidden(firstPaintChunk.properties.backfaceHidden),
      propertyTreeState(firstPaintChunk.properties.propertyTreeState),
      isForeign(chunkIsForeign) {
  paintChunks.push_back(&firstPaintChunk);
}

void PaintArtifactCompositor::PendingLayer::merge(
    const PendingLayer& guest,
    GeometryMapper& geometryMapper) {
  DCHECK(!isForeign && !guest.isForeign);
  DCHECK_EQ(backfaceHidden, guest.backfaceHidden);

  paintChunks.appendVector(guest.paintChunks);
  FloatRect guestBoundsInHome = guest.bounds;
  geometryMapper.localToAncestorVisualRect(
      guest.propertyTreeState, propertyTreeState, guestBoundsInHome);
  FloatRect oldBounds = bounds;
  bounds.unite(guestBoundsInHome);
  if (bounds != oldBounds)
    knownToBeOpaque = false;
  // TODO(crbug.com/701991): Upgrade GeometryMapper.
  // If we knew the new bounds is enclosed by the mapped opaque region of
  // the guest layer, we can deduce the merged layer being opaque too.
}

static bool canUpcastTo(const PropertyTreeState& guest,
                        const PropertyTreeState& home);
bool PaintArtifactCompositor::PendingLayer::canMerge(
    const PendingLayer& guest) const {
  if (isForeign || guest.isForeign)
    return false;
  if (backfaceHidden != guest.backfaceHidden)
    return false;
  if (propertyTreeState.effect() != guest.propertyTreeState.effect())
    return false;
  return canUpcastTo(guest.propertyTreeState, propertyTreeState);
}

void PaintArtifactCompositor::PendingLayer::upcast(
    const PropertyTreeState& newState,
    GeometryMapper& geometryMapper) {
  DCHECK(!isForeign);
  geometryMapper.localToAncestorVisualRect(propertyTreeState, newState, bounds);

  propertyTreeState = newState;
  // TODO(crbug.com/701991): Upgrade GeometryMapper.
  // A local visual rect mapped to an ancestor space may become a polygon
  // (e.g. consider transformed clip), also effects may affect the opaque
  // region. To determine whether the layer is still opaque, we need to
  // query conservative opaque rect after mapping to an ancestor space,
  // which is not supported by GeometryMapper yet.
  knownToBeOpaque = false;
}

static bool isNonCompositingAncestorOf(
    const TransformPaintPropertyNode* ancestor,
    const TransformPaintPropertyNode* node) {
  for (; node != ancestor; node = node->parent()) {
    if (!node || node->hasDirectCompositingReasons())
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
static bool canUpcastTo(const PropertyTreeState& guest,
                        const PropertyTreeState& home) {
  DCHECK_EQ(home.effect(), guest.effect());

  for (const ClipPaintPropertyNode* currentClip = guest.clip();
       currentClip != home.clip(); currentClip = currentClip->parent()) {
    if (!currentClip || currentClip->hasDirectCompositingReasons())
      return false;
    if (!isNonCompositingAncestorOf(home.transform(),
                                    currentClip->localTransformSpace()))
      return false;
  }

  return isNonCompositingAncestorOf(home.transform(), guest.transform());
}

// Returns nullptr if 'ancestor' is not a strict ancestor of 'node'.
// Otherwise, return the child of 'ancestor' that is an ancestor of 'node' or
// 'node' itself.
static const EffectPaintPropertyNode* strictChildOfAlongPath(
    const EffectPaintPropertyNode* ancestor,
    const EffectPaintPropertyNode* node) {
  for (; node; node = node->parent()) {
    if (node->parent() == ancestor)
      return node;
  }
  return nullptr;
}

bool PaintArtifactCompositor::mightOverlap(const PendingLayer& layerA,
                                           const PendingLayer& layerB,
                                           GeometryMapper& geometryMapper) {
  PropertyTreeState rootPropertyTreeState(TransformPaintPropertyNode::root(),
                                          ClipPaintPropertyNode::root(),
                                          EffectPaintPropertyNode::root());

  FloatRect boundsA = layerA.bounds;
  geometryMapper.localToAncestorVisualRect(layerA.propertyTreeState,
                                           rootPropertyTreeState, boundsA);
  FloatRect boundsB = layerB.bounds;
  geometryMapper.localToAncestorVisualRect(layerB.propertyTreeState,
                                           rootPropertyTreeState, boundsB);

  return boundsA.intersects(boundsB);
}

bool PaintArtifactCompositor::canDecompositeEffect(
    const EffectPaintPropertyNode* effect,
    const PendingLayer& layer) {
  // If the effect associated with the layer is deeper than than the effect
  // we are attempting to decomposite, than implies some previous decision
  // did not allow to decomposite intermediate effects.
  if (layer.propertyTreeState.effect() != effect)
    return false;
  if (layer.isForeign)
    return false;
  // TODO(trchen): Exotic blending layer may be decomposited only if it could
  // be merged into the first layer of the current group.
  if (effect->blendMode() != SkBlendMode::kSrcOver)
    return false;
  if (effect->hasDirectCompositingReasons())
    return false;
  if (!canUpcastTo(layer.propertyTreeState,
                   PropertyTreeState(effect->localTransformSpace(),
                                     effect->outputClip(), effect)))
    return false;
  return true;
}

void PaintArtifactCompositor::layerizeGroup(
    const PaintArtifact& paintArtifact,
    Vector<PendingLayer>& pendingLayers,
    GeometryMapper& geometryMapper,
    const EffectPaintPropertyNode& currentGroup,
    Vector<PaintChunk>::const_iterator& chunkIt) {
  size_t firstLayerInCurrentGroup = pendingLayers.size();
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
  while (chunkIt != paintArtifact.paintChunks().end()) {
    // Look at the effect node of the next chunk. There are 3 possible cases:
    // A. The next chunk belongs to the current group but no subgroup.
    // B. The next chunk does not belong to the current group.
    // C. The next chunk belongs to some subgroup of the current group.
    const EffectPaintPropertyNode* chunkEffect =
        chunkIt->properties.propertyTreeState.effect();
    if (chunkEffect == &currentGroup) {
      // Case A: The next chunk belongs to the current group but no subgroup.
      bool isForeign = paintArtifact.getDisplayItemList()[chunkIt->beginIndex]
                           .isForeignLayer();
      pendingLayers.push_back(PendingLayer(*chunkIt++, isForeign));
      if (isForeign)
        continue;
    } else {
      const EffectPaintPropertyNode* subgroup =
          strictChildOfAlongPath(&currentGroup, chunkEffect);
      // Case B: This means we need to close the current group without
      //         processing the next chunk.
      if (!subgroup)
        break;
      // Case C: The following chunks belong to a subgroup. Process them by
      //         a recursion call.
      size_t firstLayerInSubgroup = pendingLayers.size();
      layerizeGroup(paintArtifact, pendingLayers, geometryMapper, *subgroup,
                    chunkIt);
      // Now the chunk iterator stepped over the subgroup we just saw.
      // If the subgroup generated 2 or more layers then the subgroup must be
      // composited to satisfy grouping requirement.
      // i.e. Grouping effects generally must be applied atomically,
      // for example,  Opacity(A+B) != Opacity(A) + Opacity(B), thus an effect
      // either applied 100% within a layer, or not at all applied within layer
      // (i.e. applied by compositor render surface instead).
      if (pendingLayers.size() != firstLayerInSubgroup + 1)
        continue;
      // Now attempt to "decomposite" subgroup.
      PendingLayer& subgroupLayer = pendingLayers[firstLayerInSubgroup];
      if (!canDecompositeEffect(subgroup, subgroupLayer))
        continue;
      subgroupLayer.upcast(
          PropertyTreeState(subgroup->localTransformSpace(),
                            subgroup->outputClip(), &currentGroup),
          geometryMapper);
    }
    // At this point pendingLayers.back() is the either a layer from a
    // "decomposited" subgroup or a layer created from a chunk we just
    // processed. Now determine whether it could be merged into a previous
    // layer.
    const PendingLayer& newLayer = pendingLayers.back();
    DCHECK(!newLayer.isForeign);
    DCHECK_EQ(&currentGroup, newLayer.propertyTreeState.effect());
    // This iterates pendingLayers[firstLayerInCurrentGroup:-1] in reverse.
    for (size_t candidateIndex = pendingLayers.size() - 1;
         candidateIndex-- > firstLayerInCurrentGroup;) {
      PendingLayer& candidateLayer = pendingLayers[candidateIndex];
      if (candidateLayer.canMerge(newLayer)) {
        candidateLayer.merge(newLayer, geometryMapper);
        pendingLayers.pop_back();
        break;
      }
      if (mightOverlap(newLayer, candidateLayer, geometryMapper))
        break;
    }
  }
}

void PaintArtifactCompositor::collectPendingLayers(
    const PaintArtifact& paintArtifact,
    Vector<PendingLayer>& pendingLayers,
    GeometryMapper& geometryMapper) {
  Vector<PaintChunk>::const_iterator cursor =
      paintArtifact.paintChunks().begin();
  layerizeGroup(paintArtifact, pendingLayers, geometryMapper,
                *EffectPaintPropertyNode::root(), cursor);
  DCHECK_EQ(paintArtifact.paintChunks().end(), cursor);
}

void PaintArtifactCompositor::update(
    const PaintArtifact& paintArtifact,
    RasterInvalidationTrackingMap<const PaintChunk>* rasterChunkInvalidations,
    bool storeDebugInfo,
    GeometryMapper& geometryMapper) {
#ifndef NDEBUG
  storeDebugInfo = true;
#endif

  DCHECK(m_rootLayer);

  cc::LayerTreeHost* layerTreeHost = m_rootLayer->layer_tree_host();

  // The tree will be null after detaching and this update can be ignored.
  // See: WebViewImpl::detachPaintArtifactCompositor().
  if (!layerTreeHost)
    return;

  if (m_extraDataForTestingEnabled)
    m_extraDataForTesting = WTF::wrapUnique(new ExtraDataForTesting);

  m_rootLayer->RemoveAllChildren();

  m_rootLayer->set_property_tree_sequence_number(sPropertyTreeSequenceNumber);

  PropertyTreeManager propertyTreeManager(*layerTreeHost->property_trees(),
                                          m_rootLayer.get(),
                                          sPropertyTreeSequenceNumber);

  Vector<PendingLayer, 0> pendingLayers;
  collectPendingLayers(paintArtifact, pendingLayers, geometryMapper);

  Vector<std::unique_ptr<ContentLayerClientImpl>> newContentLayerClients;
  newContentLayerClients.reserveCapacity(paintArtifact.paintChunks().size());
  for (const PendingLayer& pendingLayer : pendingLayers) {
    gfx::Vector2dF layerOffset;
    scoped_refptr<cc::Layer> layer = compositedLayerForPendingLayer(
        paintArtifact, pendingLayer, layerOffset, newContentLayerClients,
        rasterChunkInvalidations, storeDebugInfo, geometryMapper);

    const auto* transform = pendingLayer.propertyTreeState.transform();
    int transformId =
        propertyTreeManager.ensureCompositorTransformNode(transform);
    int clipId = propertyTreeManager.ensureCompositorClipNode(
        pendingLayer.propertyTreeState.clip());
    int effectId = propertyTreeManager.switchToEffectNode(
        *pendingLayer.propertyTreeState.effect());

    layer->set_offset_to_transform_parent(layerOffset);
    layer->SetElementId(pendingLayer.propertyTreeState.compositorElementId());

    m_rootLayer->AddChild(layer);
    layer->set_property_tree_sequence_number(sPropertyTreeSequenceNumber);
    layer->SetTransformTreeIndex(transformId);
    layer->SetClipTreeIndex(clipId);
    layer->SetEffectTreeIndex(effectId);
    propertyTreeManager.updateLayerScrollMapping(layer.get(), transform);

    layer->SetShouldCheckBackfaceVisibility(pendingLayer.backfaceHidden);

    if (m_extraDataForTestingEnabled)
      m_extraDataForTesting->contentLayers.push_back(layer);
  }
  m_contentLayerClients.clear();
  m_contentLayerClients.swap(newContentLayerClients);

  // Mark the property trees as having been rebuilt.
  layerTreeHost->property_trees()->sequence_number =
      sPropertyTreeSequenceNumber;
  layerTreeHost->property_trees()->needs_rebuild = false;
  layerTreeHost->property_trees()->ResetCachedData();

  sPropertyTreeSequenceNumber++;
}

#ifndef NDEBUG
void PaintArtifactCompositor::showDebugData() {
  LOG(ERROR) << layersAsJSON(LayerTreeIncludesDebugInfo)
                    ->toPrettyJSONString()
                    .utf8()
                    .data();
}
#endif

}  // namespace blink
