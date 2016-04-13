// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "wtf/CurrentTime.h"

namespace blink {

static void paintLayers(PaintLayer& layer, SimDisplayItemList& displayList)
{
    if (layer.isAllowedToQueryCompositingState() && layer.compositingState() == PaintsIntoOwnBacking) {
        CompositedLayerMapping* mapping = layer.compositedLayerMapping();
        GraphicsLayer* graphicsLayer = mapping->mainGraphicsLayer();
        if (graphicsLayer->hasTrackedPaintInvalidations()) {
            ContentLayerDelegate* delegate = graphicsLayer->contentLayerDelegateForTesting();
            delegate->paintContents(&displayList);
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

SimCompositor::SimCompositor()
    : m_needsAnimate(false)
    , m_deferCommits(true)
    , m_hasSelection(false)
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

void SimCompositor::setNeedsAnimate()
{
    m_needsAnimate = true;
}

void SimCompositor::setDeferCommits(bool deferCommits)
{
    m_deferCommits = deferCommits;
}

void SimCompositor::registerSelection(const WebSelection&)
{
    m_hasSelection = true;
}

void SimCompositor::clearSelection()
{
    m_hasSelection = false;
}

SimDisplayItemList SimCompositor::beginFrame()
{
    DCHECK(m_webViewImpl);
    DCHECK(!m_deferCommits);
    DCHECK(m_needsAnimate);
    m_needsAnimate = false;

    // Always advance the time as if the compositor was running at 60fps.
    m_lastFrameTimeMonotonic = monotonicallyIncreasingTime() + 0.016;

    m_webViewImpl->beginFrame(m_lastFrameTimeMonotonic);
    m_webViewImpl->updateAllLifecyclePhases();

    LocalFrame* root = m_webViewImpl->mainFrameImpl()->frame();

    SimDisplayItemList displayList;
    paintFrames(*root, displayList);

    return displayList;
}

} // namespace blink
