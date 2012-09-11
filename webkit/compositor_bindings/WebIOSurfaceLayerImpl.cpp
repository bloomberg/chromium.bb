// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebIOSurfaceLayerImpl.h"

#include "IOSurfaceLayerChromium.h"
#include "WebLayerImpl.h"
#include "webcore_convert.h"

using WebCore::IOSurfaceLayerChromium;

namespace WebKit {

WebIOSurfaceLayer* WebIOSurfaceLayer::create()
{
    return new WebIOSurfaceLayerImpl();
}

WebIOSurfaceLayerImpl::WebIOSurfaceLayerImpl()
    : m_layer(adoptPtr(new WebLayerImpl(IOSurfaceLayerChromium::create())))
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
