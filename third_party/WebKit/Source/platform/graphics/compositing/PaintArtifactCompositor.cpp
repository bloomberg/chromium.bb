// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintArtifactCompositor.h"

#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/playback/compositing_display_item.h"
#include "cc/playback/display_item_list.h"
#include "cc/playback/display_item_list_settings.h"
#include "cc/playback/drawing_display_item.h"
#include "cc/playback/filter_display_item.h"
#include "cc/playback/float_clip_display_item.h"
#include "cc/playback/transform_display_item.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "platform/RuntimeEnabledFeatures.h"
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
#include <algorithm>
#include <memory>
#include <utility>

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
    m_paintChunkDebugData.append(std::move(json));
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

    tracking.trackedRasterInvalidations.append(*rasterInvalidationInfo);

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

namespace {

static gfx::Rect largeRect(-200000, -200000, 400000, 400000);

static void appendDisplayItemToCcDisplayItemList(const DisplayItem& displayItem,
                                                 cc::DisplayItemList* list) {
  if (DisplayItem::isDrawingType(displayItem.getType())) {
    const SkPicture* picture =
        static_cast<const DrawingDisplayItem&>(displayItem).picture();
    if (!picture)
      return;
    // In theory we would pass the bounds of the picture, previously done as:
    // gfx::Rect bounds = gfx::SkIRectToRect(picture->cullRect().roundOut());
    // or use the visual rect directly. However, clip content layers attempt
    // to raster in a different space than that of the visual rects. We'll be
    // reworking visual rects further for SPv2, so for now we just pass a
    // visual rect large enough to make sure items raster.
    list->CreateAndAppendDrawingItem<cc::DrawingDisplayItem>(
        largeRect, sk_ref_sp(picture));
  }
}

scoped_refptr<cc::Layer> foreignLayerForPaintChunk(
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

constexpr int kInvalidNodeId = -1;
// cc's property trees use 0 for the root node (always non-null).
constexpr int kRealRootNodeId = 0;
// cc allocates special nodes for root effects such as the device scale.
constexpr int kSecondaryRootNodeId = 1;
constexpr int kPropertyTreeSequenceNumber = 1;

enum EndDisplayItemType { EndTransform, EndClip, EndEffect };

// Applies the clips between |localState| and |ancestorState| into a single
// combined cc::FloatClipDisplayItem on |ccList|.
static void applyClipsBetweenStates(const PropertyTreeState& localState,
                                    const PropertyTreeState& ancestorState,
                                    cc::DisplayItemList& ccList,
                                    Vector<EndDisplayItemType>& endDisplayItems,
                                    GeometryMapper& geometryMapper) {
  DCHECK(localState.transform() == ancestorState.transform());
#ifdef DCHECK_IS_ON
  const TransformPaintPropertyNode* transformNode =
      localState.clip()->localTransformSpace();
  if (transformNode != ancestorState.transform()) {
    bool success = false;
    const TransformationMatrix& localToAncestorMatrix =
        geometryMapper.localToAncestorMatrix(transformNode, ancestorState,
                                             success);
    DCHECK(success);
    // Clips are only in descendant spaces that are transformed by one
    // or more scrolls.
    DCHECK(localToAncestorMatrix.isIdentityOrTranslation());
  }
#endif

  FloatRect combinedClip;
  bool success = false;
  combinedClip = geometryMapper.localToAncestorClipRect(localState,
                                                        ancestorState, success);
  DCHECK(success);

  ccList.CreateAndAppendPairedBeginItem<cc::FloatClipDisplayItem>(
      gfx::RectF(combinedClip));
  endDisplayItems.push_back(EndClip);
}

static void recordPairedBeginDisplayItems(
    const Vector<PropertyTreeState>& pairedStates,
    const PropertyTreeState& pendingLayerState,
    cc::DisplayItemList& ccList,
    Vector<EndDisplayItemType>& endDisplayItems,
    GeometryMapper& geometryMapper) {
  PropertyTreeState mappedClipDestinationSpace = pendingLayerState;
  PropertyTreeState clipSpace = pendingLayerState;
  bool hasClip = false;

  for (Vector<PropertyTreeState>::const_reverse_iterator pairedState =
           pairedStates.rbegin();
       pairedState != pairedStates.rend(); ++pairedState) {
    switch (pairedState->innermostNode()) {
      case PropertyTreeState::Transform: {
        if (hasClip) {
          applyClipsBetweenStates(clipSpace, mappedClipDestinationSpace, ccList,
                                  endDisplayItems, geometryMapper);
          hasClip = false;
        }
        mappedClipDestinationSpace = *pairedState;
        clipSpace = *pairedState;

        TransformationMatrix matrix = pairedState->transform()->matrix();
        matrix.applyTransformOrigin(pairedState->transform()->origin());

        gfx::Transform transform(gfx::Transform::kSkipInitialization);
        transform.matrix() = TransformationMatrix::toSkMatrix44(matrix);

        ccList.CreateAndAppendPairedBeginItem<cc::TransformDisplayItem>(
            transform);
        endDisplayItems.push_back(EndTransform);
        break;
      }
      case PropertyTreeState::Clip: {
        // Clips are handled in |applyClips| when ending the iterator, or
        // transitioning between transform spaces. Here we store off the
        // PropertyTreeState of the first found clip, under the transform of
        // pairedState->transform(). All subsequent clips before applying the
        // transform will be applied in applyClips.
        clipSpace = *pairedState;
        hasClip = true;
#ifdef DCHECK_IS_ON
        if (pairedState->clip()->localTransformSpace() !=
            pairedState->transform()) {
          const TransformationMatrix& localTransformMatrix =
              pairedState->effect()->localTransformSpace()->matrix();
          // Clips are only in descendant spaces that are transformed by scroll.
          DCHECK(localTransformMatrix.isIdentityOrTranslation());
        }
#endif
        break;
      }
      case PropertyTreeState::Effect: {
        // TODO(chrishtr): skip effect and/or compositing display items if
        // not necessary.

        FloatRect clipRect =
            pairedState->effect()->outputClip()->clipRect().rect();
        // TODO(chrishtr): specify origin of the filter.
        FloatPoint filterOrigin;
        if (pairedState->effect()->localTransformSpace() !=
            pairedState->transform()) {
          bool success = false;
          const TransformPaintPropertyNode* transformNode =
              pairedState->effect()->localTransformSpace();
          const TransformationMatrix& localToAncestorMatrix =
              geometryMapper.localToAncestorMatrix(transformNode, *pairedState,
                                                   success);
          DCHECK(success);
          // Effects are only in descendant spaces that are transformed by one
          // or more scrolls.
          DCHECK(localToAncestorMatrix.isIdentityOrTranslation());

          clipRect = localToAncestorMatrix.mapRect(clipRect);
          filterOrigin = localToAncestorMatrix.mapPoint(filterOrigin);
        }

        const bool kLcdTextRequiresOpaqueLayer = true;
        ccList.CreateAndAppendPairedBeginItem<cc::CompositingDisplayItem>(
            static_cast<uint8_t>(
                gfx::ToFlooredInt(255 * pairedState->effect()->opacity())),
            pairedState->effect()->blendMode(),
            // TODO(chrishtr): compute bounds as necessary.
            nullptr, nullptr, kLcdTextRequiresOpaqueLayer);

        ccList.CreateAndAppendPairedBeginItem<cc::FilterDisplayItem>(
            pairedState->effect()->filter().asCcFilterOperations(), clipRect,
            gfx::PointF(filterOrigin.x(), filterOrigin.y()));

        endDisplayItems.push_back(EndEffect);
        break;
      }
      case PropertyTreeState::None:
        break;
    }
  }

  if (hasClip) {
    applyClipsBetweenStates(clipSpace, mappedClipDestinationSpace, ccList,
                            endDisplayItems, geometryMapper);
  }
}

static void recordPairedEndDisplayItems(
    const Vector<EndDisplayItemType>& endDisplayItemTypes,
    cc::DisplayItemList* ccList) {
  for (Vector<EndDisplayItemType>::const_reverse_iterator endType =
           endDisplayItemTypes.rbegin();
       endType != endDisplayItemTypes.rend(); ++endType) {
    switch (*endType) {
      case EndTransform:
        ccList->CreateAndAppendPairedEndItem<cc::EndTransformDisplayItem>();
        break;
      case EndClip:
        ccList->CreateAndAppendPairedEndItem<cc::EndFloatClipDisplayItem>();
        break;
      case EndEffect:
        ccList->CreateAndAppendPairedEndItem<cc::EndFilterDisplayItem>();
        ccList->CreateAndAppendPairedEndItem<cc::EndCompositingDisplayItem>();
        break;
    }
  }
}

}  // namespace

scoped_refptr<cc::DisplayItemList> PaintArtifactCompositor::recordPendingLayer(
    const PaintArtifact& artifact,
    const PendingLayer& pendingLayer,
    const gfx::Rect& combinedBounds,
    GeometryMapper& geometryMapper) {
  cc::DisplayItemListSettings settings;
  scoped_refptr<cc::DisplayItemList> ccList =
      cc::DisplayItemList::Create(settings);

  gfx::Transform translation;
  translation.Translate(-combinedBounds.x(), -combinedBounds.y());
  // Passing combinedBounds as the visual rect for the begin/end transform item
  // would normally be the sensible thing to do, but see comment above re:
  // visual rects for drawing items and further rework in flight.
  ccList->CreateAndAppendPairedBeginItem<cc::TransformDisplayItem>(translation);

  const DisplayItemList& displayItems = artifact.getDisplayItemList();
  for (const auto& paintChunk : pendingLayer.paintChunks) {
    const PropertyTreeState* state = &paintChunk->properties.propertyTreeState;
    PropertyTreeStateIterator iterator(*state);
    Vector<PropertyTreeState> pairedStates;
    for (; state && *state != pendingLayer.propertyTreeState;
         state = iterator.next()) {
      if (state->innermostNode() != PropertyTreeState::None)
        pairedStates.push_back(*state);
    }

    // TODO(chrishtr): we can avoid some extra paired display items if
    // multiple PaintChunks share them. We can also collapse clips between
    // transforms into single clips in the same way that PaintLayerClipper does.
    Vector<EndDisplayItemType> endDisplayItems;

    recordPairedBeginDisplayItems(pairedStates, pendingLayer.propertyTreeState,
                                  *ccList.get(), endDisplayItems,
                                  geometryMapper);

    for (const auto& displayItem : displayItems.itemsInPaintChunk(*paintChunk))
      appendDisplayItemToCcDisplayItemList(displayItem, ccList.get());

    recordPairedEndDisplayItems(endDisplayItems, ccList.get());
  }

  ccList->CreateAndAppendPairedEndItem<cc::EndTransformDisplayItem>();

  ccList->Finalize();
  return ccList;
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
#if DCHECK_IS_ON
  for (const auto& paintChunk : pendingLayer.paintChunks) {
    DCHECK(paintChunk.properties == firstPaintChunk.properties);
  }
#endif

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

  scoped_refptr<cc::DisplayItemList> displayList = recordPendingLayer(
      paintArtifact, pendingLayer, ccCombinedBounds, geometryMapper);
  contentLayerClient->SetDisplayList(std::move(displayList));
  contentLayerClient->SetPaintableRegion(gfx::Rect(ccCombinedBounds.size()));

  layerOffset = ccCombinedBounds.OffsetFromOrigin();

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

  newContentLayerClients.append(std::move(contentLayerClient));
  return ccPictureLayer;
}

namespace {

class PropertyTreeManager {
  WTF_MAKE_NONCOPYABLE(PropertyTreeManager);

