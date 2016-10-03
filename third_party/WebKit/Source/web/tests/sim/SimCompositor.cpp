// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/tests/sim/SimCompositor.h"

#include "core/frame/FrameView.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/ContentLayerDelegate.h"
#include "platform/graphics/GraphicsLayer.h"
#include "public/platform/WebRect.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "wtf/CurrentTime.h"

namespace blink {

static void paintLayers(GraphicsLayer& layer, SimDisplayItemList& displayList) {
  if (layer.drawsContent() && layer.hasTrackedRasterInvalidations()) {
    ContentLayerDelegate* delegate = layer.contentLayerDelegateForTesting();
    delegate->paintContents(&displayList);
    layer.resetTrackedRasterInvalidations();
  }

  if (GraphicsLayer* maskLayer = layer.maskLayer())
    paintLayers(*maskLayer, displayList);
  if (GraphicsLayer* contentsClippingMaskLayer =
          layer.contentsClippingMaskLayer())
    paintLayers(*contentsClippingMaskLayer, displayList);

  for (auto child : layer.children())
    paintLayers(*child, displayList);
}

static void paintFrames(LocalFrame& root, SimDisplayItemList& displayList) {
  GraphicsLayer* layer =
      root.view()->layoutViewItem().compositor()->rootGraphicsLayer();
  paintLayers(*layer, displayList);
}

SimCompositor::SimCompositor()
    : m_needsBeginFrame(false),
      m_deferCommits(true),
      m_hasSelection(false),
      m_webViewImpl(0),
      m_lastFrameTimeMonotonic(0) {
  FrameView::setInitialTracksPaintInvalidationsForTesting(true);
}

SimCompositor::~SimCompositor() {
  FrameView::setInitialTracksPaintInvalidationsForTesting(false);
}

void SimCompositor::setWebViewImpl(WebViewImpl& webViewImpl) {
  m_webViewImpl = &webViewImpl;
}

void SimCompositor::setNeedsBeginFrame() {
  m_needsBeginFrame = true;
}

void SimCompositor::setNeedsCompositorUpdate() {
  m_needsBeginFrame = true;
}

void SimCompositor::setDeferCommits(bool deferCommits) {
  m_deferCommits = deferCommits;
}

void SimCompositor::registerSelection(const WebSelection&) {
  m_hasSelection = true;
}

void SimCompositor::clearSelection() {
  m_hasSelection = false;
}

SimDisplayItemList SimCompositor::beginFrame() {
  DCHECK(m_webViewImpl);
  DCHECK(!m_deferCommits);
  DCHECK(m_needsBeginFrame);
  m_needsBeginFrame = false;

  // Always advance the time as if the compositor was running at 60fps.
  m_lastFrameTimeMonotonic = monotonicallyIncreasingTime() + 0.016;

  m_webViewImpl->beginFrame(m_lastFrameTimeMonotonic);
  m_webViewImpl->updateAllLifecyclePhases();

  LocalFrame* root = m_webViewImpl->mainFrameImpl()->frame();

  SimDisplayItemList displayList;
  paintFrames(*root, displayList);

  return displayList;
}

}  // namespace blink
