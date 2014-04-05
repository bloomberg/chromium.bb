// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/compositing/CompositingPropertyUpdater.h"

#include "core/rendering/RenderLayer.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"

namespace WebCore {

CompositingPropertyUpdater::CompositingPropertyUpdater(RenderLayer* rootRenderLayer)
    : m_geometryMap(UseTransforms)
    , m_rootRenderLayer(rootRenderLayer)
{
}

CompositingPropertyUpdater::~CompositingPropertyUpdater()
{
}

void CompositingPropertyUpdater::updateAncestorDependentProperties(RenderLayer* layer, UpdateType updateType, RenderLayer* enclosingCompositedLayer)
{
    if (!layer->childNeedsToUpdateAncestorDependantProperties() && updateType != ForceUpdate)
        return;

    m_geometryMap.pushMappingsToAncestor(layer, layer->parent());

    if (layer->hasCompositedLayerMapping())
        enclosingCompositedLayer = layer;

    if (layer->needsToUpdateAncestorDependentProperties()) {
        if (enclosingCompositedLayer)
            enclosingCompositedLayer->compositedLayerMapping()->setNeedsGraphicsLayerUpdate();
        updateType = ForceUpdate;
    }

    if (updateType == ForceUpdate) {
        RenderLayer::AncestorDependentProperties properties;

        if (!layer->isRootLayer()) {
            properties.clippedAbsoluteBoundingBox = enclosingIntRect(m_geometryMap.absoluteRect(layer->boundingBoxForCompositingOverlapTest()));
            // FIXME: Setting the absBounds to 1x1 instead of 0x0 makes very little sense,
            // but removing this code will make JSGameBench sad.
            // See https://codereview.chromium.org/13912020/
            if (properties.clippedAbsoluteBoundingBox.isEmpty())
                properties.clippedAbsoluteBoundingBox.setSize(IntSize(1, 1));

            IntRect clipRect = pixelSnappedIntRect(layer->clipper().backgroundClipRect(ClipRectsContext(m_rootRenderLayer, AbsoluteClipRects)).rect());
            properties.clippedAbsoluteBoundingBox.intersect(clipRect);
        }

        layer->updateAncestorDependentProperties(properties);
    }

    for (RenderLayer* child = layer->firstChild(); child; child = child->nextSibling())
        updateAncestorDependentProperties(child, updateType, enclosingCompositedLayer);

    m_geometryMap.popMappingsToAncestor(layer->parent());

    layer->clearChildNeedsToUpdateAncestorDependantProperties();
}

#if !ASSERT_DISABLED

void CompositingPropertyUpdater::assertNeedsToUpdateAncestorDependantPropertiesBitsCleared(RenderLayer* layer)
{
    ASSERT(!layer->childNeedsToUpdateAncestorDependantProperties());
    ASSERT(!layer->needsToUpdateAncestorDependentProperties());

    for (RenderLayer* child = layer->firstChild(); child; child = child->nextSibling())
        assertNeedsToUpdateAncestorDependantPropertiesBitsCleared(child);
}

#endif

} // namespace WebCore
