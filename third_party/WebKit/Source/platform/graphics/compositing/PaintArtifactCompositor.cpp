// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintArtifactCompositor.h"

#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/playback/display_item_list.h"
#include "cc/playback/display_item_list_settings.h"
#include "cc/playback/drawing_display_item.h"
#include "cc/playback/transform_display_item.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/ForeignLayerDisplayItem.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "skia/ext/refptr.h"
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
#include "wtf/PassOwnPtr.h"
#include <algorithm>
#include <utility>

namespace blink {

class PaintArtifactCompositor::ContentLayerClientImpl : public cc::ContentLayerClient {
    WTF_MAKE_NONCOPYABLE(ContentLayerClientImpl);
    USING_FAST_MALLOC(ContentLayerClientImpl);
public:
    ContentLayerClientImpl(scoped_refptr<cc::DisplayItemList> list, const gfx::Rect& paintableRegion)
        : m_ccDisplayItemList(std::move(list)), m_paintableRegion(paintableRegion) { }

    // cc::ContentLayerClient
    gfx::Rect PaintableRegion() override { return m_paintableRegion; }
    scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(PaintingControlSetting) override
    {
        return m_ccDisplayItemList;
    }
    bool FillsBoundsCompletely() const override { return false; }
    size_t GetApproximateUnsharedMemoryUsage() const override
    {
        // TODO(jbroman): Actually calculate memory usage.
        return 0;
    }

private:
    scoped_refptr<cc::DisplayItemList> m_ccDisplayItemList;
    gfx::Rect m_paintableRegion;
};

PaintArtifactCompositor::PaintArtifactCompositor()
{
    if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        return;
    m_rootLayer = cc::Layer::Create();
    m_webLayer = adoptPtr(Platform::current()->compositorSupport()->createLayerFromCCLayer(m_rootLayer.get()));
}

PaintArtifactCompositor::~PaintArtifactCompositor()
{
}

namespace {

static void appendDisplayItemToCcDisplayItemList(const DisplayItem& displayItem, cc::DisplayItemList* list)
{
    if (DisplayItem::isDrawingType(displayItem.getType())) {
        const SkPicture* picture = static_cast<const DrawingDisplayItem&>(displayItem).picture();
        if (!picture)
            return;
        gfx::Rect bounds = gfx::SkIRectToRect(picture->cullRect().roundOut());
        list->CreateAndAppendItem<cc::DrawingDisplayItem>(bounds, sk_ref_sp(picture));
    }
}

static scoped_refptr<cc::DisplayItemList> recordPaintChunk(const PaintArtifact& artifact, const PaintChunk& chunk, const gfx::Rect& combinedBounds)
{
    cc::DisplayItemListSettings settings;
    scoped_refptr<cc::DisplayItemList> list = cc::DisplayItemList::Create(
        gfx::Rect(combinedBounds.size()), settings);

    gfx::Transform translation;
    translation.Translate(-combinedBounds.x(), -combinedBounds.y());
    // TODO(jbroman, wkorman): What visual rectangle is wanted here?
    list->CreateAndAppendItem<cc::TransformDisplayItem>(gfx::Rect(), translation);

    const DisplayItemList& displayItems = artifact.getDisplayItemList();
    for (const auto& displayItem : displayItems.itemsInPaintChunk(chunk))
        appendDisplayItemToCcDisplayItemList(displayItem, list.get());

    list->CreateAndAppendItem<cc::EndTransformDisplayItem>(gfx::Rect());

    list->Finalize();
    return list;
}

static gfx::Transform transformToTransformSpace(const TransformPaintPropertyNode* currentSpace, const TransformPaintPropertyNode* targetSpace)
{
    TransformationMatrix matrix;
    while (currentSpace != targetSpace) {
        TransformationMatrix localMatrix = currentSpace->matrix();
        localMatrix.applyTransformOrigin(currentSpace->origin());
        matrix = localMatrix * matrix;
        currentSpace = currentSpace->parent();
    }
    return gfx::Transform(TransformationMatrix::toSkMatrix44(matrix));
}

unsigned clipNodeDepth(const ClipPaintPropertyNode* node)
{
    unsigned depth = 0;
    while (node) {
        depth++;
        node = node->parent();
    }
    return depth;
}

const ClipPaintPropertyNode* nearestCommonAncestor(const ClipPaintPropertyNode* a, const ClipPaintPropertyNode* b)
{
    // Measure both depths.
    unsigned depthA = clipNodeDepth(a);
    unsigned depthB = clipNodeDepth(b);

    // Make it so depthA >= depthB.
    if (depthA < depthB) {
        std::swap(a, b);
        std::swap(depthA, depthB);
    }

    // Make it so depthA == depthB.
    while (depthA > depthB) {
        a = a->parent();
        depthA--;
    }

    // Walk up until we find the ancestor.
    while (a != b) {
        a = a->parent();
        b = b->parent();
    }
    return a;
}

const TransformPaintPropertyNode* localTransformSpace(const ClipPaintPropertyNode* clip)
{
    return clip ? clip->localTransformSpace() : nullptr;
}

scoped_refptr<cc::Layer> createClipLayer(const ClipPaintPropertyNode* node)
{
    // TODO(jbroman): Handle rounded-rect clips.
    // TODO(jbroman): Handle clips of non-integer size.
    gfx::RectF clipRect = node->clipRect().rect();

    // TODO(jbroman): This, and the similar logic in
    // PaintArtifactCompositor::update, will need to be updated to account for
    // other kinds of intermediate layers, such as those that apply effects.
    // TODO(jbroman): This assumes that the transform space of this node's
    // parent is an ancestor of this node's transform space. That's not
    // necessarily true, and this should be fixed. crbug.com/597156
    gfx::Transform transform = transformToTransformSpace(localTransformSpace(node), localTransformSpace(node->parent()));
    gfx::Vector2dF offset = clipRect.OffsetFromOrigin();
    transform.Translate(offset.x(), offset.y());
    if (node->parent()) {
        FloatPoint offsetDueToParentClipOffset = node->parent()->clipRect().rect().location();
        gfx::Transform undoClipOffset;
        undoClipOffset.Translate(-offsetDueToParentClipOffset.x(), -offsetDueToParentClipOffset.y());
        transform.ConcatTransform(undoClipOffset);
    }

    scoped_refptr<cc::Layer> layer = cc::Layer::Create();
    layer->SetIsDrawable(false);
    layer->SetMasksToBounds(true);
    layer->SetPosition(gfx::PointF());
    layer->SetBounds(gfx::ToRoundedSize(clipRect.size()));
    layer->SetTransform(transform);
    return layer;
}

class ClipLayerManager {
public:
    ClipLayerManager(cc::Layer* rootLayer)
    {
        m_clipLayers.append(NodeLayerPair(nullptr, rootLayer));
    }

