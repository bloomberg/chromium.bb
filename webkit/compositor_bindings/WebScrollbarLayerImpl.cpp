// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebScrollbarLayerImpl.h"

#include "ScrollbarLayerChromium.h"
#include "WebLayerImpl.h"

using cc::ScrollbarLayerChromium;

namespace WebKit {

WebScrollbarLayer* WebScrollbarLayer::create(WebScrollbar* scrollbar, WebScrollbarThemePainter painter, WebScrollbarThemeGeometry* geometry)
{
    return new WebScrollbarLayerImpl(scrollbar, painter, geometry);
}


WebScrollbarLayerImpl::WebScrollbarLayerImpl(WebScrollbar* scrollbar, WebScrollbarThemePainter painter, WebScrollbarThemeGeometry* geometry)
    : m_layer(adoptPtr(new WebLayerImpl(ScrollbarLayerChromium::create(adoptPtr(scrollbar), painter, adoptPtr(geometry), 0))))
{
}

WebScrollbarLayerImpl::~WebScrollbarLayerImpl()
{
}

WebLayer* WebScrollbarLayerImpl::layer()
{
    return m_layer.get();
}

void WebScrollbarLayerImpl::setScrollLayer(WebLayer* layer)
{
    int id = layer ? static_cast<WebLayerImpl*>(layer)->layer()->id() : 0;
    static_cast<ScrollbarLayerChromium*>(m_layer->layer())->setScrollLayerId(id);
}



} // namespace WebKit
