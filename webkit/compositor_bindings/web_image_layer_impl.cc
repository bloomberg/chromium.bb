// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_image_layer_impl.h"

#include "base/command_line.h"
#include "cc/base/switches.h"
#include "cc/layers/image_layer.h"
#include "cc/layers/picture_image_layer.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/compositor_bindings/web_layer_impl_fixed_bounds.h"

static bool usingPictureLayer() {
  return cc::switches::IsImplSidePaintingEnabled();
}

namespace webkit {

WebImageLayerImpl::WebImageLayerImpl() {
  if (usingPictureLayer())
    layer_.reset(new WebLayerImplFixedBounds(cc::PictureImageLayer::Create()));
  else
    layer_.reset(new WebLayerImpl(cc::ImageLayer::Create()));
}

WebImageLayerImpl::~WebImageLayerImpl() {}

WebKit::WebLayer* WebImageLayerImpl::layer() { return layer_.get(); }

void WebImageLayerImpl::setBitmap(SkBitmap bitmap) {
  if (usingPictureLayer()) {
    static_cast<cc::PictureImageLayer*>(layer_->layer())->SetBitmap(bitmap);
    static_cast<WebLayerImplFixedBounds*>(layer_.get())->SetFixedBounds(
        gfx::Size(bitmap.width(), bitmap.height()));
  } else {
    static_cast<cc::ImageLayer*>(layer_->layer())->SetBitmap(bitmap);
  }
}

}  // namespace webkit
