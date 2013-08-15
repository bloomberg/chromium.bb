// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/compositor_bindings/web_nine_patch_layer_impl.h"

#include "base/command_line.h"
#include "cc/base/switches.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/picture_image_layer.h"
#include "webkit/renderer/compositor_bindings/web_layer_impl.h"
#include "webkit/renderer/compositor_bindings/web_layer_impl_fixed_bounds.h"

namespace webkit {

WebNinePatchLayerImpl::WebNinePatchLayerImpl() {
  layer_.reset(new WebLayerImpl(cc::NinePatchLayer::Create()));
}

WebNinePatchLayerImpl::~WebNinePatchLayerImpl() {}

WebKit::WebLayer* WebNinePatchLayerImpl::layer() { return layer_.get(); }

void WebNinePatchLayerImpl::setBitmap(SkBitmap bitmap,
                                      const WebKit::WebRect& aperture) {
  static_cast<cc::NinePatchLayer*>(layer_->layer())->SetBitmap(
      bitmap, gfx::Rect(aperture));
}

}  // namespace webkit
