// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web_content_layer_impl.h"

#include "SkMatrix44.h"
#include "cc/content_layer.h"
#include "cc/stubs/float_rect.h"
#include "cc/stubs/int_rect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebContentLayerClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "webcore_convert.h"

using namespace cc;

namespace WebKit {

WebContentLayer* WebContentLayer::create(WebContentLayerClient* client)
{
    return new WebContentLayerImpl(client);
}

WebContentLayerImpl::WebContentLayerImpl(WebContentLayerClient* client)
    : m_layer(new WebLayerImpl(ContentLayerChromium::create(this)))
    , m_client(client)
{
    m_layer->layer()->setIsDrawable(true);
}

WebContentLayerImpl::~WebContentLayerImpl()
{
    static_cast<ContentLayerChromium*>(m_layer->layer())->clearClient();
}

WebLayer* WebContentLayerImpl::layer()
{
    return m_layer.get();
}

void WebContentLayerImpl::setDoubleSided(bool doubleSided)
{
    m_layer->layer()->setDoubleSided(doubleSided);
}

void WebContentLayerImpl::setBoundsContainPageScale(bool boundsContainPageScale)
{
    return m_layer->layer()->setBoundsContainPageScale(boundsContainPageScale);
}

bool WebContentLayerImpl::boundsContainPageScale() const
{
    return m_layer->layer()->boundsContainPageScale();
}

void WebContentLayerImpl::setUseLCDText(bool enable)
{
    m_layer->layer()->setUseLCDText(enable);
}

void WebContentLayerImpl::setDrawCheckerboardForMissingTiles(bool enable)
{
    m_layer->layer()->setDrawCheckerboardForMissingTiles(enable);
}


void WebContentLayerImpl::paintContents(SkCanvas* canvas, const IntRect& clip, FloatRect& opaque)
{
    if (!m_client)
        return;
    WebFloatRect webOpaque;
    m_client->paintContents(canvas, convert(clip), webOpaque);
    opaque = convert(webOpaque);
}

} // namespace WebKit
