// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_layer_tree_view_impl.h"

#include "base/command_line.h"
#include "cc/font_atlas.h"
#include "cc/input_handler.h"
#include "cc/layer.h"
#include "cc/layer_tree_host.h"
#include "cc/switches.h"
#include "cc/thread.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebInputHandler.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeViewClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeView.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRenderingStats.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/compositor_bindings/web_rendering_stats_impl.h"
#include "webkit/compositor_bindings/web_to_ccinput_handler_adapter.h"


namespace WebKit {

WebLayerTreeViewImpl::WebLayerTreeViewImpl(WebLayerTreeViewClient* client)
    : client_(client) {
}

WebLayerTreeViewImpl::~WebLayerTreeViewImpl() {
}

bool WebLayerTreeViewImpl::initialize(
    const WebLayerTreeView::Settings& web_settings,
    scoped_ptr<cc::Thread> impl_thread) {
  cc::LayerTreeSettings settings;
  settings.acceleratePainting = web_settings.acceleratePainting;
  settings.renderVSyncEnabled = web_settings.renderVSyncEnabled;
  settings.perTilePaintingEnabled = web_settings.perTilePaintingEnabled;
  settings.acceleratedAnimationEnabled =
      web_settings.acceleratedAnimationEnabled;
  settings.pageScalePinchZoomEnabled = web_settings.pageScalePinchZoomEnabled;
  settings.refreshRate = web_settings.refreshRate;
  settings.defaultTileSize = web_settings.defaultTileSize;
  settings.maxUntiledLayerSize = web_settings.maxUntiledLayerSize;
  settings.initialDebugState.showFPSCounter = web_settings.showFPSCounter;
  settings.initialDebugState.showPaintRects = web_settings.showPaintRects;
  settings.initialDebugState.showPlatformLayerTree =
      web_settings.showPlatformLayerTree;
  settings.initialDebugState.showDebugBorders = web_settings.showDebugBorders;
  layer_tree_host_ = cc::LayerTreeHost::create(this,
                                               settings,
                                               impl_thread.Pass());
  if (!layer_tree_host_)
    return false;
  return true;
}

cc::LayerTreeHost* WebLayerTreeViewImpl::layer_tree_host() const {
  return layer_tree_host_.get();
}

void WebLayerTreeViewImpl::setSurfaceReady() {
  layer_tree_host_->setSurfaceReady();
}

void WebLayerTreeViewImpl::setRootLayer(const WebLayer& root) {
  layer_tree_host_->setRootLayer(
      static_cast<const WebLayerImpl*>(&root)->layer());
}

void WebLayerTreeViewImpl::clearRootLayer() {
  layer_tree_host_->setRootLayer(scoped_refptr<cc::Layer>());
}

void WebLayerTreeViewImpl::setViewportSize(
    const WebSize& layout_viewport_size, const WebSize& device_viewport_size) {
  if (!device_viewport_size.isEmpty()) {
    layer_tree_host_->setViewportSize(layout_viewport_size,
                                      device_viewport_size);
  } else {
    layer_tree_host_->setViewportSize(layout_viewport_size,
                                      layout_viewport_size);
  }
}

WebSize WebLayerTreeViewImpl::layoutViewportSize() const {
  return layer_tree_host_->layoutViewportSize();
}

WebSize WebLayerTreeViewImpl::deviceViewportSize() const {
  return layer_tree_host_->deviceViewportSize();
}

WebFloatPoint WebLayerTreeViewImpl::adjustEventPointForPinchZoom(
    const WebFloatPoint& point) const {
  return layer_tree_host_->adjustEventPointForPinchZoom(point);
}

void WebLayerTreeViewImpl::setDeviceScaleFactor(float device_scale_factor) {
  layer_tree_host_->setDeviceScaleFactor(device_scale_factor);
}

float WebLayerTreeViewImpl::deviceScaleFactor() const {
  return layer_tree_host_->deviceScaleFactor();
}

void WebLayerTreeViewImpl::setBackgroundColor(WebColor color) {
  layer_tree_host_->setBackgroundColor(color);
}

void WebLayerTreeViewImpl::setHasTransparentBackground(bool transparent) {
  layer_tree_host_->setHasTransparentBackground(transparent);
}

void WebLayerTreeViewImpl::setVisible(bool visible) {
  layer_tree_host_->setVisible(visible);
}

void WebLayerTreeViewImpl::setPageScaleFactorAndLimits(float page_scale_factor,
                                                       float minimum,
                                                       float maximum) {
  layer_tree_host_->setPageScaleFactorAndLimits(page_scale_factor,
                                                minimum, maximum);
}

void WebLayerTreeViewImpl::startPageScaleAnimation(const WebPoint& scroll,
                                                   bool use_anchor,
                                                   float new_page_scale,
                                                   double duration_sec) {
  int64 duration_us = duration_sec * base::Time::kMicrosecondsPerSecond;
  base::TimeDelta duration = base::TimeDelta::FromMicroseconds(duration_us);
  layer_tree_host_->startPageScaleAnimation(gfx::Vector2d(scroll.x, scroll.y),
                                            use_anchor,
                                            new_page_scale,
                                            duration);
}

void WebLayerTreeViewImpl::setNeedsAnimate() {
  layer_tree_host_->setNeedsAnimate();
}

void WebLayerTreeViewImpl::setNeedsRedraw() {
  layer_tree_host_->setNeedsRedraw();
}

bool WebLayerTreeViewImpl::commitRequested() const {
  return layer_tree_host_->commitRequested();
}

void WebLayerTreeViewImpl::composite() {
  layer_tree_host_->composite();
}

void WebLayerTreeViewImpl::didStopFlinging() {
  layer_tree_host_->didStopFlinging();
}

void WebLayerTreeViewImpl::updateAnimations(double frame_begin_time_seconds) {
  int64 frame_begin_time_us =
      frame_begin_time_seconds * base::Time::kMicrosecondsPerSecond;
  base::TimeTicks frame_begin_time =
      base::TimeTicks::FromInternalValue(frame_begin_time_us);
  layer_tree_host_->updateAnimations(frame_begin_time);
}

bool WebLayerTreeViewImpl::compositeAndReadback(void* pixels,
                                                const WebRect& rect) {
  return layer_tree_host_->compositeAndReadback(pixels, rect);
}

void WebLayerTreeViewImpl::finishAllRendering() {
  layer_tree_host_->finishAllRendering();
}

void WebLayerTreeViewImpl::setDeferCommits(bool defer_commits) {
  layer_tree_host_->setDeferCommits(defer_commits);
}

void WebLayerTreeViewImpl::renderingStats(WebRenderingStats& stats) const {
  layer_tree_host_->renderingStats(
      &static_cast<WebRenderingStatsImpl&>(stats).rendering_stats);
}

void WebLayerTreeViewImpl::setShowFPSCounter(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debugState();
  debug_state.showFPSCounter = show;
  layer_tree_host_->setDebugState(debug_state);
}

void WebLayerTreeViewImpl::setShowPaintRects(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debugState();
  debug_state.showPaintRects = show;
  layer_tree_host_->setDebugState(debug_state);
}

void WebLayerTreeViewImpl::setContinuousPaintingEnabled(bool enabled) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debugState();
  debug_state.continuousPainting = enabled;
  layer_tree_host_->setDebugState(debug_state);
}

