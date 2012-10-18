// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "webkit/compositor_bindings/web_compositor_support_impl.h"

#include "base/memory/scoped_ptr.h"
#include "webkit/compositor_bindings/web_animation_impl.h"
#include "webkit/compositor_bindings/web_compositor_impl.h"
#include "webkit/compositor_bindings/web_content_layer_impl.h"
#include "webkit/compositor_bindings/web_delegated_renderer_layer_impl.h"
#include "webkit/compositor_bindings/web_external_texture_layer_impl.h"
#include "webkit/compositor_bindings/web_float_animation_curve_impl.h"
#include "webkit/compositor_bindings/web_image_layer_impl.h"
#include "webkit/compositor_bindings/web_io_surface_layer_impl.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/compositor_bindings/web_layer_tree_view_impl.h"
#include "webkit/compositor_bindings/web_scrollbar_layer_impl.h"
#include "webkit/compositor_bindings/web_solid_color_layer_impl.h"
#include "webkit/compositor_bindings/web_transform_animation_curve_impl.h"
#include "webkit/compositor_bindings/web_video_layer_impl.h"

using WebKit::WebAnimation;
using WebKit::WebAnimationCurve;
using WebKit::WebContentLayer;
using WebKit::WebContentLayerClient;
using WebKit::WebDelegatedRendererLayer;
using WebKit::WebExternalTextureLayer;
using WebKit::WebExternalTextureLayerClient;
using WebKit::WebFloatAnimationCurve;
using WebKit::WebIOSurfaceLayer;
using WebKit::WebImageLayer;
using WebKit::WebImageLayer;
using WebKit::WebLayer;
using WebKit::WebLayerTreeView;
using WebKit::WebLayerTreeViewClient;
using WebKit::WebScrollbar;
using WebKit::WebScrollbarLayer;
using WebKit::WebScrollbarThemeGeometry;
using WebKit::WebScrollbarThemePainter;
using WebKit::WebSolidColorLayer;
using WebKit::WebTransformAnimationCurve;
using WebKit::WebVideoFrameProvider;
using WebKit::WebVideoLayer;

using WebKit::WebCompositorImpl;

namespace webkit {

WebCompositorSupportImpl::WebCompositorSupportImpl() {
}

WebCompositorSupportImpl::~WebCompositorSupportImpl() {
}

void WebCompositorSupportImpl::initialize(WebKit::WebThread* thread) {
  WebCompositorImpl::initialize(thread);
}

bool WebCompositorSupportImpl::isThreadingEnabled() {
  return WebCompositorImpl::isThreadingEnabled();
}

void WebCompositorSupportImpl::shutdown() {
  WebCompositorImpl::shutdown();
}

void WebCompositorSupportImpl::setPerTilePaintingEnabled(bool enabled) {
  WebCompositorImpl::setPerTilePaintingEnabled(enabled);
}

void WebCompositorSupportImpl::setPartialSwapEnabled(bool enabled) {
  WebCompositorImpl::setPartialSwapEnabled(enabled);
}

void WebCompositorSupportImpl::setAcceleratedAnimationEnabled(bool enabled) {
  WebCompositorImpl::setAcceleratedAnimationEnabled(enabled);
}

void WebCompositorSupportImpl::setPageScalePinchZoomEnabled(bool enabled) {
  WebCompositorImpl::setPageScalePinchZoomEnabled(enabled);
}

WebLayerTreeView* WebCompositorSupportImpl::createLayerTreeView(
    WebLayerTreeViewClient* client, const WebLayer& root,
    const WebLayerTreeView::Settings& settings) {
  scoped_ptr<WebKit::WebLayerTreeViewImpl> layerTreeViewImpl(
      new WebKit::WebLayerTreeViewImpl(client));
  if (!layerTreeViewImpl->initialize(settings))
    return NULL;
  layerTreeViewImpl->setRootLayer(root);
  return layerTreeViewImpl.release();
}

WebLayer* WebCompositorSupportImpl::createLayer() {
  return new WebKit::WebLayerImpl();
}

WebContentLayer* WebCompositorSupportImpl::createContentLayer(
    WebContentLayerClient* client) {
  return new WebKit::WebContentLayerImpl(client);
}

WebDelegatedRendererLayer*
    WebCompositorSupportImpl::createDelegatedRendererLayer() {
  return new WebKit::WebDelegatedRendererLayerImpl();
}

WebExternalTextureLayer* WebCompositorSupportImpl::createExternalTextureLayer(
    WebExternalTextureLayerClient* client) {
  return new WebKit::WebExternalTextureLayerImpl(client);
}

WebKit::WebIOSurfaceLayer*
    WebCompositorSupportImpl::createIOSurfaceLayer() {
  return new WebKit::WebIOSurfaceLayerImpl();
}

WebKit::WebImageLayer* WebCompositorSupportImpl::createImageLayer() {
  return new WebKit::WebImageLayerImpl();
}

WebSolidColorLayer* WebCompositorSupportImpl::createSolidColorLayer() {
  return new WebKit::WebSolidColorLayerImpl();
}

WebVideoLayer* WebCompositorSupportImpl::createVideoLayer(
    WebKit::WebVideoFrameProvider* provider) {
  return new WebKit::WebVideoLayerImpl(provider);
}

WebScrollbarLayer* WebCompositorSupportImpl::createScrollbarLayer(
      WebScrollbar* scrollbar,
      WebScrollbarThemePainter painter,
      WebScrollbarThemeGeometry* geometry) {
  return new WebKit::WebScrollbarLayerImpl(scrollbar, painter, geometry);
}

WebAnimation* WebCompositorSupportImpl::createAnimation(
      const WebKit::WebAnimationCurve& curve,
      WebKit::WebAnimation::TargetProperty target,
      int animationId) {
  return new WebKit::WebAnimationImpl(curve, target, animationId);
}

WebFloatAnimationCurve* WebCompositorSupportImpl::createFloatAnimationCurve() {
  return new WebKit::WebFloatAnimationCurveImpl();
}

WebTransformAnimationCurve*
    WebCompositorSupportImpl::createTransformAnimationCurve() {
  return new WebKit::WebTransformAnimationCurveImpl();
}

}  // namespace webkit
