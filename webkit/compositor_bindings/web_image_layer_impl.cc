// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_image_layer_impl.h"

#include "base/command_line.h"
#include "cc/image_layer.h"
#include "cc/picture_image_layer.h"
#include "cc/switches.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/compositor_bindings/web_layer_impl_fixed_bounds.h"

static bool usingPictureLayer() {
  return cc::switches::IsImplSidePaintingEnabled();
}

namespace WebKit {

WebImageLayerImpl::WebImageLayerImpl() {
  if (usingPictureLayer())
    layer_.reset(new WebLayerImplFixedBounds(cc::PictureImageLayer::create()));
  else
    layer_.reset(new WebLayerImpl(cc::ImageLayer::create()));
}

WebImageLayerImpl::~WebImageLayerImpl() {}

WebLayer* WebImageLayerImpl::layer() { return layer_.get(); }

void WebImageLayerImpl::setBitmap(SkBitmap bitmap) {
  if (usingPictureLayer()) {
    static_cast<cc::PictureImageLayer*>(layer_->layer())->setBitmap(bitmap);
    static_cast<WebLayerImplFixedBounds*>(layer_.get())->SetFixedBounds(gfx::Size(bitmap.width(), bitmap.height()));
  } else
    static_cast<cc::ImageLayer*>(layer_->layer())->setBitmap(bitmap);
}

}  // namespace WebKit