 public:
  PropertyTreeManager(cc::PropertyTrees& propertyTrees, cc::Layer* rootLayer)
      : m_propertyTrees(propertyTrees),
        m_rootLayer(rootLayer)
  {
    setupRootTransformNode();
    setupRootClipNode();
    setupRootEffectNode();
    setupRootScrollNode();
  }

  void setupRootTransformNode();
  void setupRootClipNode();
  void setupRootEffectNode();
  void setupRootScrollNode();

  int compositorIdForTransformNode(const TransformPaintPropertyNode*);
  int compositorIdForClipNode(const ClipPaintPropertyNode*);
  int switchToEffectNode(const EffectPaintPropertyNode& nextEffect);
  int compositorIdForCurrentEffectNode() const {
    return m_effectStack.back().id;
  }
  int compositorIdForScrollNode(const ScrollPaintPropertyNode*);

  // Scroll offset has special treatment in the transform and scroll trees.
  void updateScrollOffset(int layerId, int scrollId);

 private:
  void buildEffectNodesRecursively(const EffectPaintPropertyNode* nextEffect);

  cc::TransformTree& transformTree() { return m_propertyTrees.transform_tree; }
  cc::ClipTree& clipTree() { return m_propertyTrees.clip_tree; }
  cc::EffectTree& effectTree() { return m_propertyTrees.effect_tree; }
  cc::ScrollTree& scrollTree() { return m_propertyTrees.scroll_tree; }

