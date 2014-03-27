// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/compositing/CompositingPropertyUpdater.h"

#include "core/rendering/RenderLayer.h"

namespace WebCore {

CompositingPropertyUpdater::CompositingPropertyUpdater()
    : m_geometryMap(UseTransforms)
{
}

CompositingPropertyUpdater::~CompositingPropertyUpdater()
{
}

void CompositingPropertyUpdater::updateAncestorDependentProperties(RenderLayer* layer, UpdateType updateType)
{
    if (!layer->childNeedsToUpdateAncestorDependantProperties() && updateType != ForceUpdate)
        return;

    m_geometryMap.pushMappingsToAncestor(layer, layer->parent());

    if (layer->needsToUpdateAncestorDependentProperties())
        updateType = ForceUpdate;

    if (updateType == ForceUpdate) {
        RenderLayer::AncestorDependentProperties properties;

        if (!layer->isRootLayer())
            properties.absoluteBoundingBox = enclosingIntRect(m_geometryMap.absoluteRect(layer->overlapBounds()));

        layer->updateAncestorDependentProperties(properties);
    }

    for (RenderLayer* child = layer->firstChild(); child; child = child->nextSibling())
        updateAncestorDependentProperties(child, updateType);

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
