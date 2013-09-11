// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/compositor_bindings/web_scrollbar_layer_impl.h"

#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/scrollbar_layer_interface.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "webkit/renderer/compositor_bindings/scrollbar_impl.h"
#include "webkit/renderer/compositor_bindings/web_layer_impl.h"

using cc::PaintedScrollbarLayer;
using cc::SolidColorScrollbarLayer;

namespace {

cc::ScrollbarOrientation ConvertOrientation(
    WebKit::WebScrollbar::Orientation orientation) {
  return orientation == WebKit::WebScrollbar::Horizontal ? cc::HORIZONTAL
    : cc::VERTICAL;
}

}  // namespace

namespace webkit {

WebScrollbarLayerImpl::WebScrollbarLayerImpl(
    WebKit::WebScrollbar* scrollbar,
    WebKit::WebScrollbarThemePainter painter,
    WebKit::WebScrollbarThemeGeometry* geometry)
    : layer_(new WebLayerImpl(PaintedScrollbarLayer::Create(
          scoped_ptr<cc::Scrollbar>(new ScrollbarImpl(
              make_scoped_ptr(scrollbar),
              painter,
              make_scoped_ptr(geometry))).Pass(), 0))) {}

WebScrollbarLayerImpl::WebScrollbarLayerImpl(
    WebKit::WebScrollbar::Orientation orientation,
    int thumb_thickness,
    bool is_left_side_vertical_scrollbar)
    : layer_(new WebLayerImpl(
          SolidColorScrollbarLayer::Create(
              ConvertOrientation(orientation),
              thumb_thickness,
              is_left_side_vertical_scrollbar,
              0))) {}

WebScrollbarLayerImpl::~WebScrollbarLayerImpl() {}

WebKit::WebLayer* WebScrollbarLayerImpl::layer() { return layer_.get(); }

void WebScrollbarLayerImpl::setScrollLayer(WebKit::WebLayer* layer) {
  int id = layer ? static_cast<WebLayerImpl*>(layer)->layer()->id() : 0;
  static_cast<PaintedScrollbarLayer*>(layer_->layer())->SetScrollLayerId(id);
}

}  // namespace webkit