  const EffectPaintPropertyNode* currentEffectNode() const {
    return m_effectStack.back().effect;
  }

  // Property trees which should be updated by the manager.
  cc::PropertyTrees& m_propertyTrees;

  // Layer to which transform "owner" layers should be added. These will not
  // have any actual children, but at present must exist in the tree.
  cc::Layer* m_rootLayer;

  // Maps from Blink-side property tree nodes to cc property node indices.
  HashMap<const TransformPaintPropertyNode*, int> m_transformNodeMap;
  HashMap<const ClipPaintPropertyNode*, int> m_clipNodeMap;
  HashMap<const ScrollPaintPropertyNode*, int> m_scrollNodeMap;

  struct BlinkEffectAndCcIdPair {
    const EffectPaintPropertyNode* effect;
    int id;
  };
  Vector<BlinkEffectAndCcIdPair> m_effectStack;

#if DCHECK_IS_ON()
  HashSet<const EffectPaintPropertyNode*> m_effectNodesConverted;
#endif
};

void PropertyTreeManager::setupRootTransformNode() {
  // cc is hardcoded to use transform node index 1 for device scale and
  // transform.
  cc::TransformTree& transformTree = m_propertyTrees.transform_tree;
  transformTree.clear();
  cc::TransformNode& transformNode = *transformTree.Node(
      transformTree.Insert(cc::TransformNode(), kRealRootNodeId));
  DCHECK_EQ(transformNode.id, kSecondaryRootNodeId);
  transformNode.source_node_id = transformNode.parent_id;
  transformTree.SetTargetId(transformNode.id, kRealRootNodeId);
  transformTree.SetContentTargetId(transformNode.id, kRealRootNodeId);

  // TODO(jaydasika): We shouldn't set ToScreeen and FromScreen of root
  // transform node here. They should be set while updating transform tree in
  // cc.
  float deviceScaleFactor = m_rootLayer->GetLayerTree()->device_scale_factor();
  gfx::Transform toScreen;
  toScreen.Scale(deviceScaleFactor, deviceScaleFactor);
  transformTree.SetToScreen(kRealRootNodeId, toScreen);
  gfx::Transform fromScreen;
  bool invertible = toScreen.GetInverse(&fromScreen);
  DCHECK(invertible);
  transformTree.SetFromScreen(kRealRootNodeId, fromScreen);
  transformTree.set_needs_update(true);

  m_transformNodeMap.set(TransformPaintPropertyNode::root(), transformNode.id);
  m_rootLayer->SetTransformTreeIndex(transformNode.id);
}

void PropertyTreeManager::setupRootClipNode() {
  // cc is hardcoded to use clip node index 1 for viewport clip.
  cc::ClipTree& clipTree = m_propertyTrees.clip_tree;
  clipTree.clear();
  m_propertyTrees.clip_id_to_index_map.clear();
  cc::ClipNode& clipNode =
      *clipTree.Node(clipTree.Insert(cc::ClipNode(), kRealRootNodeId));
  DCHECK_EQ(clipNode.id, kSecondaryRootNodeId);

  clipNode.resets_clip = true;
  clipNode.owner_id = m_rootLayer->id();
  clipNode.clip_type = cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP;
  clipNode.clip = gfx::RectF(
      gfx::SizeF(m_rootLayer->GetLayerTree()->device_viewport_size()));
  clipNode.transform_id = kRealRootNodeId;
  clipNode.target_transform_id = kRealRootNodeId;
  clipNode.target_effect_id = kSecondaryRootNodeId;
  m_propertyTrees.clip_id_to_index_map[clipNode.owner_id] = clipNode.id;

  m_clipNodeMap.set(ClipPaintPropertyNode::root(), clipNode.id);
  m_rootLayer->SetClipTreeIndex(clipNode.id);
}

void PropertyTreeManager::setupRootEffectNode() {
  // cc is hardcoded to use effect node index 1 for root render surface.
  cc::EffectTree& effectTree = m_propertyTrees.effect_tree;
  effectTree.clear();
  m_propertyTrees.effect_id_to_index_map.clear();
  cc::EffectNode& effectNode =
      *effectTree.Node(effectTree.Insert(cc::EffectNode(), kInvalidNodeId));
  DCHECK_EQ(effectNode.id, kSecondaryRootNodeId);
  effectNode.owner_id = m_rootLayer->id();
  effectNode.transform_id = kRealRootNodeId;
  effectNode.clip_id = kSecondaryRootNodeId;
  effectNode.has_render_surface = true;
  m_propertyTrees.effect_id_to_index_map[effectNode.owner_id] = effectNode.id;

  m_effectStack.append(
      BlinkEffectAndCcIdPair{EffectPaintPropertyNode::root(), effectNode.id});
  m_rootLayer->SetEffectTreeIndex(effectNode.id);
}

void PropertyTreeManager::setupRootScrollNode() {
  cc::ScrollTree& scrollTree = m_propertyTrees.scroll_tree;
  scrollTree.clear();
  m_propertyTrees.scroll_id_to_index_map.clear();
  cc::ScrollNode& scrollNode =
      *scrollTree.Node(scrollTree.Insert(cc::ScrollNode(), kRealRootNodeId));
  DCHECK_EQ(scrollNode.id, kSecondaryRootNodeId);
  scrollNode.owner_id = m_rootLayer->id();
  scrollNode.transform_id = kSecondaryRootNodeId;
  m_propertyTrees.scroll_id_to_index_map[scrollNode.owner_id] = scrollNode.id;

  m_scrollNodeMap.set(ScrollPaintPropertyNode::root(), scrollNode.id);
  m_rootLayer->SetScrollTreeIndex(scrollNode.id);
}

int PropertyTreeManager::compositorIdForTransformNode(
    const TransformPaintPropertyNode* transformNode) {
  DCHECK(transformNode);
  // TODO(crbug.com/645615): Remove the failsafe here.
  if (!transformNode)
    return kSecondaryRootNodeId;

  auto it = m_transformNodeMap.find(transformNode);
  if (it != m_transformNodeMap.end())
    return it->value;

  scoped_refptr<cc::Layer> dummyLayer = cc::Layer::Create();
  int parentId = compositorIdForTransformNode(transformNode->parent());
  int id = transformTree().Insert(cc::TransformNode(), parentId);

  cc::TransformNode& compositorNode = *transformTree().Node(id);
  transformTree().SetTargetId(id, kRealRootNodeId);
  transformTree().SetContentTargetId(id, kRealRootNodeId);
  compositorNode.source_node_id = parentId;

  FloatPoint3D origin = transformNode->origin();
  compositorNode.pre_local.matrix().setTranslate(-origin.x(), -origin.y(),
                                                 -origin.z());
  compositorNode.local.matrix() =
      TransformationMatrix::toSkMatrix44(transformNode->matrix());
  compositorNode.post_local.matrix().setTranslate(origin.x(), origin.y(),
                                                  origin.z());
  compositorNode.needs_local_transform_update = true;
  compositorNode.flattens_inherited_transform =
      transformNode->flattensInheritedTransform();
  compositorNode.sorting_context_id = transformNode->renderingContextId();

  m_rootLayer->AddChild(dummyLayer);
  dummyLayer->SetTransformTreeIndex(id);
  dummyLayer->SetClipTreeIndex(kSecondaryRootNodeId);
  dummyLayer->SetEffectTreeIndex(kSecondaryRootNodeId);
  dummyLayer->SetScrollTreeIndex(kRealRootNodeId);
  dummyLayer->set_property_tree_sequence_number(kPropertyTreeSequenceNumber);

  auto result = m_transformNodeMap.set(transformNode, id);
  DCHECK(result.isNewEntry);
  transformTree().set_needs_update(true);
  return id;
}

int PropertyTreeManager::compositorIdForClipNode(
    const ClipPaintPropertyNode* clipNode) {
  DCHECK(clipNode);
  // TODO(crbug.com/645615): Remove the failsafe here.
  if (!clipNode)
    return kSecondaryRootNodeId;

  auto it = m_clipNodeMap.find(clipNode);
  if (it != m_clipNodeMap.end())
    return it->value;

  scoped_refptr<cc::Layer> dummyLayer = cc::Layer::Create();
  int parentId = compositorIdForClipNode(clipNode->parent());
  int id = clipTree().Insert(cc::ClipNode(), parentId);

  cc::ClipNode& compositorNode = *clipTree().Node(id);
  compositorNode.owner_id = dummyLayer->id();
  m_propertyTrees.clip_id_to_index_map[compositorNode.owner_id] = id;

  // TODO(jbroman): Don't discard rounded corners.
  compositorNode.clip = clipNode->clipRect().rect();
  compositorNode.transform_id =
      compositorIdForTransformNode(clipNode->localTransformSpace());
  compositorNode.target_transform_id = kRealRootNodeId;
  compositorNode.target_effect_id = kSecondaryRootNodeId;
  compositorNode.clip_type = cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP;
  compositorNode.layers_are_clipped = true;
  compositorNode.layers_are_clipped_when_surfaces_disabled = true;

  m_rootLayer->AddChild(dummyLayer);
  dummyLayer->SetTransformTreeIndex(compositorNode.transform_id);
  dummyLayer->SetClipTreeIndex(id);
  dummyLayer->SetEffectTreeIndex(kSecondaryRootNodeId);
  dummyLayer->SetScrollTreeIndex(kRealRootNodeId);
  dummyLayer->set_property_tree_sequence_number(kPropertyTreeSequenceNumber);

  auto result = m_clipNodeMap.set(clipNode, id);
  DCHECK(result.isNewEntry);
  clipTree().set_needs_update(true);
  return id;
}

int PropertyTreeManager::compositorIdForScrollNode(
    const ScrollPaintPropertyNode* scrollNode) {
  DCHECK(scrollNode);
  // TODO(crbug.com/645615): Remove the failsafe here.
  if (!scrollNode)
    return kSecondaryRootNodeId;

  auto it = m_scrollNodeMap.find(scrollNode);
  if (it != m_scrollNodeMap.end())
    return it->value;

  int parentId = compositorIdForScrollNode(scrollNode->parent());
  int id = scrollTree().Insert(cc::ScrollNode(), parentId);

  cc::ScrollNode& compositorNode = *scrollTree().Node(id);
  compositorNode.owner_id = parentId;
  m_propertyTrees.scroll_id_to_index_map[compositorNode.owner_id] = id;

  compositorNode.scrollable = true;

  compositorNode.scroll_clip_layer_bounds.SetSize(scrollNode->clip().width(),
                                                  scrollNode->clip().height());
  compositorNode.bounds.SetSize(scrollNode->bounds().width(),
                                scrollNode->bounds().height());
  compositorNode.user_scrollable_horizontal =
      scrollNode->userScrollableHorizontal();
  compositorNode.user_scrollable_vertical =
      scrollNode->userScrollableVertical();
  compositorNode.transform_id =
      compositorIdForTransformNode(scrollNode->scrollOffsetTranslation());
  compositorNode.main_thread_scrolling_reasons =
      scrollNode->mainThreadScrollingReasons();

  auto result = m_scrollNodeMap.set(scrollNode, id);
  DCHECK(result.isNewEntry);
  scrollTree().set_needs_update(true);

  return id;
}

void PropertyTreeManager::updateScrollOffset(int layerId, int scrollId) {
  cc::ScrollNode& scrollNode = *scrollTree().Node(scrollId);
  cc::TransformNode& transformNode =
      *transformTree().Node(scrollNode.transform_id);

  transformNode.scrolls = true;

  // Blink creates a 2d transform node just for scroll offset whereas cc's
  // transform node has a special scroll offset field. To handle this we
  // adjust cc's transform node to remove the 2d scroll translation and
  // let the cc scroll tree update the cc scroll offset.
  DCHECK(transformNode.local.IsIdentityOr2DTranslation());
  auto offset = transformNode.local.To2dTranslation();
  transformNode.local.MakeIdentity();
  scrollTree().SetScrollOffset(layerId,
                               gfx::ScrollOffset(-offset.x(), -offset.y()));
  scrollTree().set_needs_update(true);
}

unsigned depth(const EffectPaintPropertyNode* node) {
  unsigned result = 0;
  for (; node; node = node->parent())
    result++;
  return result;
}

// TODO(chrishtr): templatize this to avoid duplication of
// GeometryMapper::leastCommonAncestor.
const EffectPaintPropertyNode* lowestCommonAncestor(
    const EffectPaintPropertyNode* nodeA,
    const EffectPaintPropertyNode* nodeB) {
  // Optimized common case.
  if (nodeA == nodeB)
    return nodeA;

  unsigned depthA = depth(nodeA), depthB = depth(nodeB);
  while (depthA > depthB) {
    nodeA = nodeA->parent();
    depthA--;
  }
  while (depthB > depthA) {
    nodeB = nodeB->parent();
    depthB--;
  }
  DCHECK_EQ(depthA, depthB);
  while (nodeA != nodeB) {
    nodeA = nodeA->parent();
    nodeB = nodeB->parent();
  }
  return nodeA;
}

int PropertyTreeManager::switchToEffectNode(
    const EffectPaintPropertyNode& nextEffect) {
  const EffectPaintPropertyNode* ancestor =
      lowestCommonAncestor(currentEffectNode(), &nextEffect);
  DCHECK(ancestor) << "Malformed effect tree. All nodes must be descendant of "
                      "EffectPaintPropertyNode::root().";
  while (currentEffectNode() != ancestor)
    m_effectStack.pop_back();

  // Now the current effect is the lowest common ancestor of previous effect
  // and the next effect. That implies it is an existing node that already has
  // at least one paint chunk or child effect, and we are going to either attach
  // another paint chunk or child effect to it. We can no longer omit render
  // surface for it even for opacity-only nodes.
  // See comments in PropertyTreeManager::buildEffectNodesRecursively().
  // TODO(crbug.com/504464): Remove premature optimization here.
  if (currentEffectNode() && currentEffectNode()->opacity() != 1.f) {
    effectTree().Node(compositorIdForCurrentEffectNode())->has_render_surface =
        true;
  }

  buildEffectNodesRecursively(&nextEffect);

  return compositorIdForCurrentEffectNode();
}

void PropertyTreeManager::buildEffectNodesRecursively(
    const EffectPaintPropertyNode* nextEffect) {
  if (nextEffect == currentEffectNode())
    return;
  DCHECK(nextEffect);

  buildEffectNodesRecursively(nextEffect->parent());
  DCHECK_EQ(nextEffect->parent(), currentEffectNode());

#if DCHECK_IS_ON()
  DCHECK(!m_effectNodesConverted.contains(nextEffect))
      << "Malformed paint artifact. Paint chunks under the same effect should "
         "be contiguous.";
  m_effectNodesConverted.add(nextEffect);
#endif

  // An effect node can't omit render surface if it has child with exotic
  // blending mode. See comments below for more detail.
  // TODO(crbug.com/504464): Remove premature optimization here.
  if (nextEffect->blendMode() != SkBlendMode::kSrcOver) {
    effectTree().Node(compositorIdForCurrentEffectNode())->has_render_surface =
        true;
  }

  // We currently create dummy layers to host effect nodes and corresponding
  // render surfaces. This should be removed once cc implements better support
  // for freestanding property trees.
  scoped_refptr<cc::Layer> dummyLayer = nextEffect->ensureDummyLayer();
  m_rootLayer->AddChild(dummyLayer);

  int outputClipId = compositorIdForClipNode(nextEffect->outputClip());

  cc::EffectNode& effectNode = *effectTree().Node(effectTree().Insert(
      cc::EffectNode(), compositorIdForCurrentEffectNode()));
  effectNode.owner_id = dummyLayer->id();
  effectNode.clip_id = outputClipId;
  // Every effect is supposed to have render surface enabled for grouping,
  // but we can get away without one if the effect is opacity-only and has only
  // one compositing child with kSrcOver blend mode. This is both for
  // optimization and not introducing sub-pixel differences in layout tests.
  // See PropertyTreeManager::switchToEffectNode() and above where we
  // retrospectively enable render surface when more than one compositing child
  // or a child with exotic blend mode is detected.
  // TODO(crbug.com/504464): There is ongoing work in cc to delay render surface
  // decision until later phase of the pipeline. Remove premature optimization
  // here once the work is ready.
  if (!nextEffect->filter().isEmpty() ||
      nextEffect->blendMode() != SkBlendMode::kSrcOver)
    effectNode.has_render_surface = true;
  effectNode.opacity = nextEffect->opacity();
  effectNode.filters = nextEffect->filter().asCcFilterOperations();
  effectNode.blend_mode = nextEffect->blendMode();
  m_propertyTrees.effect_id_to_index_map[effectNode.owner_id] = effectNode.id;
  m_effectStack.append(BlinkEffectAndCcIdPair{nextEffect, effectNode.id});

  dummyLayer->set_property_tree_sequence_number(kPropertyTreeSequenceNumber);
  dummyLayer->SetTransformTreeIndex(kSecondaryRootNodeId);
  dummyLayer->SetClipTreeIndex(outputClipId);
  dummyLayer->SetEffectTreeIndex(effectNode.id);
  dummyLayer->SetScrollTreeIndex(kRealRootNodeId);
}

}  // namespace

bool PaintArtifactCompositor::canMergeInto(
    const PaintArtifact& paintArtifact,
    const PaintChunk& newChunk,
    const PendingLayer& candidatePendingLayer) {
  const PaintChunk& pendingLayerFirstChunk =
      *candidatePendingLayer.paintChunks[0];
  if (paintArtifact.getDisplayItemList()[newChunk.beginIndex].isForeignLayer())
    return false;

  if (paintArtifact.getDisplayItemList()[pendingLayerFirstChunk.beginIndex]
          .isForeignLayer())
    return false;

  if (newChunk.properties.backfaceHidden !=
      pendingLayerFirstChunk.properties.backfaceHidden)
    return false;

  DCHECK_GE(candidatePendingLayer.paintChunks.size(), 1u);
  PropertyTreeStateIterator iterator(newChunk.properties.propertyTreeState);
  for (const PropertyTreeState* currentState =
           &newChunk.properties.propertyTreeState;
       currentState; currentState = iterator.next()) {
    if (currentState->hasDirectCompositingReasons())
      return false;
    if (*currentState == candidatePendingLayer.propertyTreeState)
      return true;
  }
  return false;
}

bool PaintArtifactCompositor::mightOverlap(
    const PaintChunk& paintChunk,
    const PendingLayer& candidatePendingLayer,
    GeometryMapper& geometryMapper) {
  PropertyTreeState rootPropertyTreeState(
      TransformPaintPropertyNode::root(), ClipPaintPropertyNode::root(),
      EffectPaintPropertyNode::root(), ScrollPaintPropertyNode::root());

  bool success = false;
  FloatRect paintChunkScreenVisualRect =
      geometryMapper.localToVisualRectInAncestorSpace(
          paintChunk.bounds, paintChunk.properties.propertyTreeState,
          rootPropertyTreeState, success);
  DCHECK(success);

  success = false;
  FloatRect pendingLayerScreenVisualRect =
      geometryMapper.localToVisualRectInAncestorSpace(
          candidatePendingLayer.bounds, candidatePendingLayer.propertyTreeState,
          rootPropertyTreeState, success);
  DCHECK(success);

  return paintChunkScreenVisualRect.intersects(pendingLayerScreenVisualRect);
}

PaintArtifactCompositor::PendingLayer::PendingLayer(
    const PaintChunk& firstPaintChunk)
    : bounds(firstPaintChunk.bounds),
      knownToBeOpaque(firstPaintChunk.knownToBeOpaque),
      backfaceHidden(firstPaintChunk.properties.backfaceHidden),
      propertyTreeState(firstPaintChunk.properties.propertyTreeState) {
  paintChunks.append(&firstPaintChunk);
}

void PaintArtifactCompositor::PendingLayer::add(
    const PaintChunk& paintChunk,
    GeometryMapper* geometryMapper) {
  DCHECK(paintChunk.properties.backfaceHidden == backfaceHidden);
  paintChunks.append(&paintChunk);
  FloatRect mappedBounds = paintChunk.bounds;
  if (geometryMapper) {
    bool success = false;
    mappedBounds = geometryMapper->localToAncestorRect(
        mappedBounds, paintChunk.properties.propertyTreeState,
        propertyTreeState, success);
    DCHECK(success);
  }
  bounds.unite(mappedBounds);
  if (bounds.size() != paintChunks[0]->bounds.size()) {
    if (bounds.size() != paintChunk.bounds.size())
      knownToBeOpaque = false;
    else
      knownToBeOpaque = paintChunk.knownToBeOpaque;
  }
}

void PaintArtifactCompositor::collectPendingLayers(
    const PaintArtifact& paintArtifact,
    Vector<PendingLayer>& pendingLayers,
    GeometryMapper& geometryMapper) {
  // n = # of paint chunks. Memoizing canMergeInto() can get it to O(n^2), and
  // other heuristics can make worst-case behavior better.
  for (const PaintChunk& paintChunk : paintArtifact.paintChunks()) {
    bool createNew = true;
    for (Vector<PendingLayer>::reverse_iterator candidatePendingLayer =
             pendingLayers.rbegin();
         candidatePendingLayer != pendingLayers.rend();
         ++candidatePendingLayer) {
      if (canMergeInto(paintArtifact, paintChunk, *candidatePendingLayer)) {
        candidatePendingLayer->add(paintChunk, &geometryMapper);
        createNew = false;
        break;
      }
      if (mightOverlap(paintChunk, *candidatePendingLayer, geometryMapper)) {
        break;
      }
    }
    if (createNew)
      pendingLayers.append(PendingLayer(paintChunk));
  }
}

void PaintArtifactCompositor::update(
    const PaintArtifact& paintArtifact,
    RasterInvalidationTrackingMap<const PaintChunk>* rasterChunkInvalidations,
    bool storeDebugInfo) {
#ifndef NDEBUG
  storeDebugInfo = true;
#endif

  DCHECK(m_rootLayer);

  cc::LayerTree* layerTree = m_rootLayer->GetLayerTree();

  // The tree will be null after detaching and this update can be ignored.
  // See: WebViewImpl::detachPaintArtifactCompositor().
  if (!layerTree)
    return;

  if (m_extraDataForTestingEnabled)
    m_extraDataForTesting = WTF::wrapUnique(new ExtraDataForTesting);

  m_rootLayer->RemoveAllChildren();
  m_rootLayer->set_property_tree_sequence_number(kPropertyTreeSequenceNumber);

  PropertyTreeManager propertyTreeManager(*layerTree->property_trees(),
                                          m_rootLayer.get());

  Vector<PendingLayer, 0> pendingLayers;
  GeometryMapper geometryMapper;
  collectPendingLayers(paintArtifact, pendingLayers, geometryMapper);

  Vector<std::unique_ptr<ContentLayerClientImpl>> newContentLayerClients;
  newContentLayerClients.reserveCapacity(paintArtifact.paintChunks().size());
  for (const PendingLayer& pendingLayer : pendingLayers) {
    gfx::Vector2dF layerOffset;
    scoped_refptr<cc::Layer> layer = compositedLayerForPendingLayer(
        paintArtifact, pendingLayer, layerOffset, newContentLayerClients,
        rasterChunkInvalidations, storeDebugInfo, geometryMapper);

    int transformId = propertyTreeManager.compositorIdForTransformNode(
        pendingLayer.propertyTreeState.transform());
    int scrollId = propertyTreeManager.compositorIdForScrollNode(
        pendingLayer.propertyTreeState.scroll());
    int clipId = propertyTreeManager.compositorIdForClipNode(
        pendingLayer.propertyTreeState.clip());
    int effectId = propertyTreeManager.switchToEffectNode(
        *pendingLayer.propertyTreeState.effect());

    propertyTreeManager.updateScrollOffset(layer->id(), scrollId);

    layer->set_offset_to_transform_parent(layerOffset);

    m_rootLayer->AddChild(layer);
    layer->set_property_tree_sequence_number(kPropertyTreeSequenceNumber);
    layer->SetTransformTreeIndex(transformId);
    layer->SetClipTreeIndex(clipId);
    layer->SetEffectTreeIndex(effectId);
    layer->SetScrollTreeIndex(scrollId);

    // TODO(jbroman): This probably shouldn't be necessary, but it is still
    // queried by RenderSurfaceImpl.
    layer->Set3dSortingContextId(layerTree->property_trees()
                                     ->transform_tree.Node(transformId)
                                     ->sorting_context_id);

    layer->SetShouldCheckBackfaceVisibility(pendingLayer.backfaceHidden);

    if (m_extraDataForTestingEnabled)
      m_extraDataForTesting->contentLayers.append(layer);
  }
  m_contentLayerClients.clear();
  m_contentLayerClients.swap(newContentLayerClients);

  // Mark the property trees as having been rebuilt.
  layerTree->property_trees()->sequence_number = kPropertyTreeSequenceNumber;
  layerTree->property_trees()->needs_rebuild = false;
  layerTree->property_trees()->ResetCachedData();
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
