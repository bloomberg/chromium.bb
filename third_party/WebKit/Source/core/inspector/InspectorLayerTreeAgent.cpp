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
#include "core/page/Page.h"
#include "core/platform/graphics/IntRect.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderLayerBacking.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderView.h"
#include "public/platform/WebLayer.h"

namespace WebCore {

namespace LayerTreeAgentState {
static const char layerTreeAgentEnabled[] = "layerTreeAgentEnabled";
};

static PassRefPtr<TypeBuilder::LayerTree::Layer> buildObjectForLayer(GraphicsLayer* graphicsLayer, int nodeId, bool forceRoot)
{
    RefPtr<TypeBuilder::LayerTree::Layer> layerObject = TypeBuilder::LayerTree::Layer::create()
        .setLayerId(String::number(graphicsLayer->platformLayer()->id()))
        .setNodeId(nodeId)
        .setOffsetX(graphicsLayer->position().x())
        .setOffsetY(graphicsLayer->position().y())
        .setWidth(graphicsLayer->size().width())
        .setHeight(graphicsLayer->size().height())
        .setPaintCount(graphicsLayer->repaintCount());

    // Artificially clip tree at root renger layer's graphic layer -- it might be not the real
    // root of graphics layer hierarchy, as platform adds containing layers (e.g. for overflosw scroll).
    if (graphicsLayer->parent() && !forceRoot)
        layerObject->setParentLayerId(String::number(graphicsLayer->parent()->platformLayer()->id()));

    return layerObject;
}

static void maybeAddGraphicsLayer(GraphicsLayer* graphicsLayer, int nodeId, RefPtr<TypeBuilder::Array<TypeBuilder::LayerTree::Layer> >& layers, bool forceRoot = false)
{
    if (!graphicsLayer)
        return;
    layers->addItem(buildObjectForLayer(graphicsLayer, nodeId, forceRoot));
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

void InspectorLayerTreeAgent::didCommitLoad(Frame* frame, DocumentLoader* loader)
{
    if (loader->frame() != frame->page()->mainFrame())
        return;
}

void InspectorLayerTreeAgent::layerTreeDidChange()
{
    m_frontend->layerTreeDidChange();
}

void InspectorLayerTreeAgent::getLayers(ErrorString* errorString, const int* nodeId, RefPtr<TypeBuilder::Array<TypeBuilder::LayerTree::Layer> >& layers)
{
    layers = TypeBuilder::Array<TypeBuilder::LayerTree::Layer>::create();

    RenderView* renderView = m_page->mainFrame()->contentRenderer();
    RenderLayerCompositor* compositor = renderView ? renderView->compositor() : 0;
    if (!compositor) {
        *errorString = "Not in compositing mode";
        return;
    }
    if (!nodeId) {
        gatherLayersUsingRenderLayerHierarchy(errorString, compositor->rootRenderLayer(), layers);
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
    gatherLayersUsingRenderObjectHierarchy(errorString, renderer, layers);
}

void InspectorLayerTreeAgent::addRenderLayerBacking(ErrorString* errorString, RenderLayerBacking* layerBacking, Node* node, RefPtr<TypeBuilder::Array<TypeBuilder::LayerTree::Layer> >& layers)
{
    int nodeId = idForNode(errorString, node);
    RenderLayerCompositor* compositor = layerBacking->owningLayer()->compositor();
    bool forceRoot = layerBacking->owningLayer()->isRootLayer();
    if (layerBacking->ancestorClippingLayer()) {
        maybeAddGraphicsLayer(layerBacking->ancestorClippingLayer(), nodeId, layers, forceRoot);
        forceRoot = false;
    }
    maybeAddGraphicsLayer(layerBacking->graphicsLayer(), nodeId, layers, forceRoot);
    maybeAddGraphicsLayer(layerBacking->clippingLayer(), nodeId, layers);
    maybeAddGraphicsLayer(layerBacking->foregroundLayer(), nodeId, layers);
    maybeAddGraphicsLayer(layerBacking->backgroundLayer(), nodeId, layers);
    maybeAddGraphicsLayer(layerBacking->scrollingLayer(), nodeId, layers);
    maybeAddGraphicsLayer(layerBacking->scrollingContentsLayer(), nodeId, layers);
    maybeAddGraphicsLayer(layerBacking->layerForHorizontalScrollbar(), nodeId, layers);
    maybeAddGraphicsLayer(layerBacking->layerForVerticalScrollbar(), nodeId, layers);
}

void InspectorLayerTreeAgent::gatherLayersUsingRenderObjectHierarchy(ErrorString* errorString, RenderObject* renderer, RefPtr<TypeBuilder::Array<TypeBuilder::LayerTree::Layer> >& layers)
{
    if (renderer->hasLayer()) {
        gatherLayersUsingRenderLayerHierarchy(errorString, toRenderLayerModelObject(renderer)->layer(), layers);
        return;
    }

    for (renderer = renderer->firstChild(); renderer; renderer = renderer->nextSibling())
        gatherLayersUsingRenderObjectHierarchy(errorString, renderer, layers);
}

void InspectorLayerTreeAgent::gatherLayersUsingRenderLayerHierarchy(ErrorString* errorString, RenderLayer* renderLayer, RefPtr<TypeBuilder::Array<TypeBuilder::LayerTree::Layer> >& layers)
{
    if (renderLayer->isComposited()) {
        Node* node = (renderLayer->isReflection() ? renderLayer->parent() : renderLayer)->renderer()->generatingNode();
        addRenderLayerBacking(errorString, renderLayer->backing(), node, layers);
    }
    for (renderLayer = renderLayer->firstChild(); renderLayer; renderLayer = renderLayer->nextSibling())
        gatherLayersUsingRenderLayerHierarchy(errorString, renderLayer, layers);
}

int InspectorLayerTreeAgent::idForNode(ErrorString* errorString, Node* node)
{
    InspectorDOMAgent* domAgent = m_instrumentingAgents->inspectorDOMAgent();

    int nodeId = domAgent->boundNodeId(node);
    if (!nodeId)
        nodeId = domAgent->pushNodeToFrontend(errorString, domAgent->boundNodeId(node->document()), node);

    return nodeId;
}

} // namespace WebCore
