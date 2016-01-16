// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintArtifactCompositor.h"

#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_settings.h"
#include "cc/layers/picture_layer.h"
#include "cc/playback/display_item_list.h"
#include "cc/playback/display_item_list_settings.h"
#include "cc/playback/drawing_display_item.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "skia/ext/refptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"

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
}

PaintArtifactCompositor::~PaintArtifactCompositor()
{
}

void PaintArtifactCompositor::initializeIfNeeded()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
    if (m_rootLayer)
        return;

    m_rootLayer = cc::Layer::Create(cc::LayerSettings());
    m_webLayer = adoptPtr(Platform::current()->compositorSupport()->createLayerFromCCLayer(m_rootLayer.get()));
}

static void appendDisplayItemToCcDisplayItemList(const DisplayItem& displayItem, cc::DisplayItemList* list, gfx::Rect& combinedBounds)
{
    if (DisplayItem::isDrawingType(displayItem.type())) {
        const SkPicture* picture = static_cast<const DrawingDisplayItem&>(displayItem).picture();
        if (!picture)
            return;
        gfx::Rect bounds = gfx::SkIRectToRect(picture->cullRect().roundOut());
        list->CreateAndAppendItem<cc::DrawingDisplayItem>(bounds, skia::SharePtr(picture));
        combinedBounds.Union(bounds);
    }
}

static scoped_refptr<cc::DisplayItemList> recordPaintChunk(const PaintArtifact& artifact, const PaintChunk& chunk, gfx::Rect& combinedBounds)
{
    cc::DisplayItemListSettings settings;
    scoped_refptr<cc::DisplayItemList> list = cc::DisplayItemList::Create(gfx::Rect(), settings);

    const DisplayItemList& displayItems = artifact.displayItemList();
    for (size_t i = chunk.beginIndex; i < chunk.endIndex; i++)
        appendDisplayItemToCcDisplayItemList(displayItems[i], list.get(), combinedBounds);

    list->Finalize();
    return list;
}

void PaintArtifactCompositor::update(const PaintArtifact& paintArtifact)
{
    ASSERT(m_rootLayer);

    // TODO(jbroman): This should be incremental.
    m_rootLayer->RemoveAllChildren();
    m_contentLayerClients.clear();

    m_contentLayerClients.reserveCapacity(paintArtifact.paintChunks().size());
    for (const PaintChunk& paintChunk : paintArtifact.paintChunks()) {
        // TODO(jbroman): This only really works well for chunks without an
        // offset. That really needs to be fixed.
        gfx::Rect combinedBounds;
        scoped_refptr<cc::DisplayItemList> displayList = recordPaintChunk(paintArtifact, paintChunk, combinedBounds);
        OwnPtr<ContentLayerClientImpl> contentLayerClient = adoptPtr(
            new ContentLayerClientImpl(std::move(displayList), combinedBounds));
        scoped_refptr<cc::PictureLayer> layer = cc::PictureLayer::Create(cc::LayerSettings(), contentLayerClient.get());
        layer->SetPosition(gfx::PointF());
        layer->SetBounds(combinedBounds.size());
        // TODO(jbroman): Layer transforms would be nice.
        layer->SetIsDrawable(true);
        layer->SetNeedsDisplay();
        m_contentLayerClients.append(contentLayerClient.release());
        m_rootLayer->AddChild(std::move(layer));
    }
}

} // namespace blink
