// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_io_surface_layer_impl.h"

#include "cc/io_surface_layer.h"
#include "webkit/compositor_bindings/web_layer_impl.h"

using cc::IOSurfaceLayer;

namespace WebKit {

WebIOSurfaceLayerImpl::WebIOSurfaceLayerImpl()
    : m_layer(new WebLayerImpl(IOSurfaceLayer::create()))
{
    m_layer->layer()->setIsDrawable(true);
}

WebIOSurfaceLayerImpl::~WebIOSurfaceLayerImpl()
{
}

void WebIOSurfaceLayerImpl::setIOSurfaceProperties(unsigned ioSurfaceId, WebSize size)
{
    static_cast<IOSurfaceLayer*>(m_layer->layer())->setIOSurfaceProperties(ioSurfaceId, size);
}

WebLayer* WebIOSurfaceLayerImpl::layer()
{
    return m_layer.get();
}

} // namespace WebKit
