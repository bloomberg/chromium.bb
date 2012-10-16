// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_compositor_support_impl.h"

#include "config.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/compositor_bindings/WebLayerImpl.h"
#include "webkit/compositor_bindings/WebLayerTreeViewImpl.h"
#include "webkit/compositor_bindings/WebCompositorImpl.h"
#include "webkit/compositor_bindings/WebContentLayerImpl.h"
#include "webkit/compositor_bindings/WebDelegatedRendererLayerImpl.h"
#include "webkit/compositor_bindings/WebExternalTextureLayerImpl.h"
#include "webkit/compositor_bindings/WebIOSurfaceLayerImpl.h"
#include "webkit/compositor_bindings/WebSolidColorLayerImpl.h"
#include "webkit/compositor_bindings/WebImageLayerImpl.h"
#include "webkit/compositor_bindings/WebVideoLayerImpl.h"
#include "webkit/compositor_bindings/WebScrollbarLayerImpl.h"
#include "webkit/compositor_bindings/WebAnimationImpl.h"
#include "webkit/compositor_bindings/WebFloatAnimationCurveImpl.h"
#include "webkit/compositor_bindings/WebTransformAnimationCurveImpl.h"

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
