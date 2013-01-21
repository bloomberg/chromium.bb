// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_compositor_support_impl.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "cc/thread_impl.h"
#include "cc/transform_operations.h"
#include "webkit/compositor_bindings/web_animation_impl.h"
#include "webkit/compositor_bindings/web_compositor_support_output_surface.h"
#include "webkit/compositor_bindings/web_compositor_support_software_output_device.h"
#include "webkit/compositor_bindings/web_content_layer_impl.h"
#include "webkit/compositor_bindings/web_external_texture_layer_impl.h"
#include "webkit/compositor_bindings/web_float_animation_curve_impl.h"
#include "webkit/compositor_bindings/web_image_layer_impl.h"
#include "webkit/compositor_bindings/web_io_surface_layer_impl.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/compositor_bindings/web_layer_tree_view_impl.h"
#include "webkit/compositor_bindings/web_scrollbar_layer_impl.h"
#include "webkit/compositor_bindings/web_solid_color_layer_impl.h"
#include "webkit/compositor_bindings/web_transform_animation_curve_impl.h"
#include "webkit/compositor_bindings/web_transform_operations_impl.h"
#include "webkit/compositor_bindings/web_video_layer_impl.h"
#include "webkit/glue/webthread_impl.h"
#include "webkit/support/webkit_support.h"

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
using WebKit::WebTransformOperations;
using WebKit::WebVideoFrameProvider;
using WebKit::WebVideoLayer;

namespace webkit {

WebCompositorSupportImpl::WebCompositorSupportImpl()
    : initialized_(false) {
}

WebCompositorSupportImpl::~WebCompositorSupportImpl() {
}

void WebCompositorSupportImpl::initialize(WebKit::WebThread* impl_thread) {
  DCHECK(!initialized_);
  initialized_ = true;
  if (impl_thread) {
    impl_thread_message_loop_proxy_ =
        static_cast<webkit_glue::WebThreadImpl*>(impl_thread)->
            message_loop()->message_loop_proxy();
  } else {
    impl_thread_message_loop_proxy_ = NULL;
  }
}

bool WebCompositorSupportImpl::isThreadingEnabled() {
  return impl_thread_message_loop_proxy_;
}

void WebCompositorSupportImpl::shutdown() {
  DCHECK(initialized_);
  initialized_ = false;
  impl_thread_message_loop_proxy_ = NULL;
}

WebLayerTreeView* WebCompositorSupportImpl::createLayerTreeView(
    WebLayerTreeViewClient* client, const WebLayer& root,
    const WebLayerTreeView::Settings& settings) {
  DCHECK(initialized_);
  scoped_ptr<WebKit::WebLayerTreeViewImpl> layerTreeViewImpl(
      new WebKit::WebLayerTreeViewImpl(client));
  scoped_ptr<cc::Thread> impl_thread;
  if (impl_thread_message_loop_proxy_)
    impl_thread = cc::ThreadImpl::createForDifferentThread(
        impl_thread_message_loop_proxy_);
  if (!layerTreeViewImpl->initialize(settings, impl_thread.Pass()))
    return NULL;
  layerTreeViewImpl->setRootLayer(root);
  return layerTreeViewImpl.release();
}

WebKit::WebCompositorOutputSurface*
    WebCompositorSupportImpl::createOutputSurfaceFor3D(
        WebKit::WebGraphicsContext3D* context) {
  scoped_ptr<WebKit::WebGraphicsContext3D> context3d = make_scoped_ptr(context);
  return WebCompositorSupportOutputSurface::Create3d(
      context3d.Pass()).release();
}

WebKit::WebCompositorOutputSurface*
    WebCompositorSupportImpl::createOutputSurfaceForSoftware() {
  scoped_ptr<WebCompositorSupportSoftwareOutputDevice> software_device =
      make_scoped_ptr(new WebCompositorSupportSoftwareOutputDevice);
  return WebCompositorSupportOutputSurface::CreateSoftware(
      software_device.PassAs<cc::SoftwareOutputDevice>()).release();
}

WebLayer* WebCompositorSupportImpl::createLayer() {
  return new WebKit::WebLayerImpl();
}

WebContentLayer* WebCompositorSupportImpl::createContentLayer(
    WebContentLayerClient* client) {
  return new WebKit::WebContentLayerImpl(client);
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

WebTransformOperations* WebCompositorSupportImpl::createTransformOperations() {
  return new WebTransformOperationsImpl();
}

}  // namespace webkit
