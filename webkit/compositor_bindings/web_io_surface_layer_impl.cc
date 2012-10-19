// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web_io_surface_layer_impl.h"

#include "cc/io_surface_layer.h"
#include "webcore_convert.h"
#include "web_layer_impl.h"

using cc::IOSurfaceLayerChromium;

namespace WebKit {

WebIOSurfaceLayer* WebIOSurfaceLayer::create()
{
    return new WebIOSurfaceLayerImpl();
}

WebIOSurfaceLayerImpl::WebIOSurfaceLayerImpl()
    : m_layer(new WebLayerImpl(IOSurfaceLayerChromium::create()))
{
    m_layer->layer()->setIsDrawable(true);
}

WebIOSurfaceLayerImpl::~WebIOSurfaceLayerImpl()
{
}

void WebIOSurfaceLayerImpl::setIOSurfaceProperties(unsigned ioSurfaceId, WebSize size)
{
    static_cast<IOSurfaceLayerChromium*>(m_layer->layer())->setIOSurfaceProperties(ioSurfaceId, convert(size));
}

WebLayer* WebIOSurfaceLayerImpl::layer()
{
    return m_layer.get();
}

} // namespace WebKit
