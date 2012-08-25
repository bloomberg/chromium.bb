// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebContentLayerImpl.h"

#include "SkMatrix44.h"
#include <public/WebContentLayerClient.h>
#include <public/WebFloatPoint.h>
#include <public/WebFloatRect.h>
#include <public/WebRect.h>
#include <public/WebSize.h>

using namespace WebCore;

namespace WebKit {

WebContentLayer* WebContentLayer::create(WebContentLayerClient* client)
{
    return new WebContentLayerImpl(client);
}

WebContentLayerImpl::WebContentLayerImpl(WebContentLayerClient* client)
    : m_webLayerImpl(adoptPtr(new WebLayerImpl(ContentLayerChromium::create(this))))
    , m_client(client)
{
    m_webLayerImpl->layer()->setIsDrawable(true);
}

WebContentLayerImpl::~WebContentLayerImpl()
{
    static_cast<ContentLayerChromium*>(m_webLayerImpl->layer())->clearDelegate();
}

WebLayer* WebContentLayerImpl::layer()
{
    return m_webLayerImpl.get();
}

void WebContentLayerImpl::setDoubleSided(bool doubleSided)
{
    m_webLayerImpl->layer()->setDoubleSided(doubleSided);
}

void WebContentLayerImpl::setContentsScale(float scale)
{
    m_webLayerImpl->layer()->setContentsScale(scale);
}

void WebContentLayerImpl::setUseLCDText(bool enable)
{
    m_webLayerImpl->layer()->setUseLCDText(enable);
}

void WebContentLayerImpl::setDrawCheckerboardForMissingTiles(bool enable)
{
    m_webLayerImpl->layer()->setDrawCheckerboardForMissingTiles(enable);
}


void WebContentLayerImpl::paintContents(SkCanvas* canvas, const IntRect& clip, FloatRect& opaque)
{
    if (!m_client)
        return;
    WebFloatRect webOpaque;
    m_client->paintContents(canvas, WebRect(clip), webOpaque);
    opaque = webOpaque;
}

} // namespace WebKit
