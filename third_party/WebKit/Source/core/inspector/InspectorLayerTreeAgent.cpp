/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/inspector/InspectorLayerTreeAgent.h"

#include "InspectorFrontend.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/platform/graphics/IntRect.h"
#include "core/platform/graphics/transforms/TransformationMatrix.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderLayerBacking.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderView.h"
#include "public/platform/WebCompositingReasons.h"
#include "public/platform/WebLayer.h"

namespace WebCore {

namespace LayerTreeAgentState {
static const char layerTreeAgentEnabled[] = "layerTreeAgentEnabled";
};

static PassRefPtr<TypeBuilder::LayerTree::Layer> buildObjectForLayer(GraphicsLayer* graphicsLayer, int nodeId)
{
    RefPtr<TypeBuilder::LayerTree::Layer> layerObject = TypeBuilder::LayerTree::Layer::create()
        .setLayerId(String::number(graphicsLayer->platformLayer()->id()))
        .setOffsetX(graphicsLayer->position().x())
        .setOffsetY(graphicsLayer->position().y())
        .setWidth(graphicsLayer->size().width())
        .setHeight(graphicsLayer->size().height())
        .setPaintCount(graphicsLayer->paintCount());

    if (nodeId)
        layerObject->setNodeId(nodeId);

    GraphicsLayer* parent = graphicsLayer->parent();
    if (!parent)
        parent = graphicsLayer->replicatedLayer();
    if (parent)
        layerObject->setParentLayerId(String::number(parent->platformLayer()->id()));
    if (!graphicsLayer->contentsAreVisible())
        layerObject->setInvisible(true);
    const TransformationMatrix& transform = graphicsLayer->transform();
    if (!transform.isIdentity()) {
        TransformationMatrix::FloatMatrix4 flattenedMatrix;
        transform.toColumnMajorFloatArray(flattenedMatrix);
        RefPtr<TypeBuilder::Array<double> > transformArray = TypeBuilder::Array<double>::create();
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(flattenedMatrix); ++i)
            transformArray->addItem(flattenedMatrix[i]);
        layerObject->setTransform(transformArray);
        const FloatPoint3D& anchor = graphicsLayer->anchorPoint();
        layerObject->setAnchorX(anchor.x());
        layerObject->setAnchorY(anchor.y());
        layerObject->setAnchorZ(anchor.z());
    }
    return layerObject;
}

void gatherGraphicsLayers(GraphicsLayer* root, HashMap<int, int>& layerIdToNodeIdMap, RefPtr<TypeBuilder::Array<TypeBuilder::LayerTree::Layer> >& layers)
{
    int layerId = root->platformLayer()->id();
    layers->addItem(buildObjectForLayer(root, layerIdToNodeIdMap.get(layerId)));
    if (GraphicsLayer* replica = root->replicaLayer())
        gatherGraphicsLayers(replica, layerIdToNodeIdMap, layers);
    for (size_t i = 0, size = root->children().size(); i < size; ++i)
        gatherGraphicsLayers(root->children()[i], layerIdToNodeIdMap, layers);
}

InspectorLayerTreeAgent::InspectorLayerTreeAgent(InstrumentingAgents* instrumentingAgents, InspectorCompositeState* state, InspectorDOMAgent* domAgent, Page* page)
    : InspectorBaseAgent<InspectorLayerTreeAgent>("LayerTree", instrumentingAgents, state)
    , m_frontend(0)
    , m_page(page)
    , m_domAgent(domAgent)
{
}

InspectorLayerTreeAgent::~InspectorLayerTreeAgent()
{
}

void InspectorLayerTreeAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->layertree();
}

void InspectorLayerTreeAgent::clearFrontend()
{
    m_frontend = 0;
    disable(0);
}

void InspectorLayerTreeAgent::restore()
{
    if (m_state->getBoolean(LayerTreeAgentState::layerTreeAgentEnabled))
        enable(0);
}

void InspectorLayerTreeAgent::enable(ErrorString*)
{
    m_state->setBoolean(LayerTreeAgentState::layerTreeAgentEnabled, true);
    m_instrumentingAgents->setInspectorLayerTreeAgent(this);
}

