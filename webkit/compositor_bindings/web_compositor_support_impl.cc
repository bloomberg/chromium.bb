// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_compositor_support_impl.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "cc/animation/transform_operations.h"
#include "cc/base/thread_impl.h"
#include "cc/output/output_surface.h"
#include "cc/output/software_output_device.h"
#include "webkit/compositor_bindings/web_animation_impl.h"
#include "webkit/compositor_bindings/web_content_layer_impl.h"
#include "webkit/compositor_bindings/web_external_texture_layer_impl.h"
#include "webkit/compositor_bindings/web_float_animation_curve_impl.h"
#include "webkit/compositor_bindings/web_image_layer_impl.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/compositor_bindings/web_scrollbar_layer_impl.h"
#include "webkit/compositor_bindings/web_solid_color_layer_impl.h"
#include "webkit/compositor_bindings/web_transform_animation_curve_impl.h"
#include "webkit/compositor_bindings/web_transform_operations_impl.h"
#include "webkit/glue/webthread_impl.h"
#include "webkit/support/webkit_support.h"

using WebKit::WebAnimation;
using WebKit::WebAnimationCurve;
using WebKit::WebContentLayer;
using WebKit::WebContentLayerClient;
using WebKit::WebExternalTextureLayer;
using WebKit::WebExternalTextureLayerClient;
using WebKit::WebFloatAnimationCurve;
using WebKit::WebImageLayer;
using WebKit::WebLayer;
using WebKit::WebScrollbar;
using WebKit::WebScrollbarLayer;
using WebKit::WebScrollbarThemeGeometry;
using WebKit::WebScrollbarThemePainter;
using WebKit::WebSolidColorLayer;
using WebKit::WebTransformAnimationCurve;
using WebKit::WebTransformOperations;

namespace webkit {

WebCompositorSupportImpl::WebCompositorSupportImpl() {}

WebCompositorSupportImpl::~WebCompositorSupportImpl() {}

WebLayer* WebCompositorSupportImpl::createLayer() {
  return new WebLayerImpl();
}

WebContentLayer* WebCompositorSupportImpl::createContentLayer(
    WebContentLayerClient* client) {
  return new WebContentLayerImpl(client);
}

WebExternalTextureLayer* WebCompositorSupportImpl::createExternalTextureLayer(
    WebExternalTextureLayerClient* client) {
  return new WebExternalTextureLayerImpl(client);
}

WebKit::WebImageLayer* WebCompositorSupportImpl::createImageLayer() {
  return new WebImageLayerImpl();
}

WebSolidColorLayer* WebCompositorSupportImpl::createSolidColorLayer() {
  return new WebSolidColorLayerImpl();
}

WebScrollbarLayer* WebCompositorSupportImpl::createScrollbarLayer(
    WebScrollbar* scrollbar,
    WebScrollbarThemePainter painter,
    WebScrollbarThemeGeometry* geometry) {
  return new WebScrollbarLayerImpl(scrollbar, painter, geometry);
}

WebAnimation* WebCompositorSupportImpl::createAnimation(
    const WebKit::WebAnimationCurve& curve,
    WebKit::WebAnimation::TargetProperty target,
    int animation_id) {
  return new WebAnimationImpl(curve, target, animation_id, 0);
}

WebFloatAnimationCurve* WebCompositorSupportImpl::createFloatAnimationCurve() {
  return new WebFloatAnimationCurveImpl();
}

WebTransformAnimationCurve*
WebCompositorSupportImpl::createTransformAnimationCurve() {
  return new WebTransformAnimationCurveImpl();
}

WebTransformOperations* WebCompositorSupportImpl::createTransformOperations() {
  return new WebTransformOperationsImpl();
}

}  // namespace webkit
