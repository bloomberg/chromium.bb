// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/tests/sim/SimCompositor.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/ContentLayerDelegate.h"
#include "public/platform/WebRect.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimLayerTreeView.h"
#include "wtf/CurrentTime.h"

namespace blink {

static void paintLayers(PaintLayer& layer, SimDisplayItemList& displayList)
{
    if (layer.isAllowedToQueryCompositingState() && layer.compositingState() == PaintsIntoOwnBacking) {
        CompositedLayerMapping* mapping = layer.compositedLayerMapping();
        GraphicsLayer* graphicsLayer = mapping->mainGraphicsLayer();
        if (graphicsLayer->hasTrackedPaintInvalidations()) {
            ContentLayerDelegate* delegate = graphicsLayer->contentLayerDelegateForTesting();
            delegate->paintContents(&displayList, WebRect(0, 0, layer.size().width(), layer.size().height()));
            graphicsLayer->resetTrackedPaintInvalidations();
        }
    }
    for (PaintLayer* child = layer.firstChild(); child; child = child->nextSibling())
        paintLayers(*child, displayList);
}

static void paintFrames(LocalFrame& root, SimDisplayItemList& displayList)
{
    for (Frame* frame = &root; frame; frame = frame->tree().traverseNext(&root)) {
        if (!frame->isLocalFrame())
            continue;
        PaintLayer* layer = toLocalFrame(frame)->view()->layoutView()->layer();
        paintLayers(*layer, displayList);
    }
}

SimCompositor::SimCompositor(SimLayerTreeView& layerTreeView)
    : m_layerTreeView(&layerTreeView)
    , m_webViewImpl(0)
    , m_lastFrameTimeMonotonic(0)
{
    FrameView::setInitialTracksPaintInvalidationsForTesting(true);
    // Disable the debug red fill so the output display list doesn't differ in
    // size in Release vs Debug builds.
    GraphicsLayer::setDrawDebugRedFillForTesting(false);
}

SimCompositor::~SimCompositor()
{
    FrameView::setInitialTracksPaintInvalidationsForTesting(false);
    GraphicsLayer::setDrawDebugRedFillForTesting(true);
}

void SimCompositor::setWebViewImpl(WebViewImpl& webViewImpl)
{
    m_webViewImpl = &webViewImpl;
}

SimDisplayItemList SimCompositor::beginFrame()
{
    ASSERT(m_webViewImpl);
    ASSERT(!m_layerTreeView->deferCommits());
    ASSERT(m_layerTreeView->needsAnimate());
    m_layerTreeView->clearNeedsAnimate();

    // Always advance the time as if the compositor was running at 60fps.
    m_lastFrameTimeMonotonic = monotonicallyIncreasingTime() + 0.016;

    m_webViewImpl->beginFrame(m_lastFrameTimeMonotonic);
    m_webViewImpl->layout();

    LocalFrame* root = m_webViewImpl->mainFrameImpl()->frame();

    SimDisplayItemList displayList;
    paintFrames(*root, displayList);

    return displayList;
}

} // namespace blink
