// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintChunksToCcLayer.h"

#include "cc/paint/compositing_display_item.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/drawing_display_item.h"
#include "cc/paint/filter_display_item.h"
#include "cc/paint/float_clip_display_item.h"
#include "cc/paint/transform_display_item.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/GeometryMapper.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/PropertyTreeState.h"

namespace blink {

namespace {

enum EndDisplayItemType { EndTransform, EndClip, EndEffect };

// Applies the clips between |localState| and |ancestorState| into a single
// combined cc::FloatClipDisplayItem on |ccList|.
static void applyClipsBetweenStates(const PropertyTreeState& localState,
                                    const PropertyTreeState& ancestorState,
                                    cc::DisplayItemList& ccList,
                                    Vector<EndDisplayItemType>& endDisplayItems,
                                    GeometryMapper& geometryMapper) {
  DCHECK(localState.transform() == ancestorState.transform());
#if DCHECK_IS_ON()
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

  const FloatClipRect& combinedClip =
      geometryMapper.localToAncestorClipRect(localState, ancestorState);

  ccList.CreateAndAppendPairedBeginItem<cc::FloatClipDisplayItem>(
      gfx::RectF(combinedClip.rect()));
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
#if DCHECK_IS_ON()
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
            nullptr,
            GraphicsContext::WebCoreColorFilterToSkiaColorFilter(
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

static gfx::Rect largeRect(-200000, -200000, 400000, 400000);
static void appendDisplayItemToCcDisplayItemList(const DisplayItem& displayItem,
                                                 cc::DisplayItemList* list) {
  if (DisplayItem::isDrawingType(displayItem.getType())) {
    sk_sp<const PaintRecord> record =
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
                                                             std::move(record));
  }
}

}  // unnamed namespace

scoped_refptr<cc::DisplayItemList> PaintChunksToCcLayer::convert(
    const Vector<const PaintChunk*>& paintChunks,
    const PropertyTreeState& layerState,
    const gfx::Vector2dF& layerOffset,
    const DisplayItemList& displayItems,
    GeometryMapper& geometryMapper) {
  auto ccList = make_scoped_refptr(new cc::DisplayItemList);

  gfx::Transform counterOffset;
  counterOffset.Translate(-layerOffset.x(), -layerOffset.y());
  ccList->CreateAndAppendPairedBeginItem<cc::TransformDisplayItem>(
      counterOffset);

  for (const auto& paintChunk : paintChunks) {
    const PropertyTreeState* state = &paintChunk->properties.propertyTreeState;
    PropertyTreeStateIterator iterator(*state);
    Vector<PropertyTreeState> pairedStates;
    for (; state && *state != layerState; state = iterator.next()) {
      if (state->innermostNode() != PropertyTreeState::None)
        pairedStates.push_back(*state);
    }

    // TODO(chrishtr): we can avoid some extra paired display items if
    // multiple PaintChunks share them. We can also collapse clips between
    // transforms into single clips in the same way that PaintLayerClipper does.
    Vector<EndDisplayItemType> endDisplayItems;

    recordPairedBeginDisplayItems(pairedStates, layerState, *ccList.get(),
                                  endDisplayItems, geometryMapper);

    for (const auto& displayItem : displayItems.itemsInPaintChunk(*paintChunk))
      appendDisplayItemToCcDisplayItemList(displayItem, ccList.get());

    recordPairedEndDisplayItems(endDisplayItems, ccList.get());
  }

  ccList->CreateAndAppendPairedEndItem<cc::EndTransformDisplayItem>();

  ccList->Finalize();
  return ccList;
}

}  // namespace blink
