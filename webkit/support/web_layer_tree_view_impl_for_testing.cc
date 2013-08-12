// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/web_layer_tree_view_impl_for_testing.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "cc/base/switches.h"
#include "cc/debug/fake_web_graphics_context_3d.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/output/software_output_device.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebLayer.h"
#include "third_party/WebKit/public/platform/WebLayerTreeView.h"
#include "third_party/WebKit/public/platform/WebRenderingStats.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "webkit/common/gpu/test_context_provider_factory.h"
#include "webkit/renderer/compositor_bindings/web_layer_impl.h"
#include "webkit/support/test_webkit_platform_support.h"

using WebKit::WebColor;
using WebKit::WebGraphicsContext3D;
using WebKit::WebRect;
using WebKit::WebRenderingStats;
using WebKit::WebSize;

namespace webkit {

WebLayerTreeViewImplForTesting::WebLayerTreeViewImplForTesting() {}

WebLayerTreeViewImplForTesting::~WebLayerTreeViewImplForTesting() {}

bool WebLayerTreeViewImplForTesting::Initialize() {
  cc::LayerTreeSettings settings;

  // For web contents, layer transforms should scale up the contents of layers
  // to keep content always crisp when possible.
  settings.layer_transforms_should_scale_layer_contents = true;

  // Accelerated animations are enabled for unit tests.
  settings.accelerated_animation_enabled = true;
  layer_tree_host_ = cc::LayerTreeHost::Create(this, settings, NULL);
  if (!layer_tree_host_)
    return false;
  return true;
}

void WebLayerTreeViewImplForTesting::setSurfaceReady() {
  layer_tree_host_->SetLayerTreeHostClientReady();
}

void WebLayerTreeViewImplForTesting::setRootLayer(
    const WebKit::WebLayer& root) {
  layer_tree_host_->SetRootLayer(
      static_cast<const WebLayerImpl*>(&root)->layer());
}

void WebLayerTreeViewImplForTesting::clearRootLayer() {
  layer_tree_host_->SetRootLayer(scoped_refptr<cc::Layer>());
}

void WebLayerTreeViewImplForTesting::setViewportSize(
    const WebSize& unused_deprecated,
    const WebSize& device_viewport_size) {
  layer_tree_host_->SetViewportSize(device_viewport_size);
}

WebSize WebLayerTreeViewImplForTesting::layoutViewportSize() const {
  return layer_tree_host_->device_viewport_size();
}

WebSize WebLayerTreeViewImplForTesting::deviceViewportSize() const {
  return layer_tree_host_->device_viewport_size();
}

void WebLayerTreeViewImplForTesting::setDeviceScaleFactor(
    float device_scale_factor) {
  layer_tree_host_->SetDeviceScaleFactor(device_scale_factor);
}

float WebLayerTreeViewImplForTesting::deviceScaleFactor() const {
  return layer_tree_host_->device_scale_factor();
}

void WebLayerTreeViewImplForTesting::setBackgroundColor(WebColor color) {
  layer_tree_host_->set_background_color(color);
}

void WebLayerTreeViewImplForTesting::setHasTransparentBackground(
    bool transparent) {
  layer_tree_host_->set_has_transparent_background(transparent);
}

void WebLayerTreeViewImplForTesting::setVisible(bool visible) {
  layer_tree_host_->SetVisible(visible);
}

void WebLayerTreeViewImplForTesting::setPageScaleFactorAndLimits(
    float page_scale_factor,
    float minimum,
    float maximum) {
  layer_tree_host_->SetPageScaleFactorAndLimits(
      page_scale_factor, minimum, maximum);
}

void WebLayerTreeViewImplForTesting::startPageScaleAnimation(
    const WebKit::WebPoint& scroll,
    bool use_anchor,
    float new_page_scale,
    double duration_sec) {}

void WebLayerTreeViewImplForTesting::setNeedsAnimate() {
  layer_tree_host_->SetNeedsAnimate();
}

void WebLayerTreeViewImplForTesting::setNeedsRedraw() {
  layer_tree_host_->SetNeedsRedraw();
}

bool WebLayerTreeViewImplForTesting::commitRequested() const {
  return layer_tree_host_->CommitRequested();
}

void WebLayerTreeViewImplForTesting::composite() {
  layer_tree_host_->Composite(base::TimeTicks::Now());
}

void WebLayerTreeViewImplForTesting::didStopFlinging() {}

bool WebLayerTreeViewImplForTesting::compositeAndReadback(
    void* pixels, const WebRect& rect_in_device_viewport) {
  return layer_tree_host_->CompositeAndReadback(pixels,
                                                rect_in_device_viewport);
}

void WebLayerTreeViewImplForTesting::finishAllRendering() {
  layer_tree_host_->FinishAllRendering();
}

void WebLayerTreeViewImplForTesting::setDeferCommits(bool defer_commits) {
  layer_tree_host_->SetDeferCommits(defer_commits);
}

void WebLayerTreeViewImplForTesting::renderingStats(WebRenderingStats&) const {}

void WebLayerTreeViewImplForTesting::Layout() {
}

void WebLayerTreeViewImplForTesting::ApplyScrollAndScale(
    gfx::Vector2d scroll_delta,
    float page_scale) {}

scoped_ptr<cc::OutputSurface>
WebLayerTreeViewImplForTesting::CreateOutputSurface(bool fallback) {
  scoped_ptr<cc::OutputSurface> surface;
  scoped_ptr<WebGraphicsContext3D> context3d(
      new cc::FakeWebGraphicsContext3D);
  surface.reset(new cc::OutputSurface(context3d.Pass()));
  return surface.Pass();
}

void WebLayerTreeViewImplForTesting::ScheduleComposite() {
}

scoped_refptr<cc::ContextProvider>
WebLayerTreeViewImplForTesting::OffscreenContextProviderForMainThread() {
  return webkit::gpu::TestContextProviderFactory::GetInstance()->
      OffscreenContextProviderForMainThread();
}

scoped_refptr<cc::ContextProvider>
WebLayerTreeViewImplForTesting::OffscreenContextProviderForCompositorThread() {
  NOTREACHED();
  return NULL;
}

}  // namespace webkit
