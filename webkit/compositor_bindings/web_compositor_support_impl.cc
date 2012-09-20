// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_compositor_support_impl.h"

#if defined(USE_LIBCC_FOR_COMPOSITOR)
#include "config.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/compositor_bindings/WebLayerImpl.h"
#include "webkit/compositor_bindings/WebLayerTreeViewImpl.h"
#include "webkit/compositor_bindings/WebCompositorImpl.h"
#include "webkit/compositor_bindings/WebContentLayerImpl.h"
#include "webkit/compositor_bindings/WebExternalTextureLayerImpl.h"
#include "webkit/compositor_bindings/WebIOSurfaceLayerImpl.h"
#include "webkit/compositor_bindings/WebSolidColorLayerImpl.h"
#include "webkit/compositor_bindings/WebImageLayerImpl.h"
#include "webkit/compositor_bindings/WebVideoLayerImpl.h"
#include "webkit/compositor_bindings/WebScrollbarLayerImpl.h"
#include "webkit/compositor_bindings/WebAnimationImpl.h"
#include "webkit/compositor_bindings/WebFloatAnimationCurveImpl.h"
#include "webkit/compositor_bindings/WebTransformAnimationCurveImpl.h"
#else
#include "third_party/WebKit/Source/Platform/chromium/public/WebAnimation.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCompositor.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebContentLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebExternalTextureLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatAnimationCurve.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebIOSurfaceLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebImageLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbarLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSolidColorLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebTransformAnimationCurve.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVideoLayer.h"
#endif

using WebKit::WebAnimation;
using WebKit::WebAnimationCurve;
using WebKit::WebContentLayer;
using WebKit::WebContentLayerClient;
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

#if defined(USE_LIBCC_FOR_COMPOSITOR)
using WebKit::WebCompositorImpl;
#else
using WebKit::WebCompositor;
#endif

namespace webkit {

WebCompositorSupportImpl::WebCompositorSupportImpl() {
}

WebCompositorSupportImpl::~WebCompositorSupportImpl() {
}

void WebCompositorSupportImpl::initialize(WebKit::WebThread* thread) {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  WebCompositorImpl::initialize(thread);
#else
  WebCompositor::initialize(thread);
#endif
}

bool WebCompositorSupportImpl::isThreadingEnabled() {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  return WebCompositorImpl::isThreadingEnabled();
#else
  return WebCompositor::isThreadingEnabled();
#endif
}

void WebCompositorSupportImpl::shutdown() {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  WebCompositorImpl::shutdown();
#else
  WebCompositor::shutdown();
#endif
}

void WebCompositorSupportImpl::setPerTilePaintingEnabled(bool enabled) {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  WebCompositorImpl::setPerTilePaintingEnabled(enabled);
#else
  WebCompositor::setPerTilePaintingEnabled(enabled);
#endif
}

void WebCompositorSupportImpl::setPartialSwapEnabled(bool enabled) {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  WebCompositorImpl::setPartialSwapEnabled(enabled);
#else
  WebCompositor::setPartialSwapEnabled(enabled);
#endif
}

void WebCompositorSupportImpl::setAcceleratedAnimationEnabled(bool enabled) {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  WebCompositorImpl::setAcceleratedAnimationEnabled(enabled);
#else
  WebCompositor::setAcceleratedAnimationEnabled(enabled);
#endif
}


WebLayerTreeView* WebCompositorSupportImpl::createLayerTreeView(
    WebLayerTreeViewClient* client, const WebLayer& root,
    const WebLayerTreeView::Settings& settings) {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  scoped_ptr<WebKit::WebLayerTreeViewImpl> layerTreeViewImpl(
      new WebKit::WebLayerTreeViewImpl(client));
  if (!layerTreeViewImpl->initialize(settings))
    return NULL;
  layerTreeViewImpl->setRootLayer(root);
  return layerTreeViewImpl.release();
#else
  return WebLayerTreeView::create(client, root, settings);
#endif
}

WebLayer* WebCompositorSupportImpl::createLayer() {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  return new WebKit::WebLayerImpl();
#else
  return WebLayer::create();
#endif
}

WebContentLayer* WebCompositorSupportImpl::createContentLayer(
    WebContentLayerClient* client) {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  return new WebKit::WebContentLayerImpl(client);
#else
  return WebKit::WebContentLayer::create(client);
#endif
}

WebExternalTextureLayer* WebCompositorSupportImpl::createExternalTextureLayer(
    WebExternalTextureLayerClient* client) {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  return new WebKit::WebExternalTextureLayerImpl(client);
#else
  return WebKit::WebExternalTextureLayer::create(client);
#endif
}

WebKit::WebIOSurfaceLayer*
    WebCompositorSupportImpl::createIOSurfaceLayer() {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  return new WebKit::WebIOSurfaceLayerImpl();
#else
  return WebKit::WebIOSurfaceLayer::create();
#endif
}

WebKit::WebImageLayer* WebCompositorSupportImpl::createImageLayer() {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  return new WebKit::WebImageLayerImpl();
#else
  return WebKit::WebImageLayer::create();
#endif
}

WebSolidColorLayer* WebCompositorSupportImpl::createSolidColorLayer() {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  return new WebKit::WebSolidColorLayerImpl();
#else
  return WebKit::WebSolidColorLayer::create();
#endif
}

WebVideoLayer* WebCompositorSupportImpl::createVideoLayer(
    WebKit::WebVideoFrameProvider* provider) {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  return new WebKit::WebVideoLayerImpl(provider);
#else
  return WebKit::WebVideoLayer::create(provider);
#endif
}

WebScrollbarLayer* WebCompositorSupportImpl::createScrollbarLayer(
      WebScrollbar* scrollbar,
      WebScrollbarThemePainter painter,
      WebScrollbarThemeGeometry* geometry) {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  return new WebKit::WebScrollbarLayerImpl(scrollbar, painter, geometry);
#else
  return WebKit::WebScrollbarLayer::create(scrollbar, painter, geometry);
#endif
}

WebAnimation* WebCompositorSupportImpl::createAnimation(
      const WebKit::WebAnimationCurve& curve,
      WebKit::WebAnimation::TargetProperty target,
      int animationId) {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  return new WebKit::WebAnimationImpl(curve, target, animationId);
#else
  return WebKit::WebAnimation::create(curve, target, animationId);
#endif
}

WebFloatAnimationCurve* WebCompositorSupportImpl::createFloatAnimationCurve() {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  return new WebKit::WebFloatAnimationCurveImpl();
#else
  return WebKit::WebFloatAnimationCurve::create();
#endif
}

WebTransformAnimationCurve*
    WebCompositorSupportImpl::createTransformAnimationCurve() {
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  return new WebKit::WebTransformAnimationCurveImpl();
#else
  return WebKit::WebTransformAnimationCurve::create();
#endif
}

}  // namespace webkit