void InspectorLayerTreeAgent::disable(ErrorString*)
{
    if (!m_state->getBoolean(LayerTreeAgentState::layerTreeAgentEnabled))
        return;
    m_state->setBoolean(LayerTreeAgentState::layerTreeAgentEnabled, false);
    m_instrumentingAgents->setInspectorLayerTreeAgent(0);
}

void InspectorLayerTreeAgent::layerTreeDidChange()
{
    m_frontend->layerTreeDidChange();
}

void InspectorLayerTreeAgent::getLayers(ErrorString* errorString, const int* nodeId, RefPtr<TypeBuilder::Array<TypeBuilder::LayerTree::Layer> >& layers)
{
    LayerIdToNodeIdMap layerIdToNodeIdMap;
    layers = TypeBuilder::Array<TypeBuilder::LayerTree::Layer>::create();

    RenderLayerCompositor* compositor = renderLayerCompositor();
    if (!compositor || !compositor->inCompositingMode()) {
        *errorString = "Not in the compositing mode";
        return;
    }
    if (!nodeId) {
        buildLayerIdToNodeIdMap(errorString, compositor->rootRenderLayer(), layerIdToNodeIdMap);
        gatherGraphicsLayers(compositor->rootGraphicsLayer(), layerIdToNodeIdMap, layers);
        return;
    }
    Node* node = m_instrumentingAgents->inspectorDOMAgent()->nodeForId(*nodeId);
    if (!node) {
        *errorString = "Provided node id doesn't match any known node";
        return;
    }
    RenderObject* renderer = node->renderer();
    if (!renderer) {
        *errorString = "Node for provided node id doesn't have a renderer";
        return;
    }
    RenderLayer* enclosingLayer = renderer->enclosingLayer();
    GraphicsLayer* enclosingGraphicsLayer = enclosingLayer->enclosingCompositingLayer()->backing()->childForSuperlayers();
    buildLayerIdToNodeIdMap(errorString, enclosingLayer, layerIdToNodeIdMap);
    gatherGraphicsLayers(enclosingGraphicsLayer, layerIdToNodeIdMap, layers);
}

void InspectorLayerTreeAgent::buildLayerIdToNodeIdMap(ErrorString* errorString, RenderLayer* root, LayerIdToNodeIdMap& layerIdToNodeIdMap)
{
    if (root->isComposited()) {
        if (Node* node = root->renderer()->generatingNode()) {
            GraphicsLayer* graphicsLayer = root->backing()->childForSuperlayers();
            layerIdToNodeIdMap.set(graphicsLayer->platformLayer()->id(), idForNode(errorString, node));
        }
    }
    for (RenderLayer* child = root->firstChild(); child; child = child->nextSibling())
        buildLayerIdToNodeIdMap(errorString, child, layerIdToNodeIdMap);
    if (!root->renderer()->isRenderIFrame())
        return;
    FrameView* childFrameView = toFrameView(toRenderWidget(root->renderer())->widget());
    if (RenderView* childRenderView = childFrameView->renderView()) {
        if (RenderLayerCompositor* childCompositor = childRenderView->compositor())
            buildLayerIdToNodeIdMap(errorString, childCompositor->rootRenderLayer(), layerIdToNodeIdMap);
    }
}

int InspectorLayerTreeAgent::idForNode(ErrorString* errorString, Node* node)
{
    InspectorDOMAgent* domAgent = m_instrumentingAgents->inspectorDOMAgent();

    int nodeId = domAgent->boundNodeId(node);
    if (!nodeId)
        nodeId = domAgent->pushNodeToFrontend(errorString, domAgent->boundNodeId(&node->document()), node);

    return nodeId;
}

RenderLayerCompositor* InspectorLayerTreeAgent::renderLayerCompositor()
{
    RenderView* renderView = m_page->mainFrame()->contentRenderer();
    RenderLayerCompositor* compositor = renderView ? renderView->compositor() : 0;
    return compositor;
}

