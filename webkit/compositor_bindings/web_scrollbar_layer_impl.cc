// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_scrollbar_layer_impl.h"

#include "cc/scrollbar_layer.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/compositor_bindings/web_to_ccscrollbar_theme_painter_adapter.h"

using cc::ScrollbarLayer;
using cc::ScrollbarThemePainter;

namespace WebKit {

WebScrollbarLayerImpl::WebScrollbarLayerImpl(
    WebScrollbar* scrollbar,
    WebScrollbarThemePainter painter,
    WebScrollbarThemeGeometry* geometry)
    : layer_(new WebLayerImpl(ScrollbarLayer::create(
          make_scoped_ptr(scrollbar),
          WebToCCScrollbarThemePainterAdapter::Create(
              make_scoped_ptr(new WebScrollbarThemePainter(painter)))
              .PassAs<ScrollbarThemePainter>(),
          make_scoped_ptr(geometry),
          0))) {}

WebScrollbarLayerImpl::~WebScrollbarLayerImpl() {}

WebLayer* WebScrollbarLayerImpl::layer() { return layer_.get(); }

void WebScrollbarLayerImpl::setScrollLayer(WebLayer* layer) {
  int id = layer ? static_cast<WebLayerImpl*>(layer)->layer()->id() : 0;
  static_cast<ScrollbarLayer*>(layer_->layer())->setScrollLayerId(id);
}

}  // namespace WebKit