scoped_ptr<cc::FontAtlas> WebLayerTreeViewImpl::createFontAtlas() {
  int font_height;
  WebRect ascii_to_web_rect_table[128];
  gfx::Rect ascii_to_rect_table[128];
  SkBitmap bitmap;

  client_->createFontAtlas(bitmap, ascii_to_web_rect_table, font_height);

  for (int i = 0; i < 128; ++i)
    ascii_to_rect_table[i] = ascii_to_web_rect_table[i];

  return cc::FontAtlas::create(bitmap, ascii_to_rect_table, font_height).Pass();
}

void WebLayerTreeViewImpl::loseCompositorContext(int num_times) {
  layer_tree_host_->loseOutputSurface(num_times);
}

void WebLayerTreeViewImpl::willBeginFrame() {
  client_->willBeginFrame();
}

void WebLayerTreeViewImpl::didBeginFrame() {
  client_->didBeginFrame();
}

void WebLayerTreeViewImpl::animate(double monotonic_frame_begin_time) {
  client_->updateAnimations(monotonic_frame_begin_time);
}

void WebLayerTreeViewImpl::layout() {
  client_->layout();
}

void WebLayerTreeViewImpl::applyScrollAndScale(gfx::Vector2d scroll_delta,
                                               float page_scale) {
  client_->applyScrollAndScale(scroll_delta, page_scale);
}

scoped_ptr<cc::OutputSurface> WebLayerTreeViewImpl::createOutputSurface() {
  WebKit::WebCompositorOutputSurface* web = client_->createOutputSurface();
  return make_scoped_ptr(static_cast<cc::OutputSurface*>(web));
}

void WebLayerTreeViewImpl::didRecreateOutputSurface(bool success) {
  client_->didRecreateOutputSurface(success);
}

scoped_ptr<cc::InputHandler> WebLayerTreeViewImpl::createInputHandler() {
  scoped_ptr<cc::InputHandler> ret;
  scoped_ptr<WebInputHandler> handler(client_->createInputHandler());
  if (handler)
    ret = WebToCCInputHandlerAdapter::create(handler.Pass());
  return ret.Pass();
}

void WebLayerTreeViewImpl::willCommit() {
  client_->willCommit();
}

void WebLayerTreeViewImpl::didCommit() {
  client_->didCommit();
}

void WebLayerTreeViewImpl::didCommitAndDrawFrame() {
  client_->didCommitAndDrawFrame();
}

void WebLayerTreeViewImpl::didCompleteSwapBuffers() {
  client_->didCompleteSwapBuffers();
}

void WebLayerTreeViewImpl::scheduleComposite() {
  client_->scheduleComposite();
}

}  // namespace WebKit