static GraphicsLayer* findLayerById(GraphicsLayer* root, int layerId)
{
    if (root->platformLayer()->id() == layerId)
        return root;
    if (root->replicaLayer()) {
        if (GraphicsLayer* layer = findLayerById(root->replicaLayer(), layerId))
            return layer;
    }
    for (size_t i = 0, size = root->children().size(); i < size; ++i) {
        if (GraphicsLayer* layer = findLayerById(root->children()[i], layerId))
            return layer;
    }
    return 0;
}

GraphicsLayer* InspectorLayerTreeAgent::layerById(ErrorString* errorString, const String& layerId)
{
    bool ok;
    int id = layerId.toInt(&ok);
    if (!ok) {
        *errorString = "Invalid layer id";
        return 0;
    }
    RenderLayerCompositor* compositor = renderLayerCompositor();
    if (!compositor)
        return 0;

    GraphicsLayer* result = findLayerById(compositor->rootGraphicsLayer(), id);
    if (!result)
        *errorString = "No layer matching given id found";
    return result;
}

struct CompositingReasonToProtocolName {
    uint64_t mask;
    const char *protocolName;
};


void InspectorLayerTreeAgent::compositingReasons(ErrorString* errorString, const String& layerId, RefPtr<TypeBuilder::Array<String> >& reasonStrings)
{
    static CompositingReasonToProtocolName compositingReasonNames[] = {
        { CompositingReason3DTransform, "transform3D" },
        { CompositingReasonVideo, "video" },
        { CompositingReasonCanvas, "canvas" },
        { CompositingReasonPlugin, "plugin" },
        { CompositingReasonIFrame, "iFrame" },
        { CompositingReasonBackfaceVisibilityHidden, "backfaceVisibilityHidden" },
        { CompositingReasonAnimation, "animation" },
        { CompositingReasonFilters, "filters" },
        { CompositingReasonPositionFixed, "positionFixed" },
        { CompositingReasonPositionSticky, "positionSticky" },
        { CompositingReasonOverflowScrollingTouch, "overflowScrollingTouch" },
        { CompositingReasonAssumedOverlap, "assumedOverlap" },
        { CompositingReasonOverlap, "overlap" },
        { CompositingReasonNegativeZIndexChildren, "negativeZIndexChildren" },
        { CompositingReasonTransformWithCompositedDescendants, "transformWithCompositedDescendants" },
        { CompositingReasonOpacityWithCompositedDescendants, "opacityWithCompositedDescendants" },
        { CompositingReasonMaskWithCompositedDescendants, "maskWithCompositedDescendants" },
        { CompositingReasonReflectionWithCompositedDescendants, "reflectionWithCompositedDescendants" },
        { CompositingReasonFilterWithCompositedDescendants, "filterWithCompositedDescendants" },
        { CompositingReasonBlendingWithCompositedDescendants, "blendingWithCompositedDescendants" },
        { CompositingReasonClipsCompositingDescendants, "clipsCompositingDescendants" },
        { CompositingReasonPerspective, "perspective" },
        { CompositingReasonPreserve3D, "preserve3D" },
        { CompositingReasonRoot, "root" },
        { CompositingReasonLayerForClip, "layerForClip" },
        { CompositingReasonLayerForScrollbar, "layerForScrollbar" },
        { CompositingReasonLayerForScrollingContainer, "layerForScrollingContainer" },
        { CompositingReasonLayerForForeground, "layerForForeground" },
        { CompositingReasonLayerForBackground, "layerForBackground" },
        { CompositingReasonLayerForMask, "layerForMask" },
        { CompositingReasonLayerForVideoOverlay, "layerForVideoOverlay" }
    };

    const GraphicsLayer* graphicsLayer = layerById(errorString, layerId);
    if (!graphicsLayer)
        return;
    WebKit::WebCompositingReasons reasonsBitmask = graphicsLayer->compositingReasons();
    reasonStrings = TypeBuilder::Array<String>::create();
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(compositingReasonNames); ++i) {
        if (!(reasonsBitmask & compositingReasonNames[i].mask))
            continue;
        reasonStrings->addItem(compositingReasonNames[i].protocolName);
#ifndef _NDEBUG
        reasonsBitmask &= ~compositingReasonNames[i].mask;
#endif
    }
    ASSERT(!reasonsBitmask);
}

} // namespace WebCore
