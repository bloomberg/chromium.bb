// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebIOSurfaceLayerImpl.h"

#include "IOSurfaceLayerChromium.h"
#include "WebLayerImpl.h"

using WebCore::IOSurfaceLayerChromium;

namespace WebKit {

WebIOSurfaceLayer* WebIOSurfaceLayer::create()
{
    RefPtr<IOSurfaceLayerChromium> layer = IOSurfaceLayerChromium::create();
    layer->setIsDrawable(true);
    return new WebIOSurfaceLayerImpl(layer.release());
}

WebIOSurfaceLayerImpl::WebIOSurfaceLayerImpl(PassRefPtr<IOSurfaceLayerChromium> layer)
    : m_layer(adoptPtr(new WebLayerImpl(layer)))
{
}

WebIOSurfaceLayerImpl::~WebIOSurfaceLayerImpl()
{
}

void WebIOSurfaceLayerImpl::setIOSurfaceProperties(unsigned ioSurfaceId, WebSize size)
{
    static_cast<IOSurfaceLayerChromium*>(m_layer->layer())->setIOSurfaceProperties(ioSurfaceId, size);
}

WebLayer* WebIOSurfaceLayerImpl::layer()
{
    return m_layer.get();
}

} // namespace WebKit
