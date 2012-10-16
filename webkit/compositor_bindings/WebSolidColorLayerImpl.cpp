// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebSolidColorLayerImpl.h"

#include "WebLayerImpl.h"
#include "cc/solid_color_layer.h"

using cc::SolidColorLayerChromium;

namespace WebKit {

WebSolidColorLayer* WebSolidColorLayer::create()
{
    return new WebSolidColorLayerImpl();
}

WebSolidColorLayerImpl::WebSolidColorLayerImpl()
    : m_layer(new WebLayerImpl(SolidColorLayerChromium::create()))
{
    m_layer->layer()->setIsDrawable(true);
}

WebSolidColorLayerImpl::~WebSolidColorLayerImpl()
{
}

WebLayer* WebSolidColorLayerImpl::layer()
{
    return m_layer.get();
}

void WebSolidColorLayerImpl::setBackgroundColor(WebColor color)
{
    m_layer->setBackgroundColor(color);
}

} // namespace WebKit
