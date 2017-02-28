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
#include "cc/playback/compositing_display_item.h"
#include "cc/playback/display_item_list.h"
#include "cc/playback/drawing_display_item.h"
#include "cc/playback/filter_display_item.h"
#include "cc/playback/float_clip_display_item.h"
#include "cc/playback/transform_display_item.h"
#include "cc/trees/layer_tree_host.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
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

namespace {

static gfx::Rect largeRect(-200000, -200000, 400000, 400000);

static void appendDisplayItemToCcDisplayItemList(const DisplayItem& displayItem,
                                                 cc::DisplayItemList* list) {
  if (DisplayItem::isDrawingType(displayItem.getType())) {
    const PaintRecord* record =
        static_cast<const DrawingDisplayItem&>(displayItem).GetPaintRecord();
    if (!record)
      return;
    // In theory we would pass the bounds of the record, previously done as:
    // gfx::Rect bounds = gfx::SkIRectToRect(record->cullRect().roundOut());
    // or use the visual rect directly. However, clip content layers attempt
    // to raster in a different space than that of the visual rects. We'll be
    // reworking visual rects further for SPv2, so for now we just pass a
    // visual rect large enough to make sure items raster.
    list->CreateAndAppendDrawingItem<cc::DrawingDisplayItem>(largeRect,
                                                             sk_ref_sp(record));
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
    const TransformationMatrix& localToAncestorMatrix =
        geometryMapper.localToAncestorMatrix(transformNode,
                                             ancestorState.transform());
    // Clips are only in descendant spaces that are transformed by one
    // or more scrolls.
    DCHECK(localToAncestorMatrix.isIdentityOrTranslation());
  }
#endif

  FloatRect combinedClip =
      geometryMapper.localToAncestorClipRect(localState, ancestorState).rect();

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
          const TransformPaintPropertyNode* transformNode =
              pairedState->effect()->localTransformSpace();
          const TransformationMatrix& localToAncestorMatrix =
              geometryMapper.localToAncestorMatrix(transformNode,
                                                   pairedState->transform());
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
            nullptr, GraphicsContext::WebCoreColorFilterToSkiaColorFilter(
                         pairedState->effect()->colorFilter()),
            kLcdTextRequiresOpaqueLayer);

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
  auto ccList = make_scoped_refptr(new cc::DisplayItemList);

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

  newContentLayerClients.push_back(std::move(contentLayerClient));
  return ccPictureLayer;
}

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
    if (*currentState == candidatePendingLayer.propertyTreeState)
      return true;
    if (currentState->hasDirectCompositingReasons())
      return false;
  }
  return false;
}

bool PaintArtifactCompositor::mightOverlap(
    const PaintChunk& paintChunk,
    const PendingLayer& candidatePendingLayer,
    GeometryMapper& geometryMapper) {
  PropertyTreeState rootPropertyTreeState(TransformPaintPropertyNode::root(),
                                          ClipPaintPropertyNode::root(),
                                          EffectPaintPropertyNode::root());

  FloatRect paintChunkScreenVisualRect =
      geometryMapper
          .localToAncestorVisualRect(paintChunk.bounds,
                                     paintChunk.properties.propertyTreeState,
                                     rootPropertyTreeState)
          .rect();

  FloatRect pendingLayerScreenVisualRect =
      geometryMapper
          .localToAncestorVisualRect(candidatePendingLayer.bounds,
                                     candidatePendingLayer.propertyTreeState,
                                     rootPropertyTreeState)
          .rect();

  return paintChunkScreenVisualRect.intersects(pendingLayerScreenVisualRect);
}

PaintArtifactCompositor::PendingLayer::PendingLayer(
    const PaintChunk& firstPaintChunk)
    : bounds(firstPaintChunk.bounds),
      knownToBeOpaque(firstPaintChunk.knownToBeOpaque),
      backfaceHidden(firstPaintChunk.properties.backfaceHidden),
      propertyTreeState(firstPaintChunk.properties.propertyTreeState) {
  paintChunks.push_back(&firstPaintChunk);
}

void PaintArtifactCompositor::PendingLayer::add(
    const PaintChunk& paintChunk,
    GeometryMapper* geometryMapper) {
  DCHECK(paintChunk.properties.backfaceHidden == backfaceHidden);
  paintChunks.push_back(&paintChunk);
  FloatRect mappedBounds = paintChunk.bounds;
  if (geometryMapper) {
    mappedBounds = geometryMapper->localToAncestorRect(
        mappedBounds, paintChunk.properties.propertyTreeState.transform(),
        propertyTreeState.transform());
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
      pendingLayers.push_back(PendingLayer(paintChunk));
  }
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
