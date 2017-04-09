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
#include "platform/wtf/CurrentTime.h"
#include "public/platform/WebRect.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/sim/SimDisplayItemList.h"

namespace blink {

static void PaintLayers(GraphicsLayer& layer,
                        SimDisplayItemList& display_list) {
  if (layer.DrawsContent() && layer.HasTrackedRasterInvalidations()) {
    ContentLayerDelegate* delegate = layer.ContentLayerDelegateForTesting();
    delegate->PaintContents(&display_list);
    layer.ResetTrackedRasterInvalidations();
  }

  if (GraphicsLayer* mask_layer = layer.MaskLayer())
    PaintLayers(*mask_layer, display_list);
  if (GraphicsLayer* contents_clipping_mask_layer =
          layer.ContentsClippingMaskLayer())
    PaintLayers(*contents_clipping_mask_layer, display_list);

  for (auto child : layer.Children())
    PaintLayers(*child, display_list);
}

static void PaintFrames(LocalFrame& root, SimDisplayItemList& display_list) {
  GraphicsLayer* layer =
      root.View()->GetLayoutViewItem().Compositor()->RootGraphicsLayer();
  PaintLayers(*layer, display_list);
}

SimCompositor::SimCompositor()
    : needs_begin_frame_(false),
      defer_commits_(true),
      has_selection_(false),
      web_view_impl_(0),
      last_frame_time_monotonic_(0) {
  FrameView::SetInitialTracksPaintInvalidationsForTesting(true);
}

SimCompositor::~SimCompositor() {
  FrameView::SetInitialTracksPaintInvalidationsForTesting(false);
}

void SimCompositor::SetWebViewImpl(WebViewImpl& web_view_impl) {
  web_view_impl_ = &web_view_impl;
}

void SimCompositor::SetNeedsBeginFrame() {
  needs_begin_frame_ = true;
}

void SimCompositor::SetDeferCommits(bool defer_commits) {
  defer_commits_ = defer_commits;
}

void SimCompositor::RegisterSelection(const WebSelection&) {
  has_selection_ = true;
}

void SimCompositor::ClearSelection() {
  has_selection_ = false;
}

SimDisplayItemList SimCompositor::BeginFrame(double time_delta_in_seconds) {
  DCHECK(web_view_impl_);
  DCHECK(!defer_commits_);
  DCHECK(needs_begin_frame_);
  DCHECK_GT(time_delta_in_seconds, 0);
  needs_begin_frame_ = false;

  last_frame_time_monotonic_ += time_delta_in_seconds;

  web_view_impl_->BeginFrame(last_frame_time_monotonic_);
  web_view_impl_->UpdateAllLifecyclePhases();

  LocalFrame* root = web_view_impl_->MainFrameImpl()->GetFrame();

  SimDisplayItemList display_list;
  PaintFrames(*root, display_list);

  return display_list;
}

}  // namespace blink
