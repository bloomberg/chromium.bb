// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_io_surface_layer_impl.h"

#include "cc/io_surface_layer.h"
#include "webkit/compositor_bindings/web_layer_impl.h"

using cc::IOSurfaceLayer;

namespace WebKit {

WebIOSurfaceLayerImpl::WebIOSurfaceLayerImpl()
    : layer_(new WebLayerImpl(IOSurfaceLayer::create())) {
  layer_->layer()->setIsDrawable(true);
}

WebIOSurfaceLayerImpl::~WebIOSurfaceLayerImpl() {}

void WebIOSurfaceLayerImpl::setIOSurfaceProperties(unsigned io_surface_id,
                                                   WebSize size) {
  static_cast<IOSurfaceLayer*>(layer_->layer())
      ->setIOSurfaceProperties(io_surface_id, size);
}

WebLayer* WebIOSurfaceLayerImpl::layer() { return layer_.get(); }

}  // namespace WebKit
