// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_scrollbar_layer_impl.h"

#include "cc/layers/scrollbar_layer.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/compositor_bindings/web_to_ccscrollbar_theme_painter_adapter.h"

using cc::ScrollbarLayer;
using cc::ScrollbarThemePainter;
using WebKit::WebScrollbarThemePainter;

namespace webkit {

WebScrollbarLayerImpl::WebScrollbarLayerImpl(
    WebKit::WebScrollbar* scrollbar,
    WebKit::WebScrollbarThemePainter painter,
    WebKit::WebScrollbarThemeGeometry* geometry)
    : layer_(new WebLayerImpl(ScrollbarLayer::Create(
          make_scoped_ptr(scrollbar),
          WebToCCScrollbarThemePainterAdapter::Create(
              make_scoped_ptr(new WebScrollbarThemePainter(painter)))
              .PassAs<ScrollbarThemePainter>(),
          make_scoped_ptr(geometry),
          0))) {}

WebScrollbarLayerImpl::~WebScrollbarLayerImpl() {}

WebKit::WebLayer* WebScrollbarLayerImpl::layer() { return layer_.get(); }

void WebScrollbarLayerImpl::setScrollLayer(WebKit::WebLayer* layer) {
  int id = layer ? static_cast<WebLayerImpl*>(layer)->layer()->id() : 0;
  static_cast<ScrollbarLayer*>(layer_->layer())->SetScrollLayerId(id);
}

}  // namespace webkit