    cc::Layer* switchToNewClipLayer(const ClipPaintPropertyNode* clip)
    {
        // Walk up to the nearest common ancestor.
        const auto* ancestor = nearestCommonAncestor(clip, m_clipLayers.last().first);
        while (m_clipLayers.last().first != ancestor)
            m_clipLayers.removeLast();

        // If the new one was an ancestor, we're done.
        cc::Layer* ancestorClipLayer = m_clipLayers.last().second;
        if (ancestor == clip)
            return ancestorClipLayer;

        // Otherwise, we need to build new clip layers.
        // We do this from the bottom up.
        size_t numExistingClipLayers = m_clipLayers.size();
        scoped_refptr<cc::Layer> childLayer;
        do {
            scoped_refptr<cc::Layer> clipLayer = createClipLayer(clip);
            m_clipLayers.append(NodeLayerPair(clip, clipLayer.get()));
            if (childLayer)
                clipLayer->AddChild(std::move(childLayer));
            childLayer = std::move(clipLayer);
            clip = clip->parent();
        } while (ancestor != clip);
        ancestorClipLayer->AddChild(std::move(childLayer));

        // Rearrange the new clip layers to be in top-down order, like they
        // should be.
        std::reverse(m_clipLayers.begin() + numExistingClipLayers, m_clipLayers.end());

        // Return the last (bottom-most) clip layer.
        return m_clipLayers.last().second;
    }

private:
    using NodeLayerPair = std::pair<const ClipPaintPropertyNode*, cc::Layer*>;
    Vector<NodeLayerPair, 16> m_clipLayers;
};

scoped_refptr<cc::Layer> foreignLayerForPaintChunk(const PaintArtifact& paintArtifact, const PaintChunk& paintChunk, gfx::Transform transform)
{
    if (paintChunk.size() != 1)
        return nullptr;

    const auto& displayItem = paintArtifact.getDisplayItemList()[paintChunk.beginIndex];
    if (!displayItem.isForeignLayer())
        return nullptr;

    const auto& foreignLayerDisplayItem = static_cast<const ForeignLayerDisplayItem&>(displayItem);
    scoped_refptr<cc::Layer> layer = foreignLayerDisplayItem.layer();
    transform.Translate(foreignLayerDisplayItem.location().x(), foreignLayerDisplayItem.location().y());
    layer->SetTransform(transform);
    layer->SetBounds(foreignLayerDisplayItem.bounds());
    layer->SetIsDrawable(true);
    return layer;
}

} // namespace

void PaintArtifactCompositor::update(const PaintArtifact& paintArtifact)
{
    ASSERT(m_rootLayer);

    // TODO(jbroman): This should be incremental.
    m_rootLayer->RemoveAllChildren();
    m_contentLayerClients.clear();

    m_contentLayerClients.reserveCapacity(paintArtifact.paintChunks().size());
    ClipLayerManager clipLayerManager(m_rootLayer.get());
    for (const PaintChunk& paintChunk : paintArtifact.paintChunks()) {
        cc::Layer* parent = clipLayerManager.switchToNewClipLayer(paintChunk.properties.clip.get());
        // TODO(jbroman): Same as above. This assumes the transform space of the current clip is
        // an ancestor of the chunk. It is not necessarily true. crbug.com/597156
        gfx::Transform transform = transformToTransformSpace(paintChunk.properties.transform.get(), localTransformSpace(paintChunk.properties.clip.get()));
        scoped_refptr<cc::Layer> layer = layerForPaintChunk(paintArtifact, paintChunk, transform);
        layer->SetNeedsDisplay();
        parent->AddChild(std::move(layer));
    }
}

scoped_refptr<cc::Layer> PaintArtifactCompositor::layerForPaintChunk(const PaintArtifact& paintArtifact, const PaintChunk& paintChunk, gfx::Transform transform)
{
    // If the paint chunk is a foreign layer, just return that layer.
    if (scoped_refptr<cc::Layer> foreignLayer = foreignLayerForPaintChunk(paintArtifact, paintChunk, transform))
        return foreignLayer;

    // The common case: create a layer for painted content.
    gfx::Rect combinedBounds = enclosingIntRect(paintChunk.bounds);
    scoped_refptr<cc::DisplayItemList> displayList = recordPaintChunk(paintArtifact, paintChunk, combinedBounds);
    OwnPtr<ContentLayerClientImpl> contentLayerClient = adoptPtr(
        new ContentLayerClientImpl(std::move(displayList), gfx::Rect(combinedBounds.size())));

    // Include the offset in the transform, because it needs to apply in
    // this layer's transform space (whereas layer position applies in its
    // parent's transform space).
    gfx::Vector2dF offset = gfx::PointF(combinedBounds.origin()).OffsetFromOrigin();
    transform.Translate(offset.x(), offset.y());

    // If a clip was applied, its origin needs to be cancelled out in
    // this transform.
    if (const auto* clip = paintChunk.properties.clip.get()) {
        FloatPoint offsetDueToClipOffset = clip->clipRect().rect().location();
        gfx::Transform undoClipOffset;
        undoClipOffset.Translate(-offsetDueToClipOffset.x(), -offsetDueToClipOffset.y());
        transform.ConcatTransform(undoClipOffset);
    }

    scoped_refptr<cc::PictureLayer> layer = cc::PictureLayer::Create(contentLayerClient.get());
    layer->SetBounds(combinedBounds.size());
    layer->SetTransform(transform);
    layer->SetIsDrawable(true);
    layer->SetDoubleSided(!paintChunk.properties.backfaceHidden);
    if (paintChunk.knownToBeOpaque)
        layer->SetContentsOpaque(true);
    m_contentLayerClients.append(contentLayerClient.release());
    return layer;
}


} // namespace blink
