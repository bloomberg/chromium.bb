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
    blink::WebScrollbar::Orientation orientation) {
  return orientation == blink::WebScrollbar::Horizontal ? cc::HORIZONTAL
    : cc::VERTICAL;
}

}  // namespace

namespace webkit {

WebScrollbarLayerImpl::WebScrollbarLayerImpl(
    blink::WebScrollbar* scrollbar,
    blink::WebScrollbarThemePainter painter,
    blink::WebScrollbarThemeGeometry* geometry)
    : layer_(new WebLayerImpl(PaintedScrollbarLayer::Create(
          scoped_ptr<cc::Scrollbar>(new ScrollbarImpl(
              make_scoped_ptr(scrollbar),
              painter,
              make_scoped_ptr(geometry))).Pass(), 0))) {}

WebScrollbarLayerImpl::WebScrollbarLayerImpl(
    blink::WebScrollbar::Orientation orientation,
    int thumb_thickness,
    bool is_left_side_vertical_scrollbar)
    : layer_(new WebLayerImpl(
          SolidColorScrollbarLayer::Create(
              ConvertOrientation(orientation),
              thumb_thickness,
              is_left_side_vertical_scrollbar,
              0))) {}

WebScrollbarLayerImpl::~WebScrollbarLayerImpl() {}

blink::WebLayer* WebScrollbarLayerImpl::layer() { return layer_.get(); }

void WebScrollbarLayerImpl::setScrollLayer(blink::WebLayer* layer) {
  int id = layer ? static_cast<WebLayerImpl*>(layer)->layer()->id() : 0;
  static_cast<PaintedScrollbarLayer*>(layer_->layer())->SetScrollLayerId(id);
}

}  // namespace webkit
