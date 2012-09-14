// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebExternalTextureLayerImpl.h"

#include "CCTextureUpdateQueue.h"
#include "TextureLayerChromium.h"
#include "WebLayerImpl.h"
#include "webcore_convert.h"
#include <public/WebExternalTextureLayerClient.h>
#include <public/WebFloatRect.h>
#include <public/WebSize.h>

using namespace cc;

namespace WebKit {

WebExternalTextureLayer* WebExternalTextureLayer::create(WebExternalTextureLayerClient* client)
{
    return new WebExternalTextureLayerImpl(client);
}

WebExternalTextureLayerImpl::WebExternalTextureLayerImpl(WebExternalTextureLayerClient* client)
    : m_client(client)
{
    RefPtr<TextureLayerChromium> layer;
    if (m_client)
        layer = TextureLayerChromium::create(this);
    else
        layer = TextureLayerChromium::create(0);
    layer->setIsDrawable(true);
    m_layer = adoptPtr(new WebLayerImpl(layer.release()));
}

WebExternalTextureLayerImpl::~WebExternalTextureLayerImpl()
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->clearClient();
}

WebLayer* WebExternalTextureLayerImpl::layer()
{
    return m_layer.get();
}

void WebExternalTextureLayerImpl::setTextureId(unsigned id)
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->setTextureId(id);
}

void WebExternalTextureLayerImpl::setFlipped(bool flipped)
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->setFlipped(flipped);
}

void WebExternalTextureLayerImpl::setUVRect(const WebFloatRect& rect)
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->setUVRect(convert(rect));
}

void WebExternalTextureLayerImpl::setOpaque(bool opaque)
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->setOpaque(opaque);
}

void WebExternalTextureLayerImpl::setPremultipliedAlpha(bool premultipliedAlpha)
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->setPremultipliedAlpha(premultipliedAlpha);
}

void WebExternalTextureLayerImpl::willModifyTexture()
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->willModifyTexture();
}

void WebExternalTextureLayerImpl::setRateLimitContext(bool rateLimit)
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->setRateLimitContext(rateLimit);
}

class WebTextureUpdaterImpl : public WebTextureUpdater {
public:
    explicit WebTextureUpdaterImpl(CCTextureUpdateQueue& queue)
        : m_queue(queue)
    {
    }

    virtual void appendCopy(unsigned sourceTexture, unsigned destinationTexture, WebSize size) OVERRIDE
    {
        TextureCopier::Parameters copy = { sourceTexture, destinationTexture, convert(size) };
        m_queue.appendCopy(copy);
    }

private:
    CCTextureUpdateQueue& m_queue;
};

unsigned WebExternalTextureLayerImpl::prepareTexture(CCTextureUpdateQueue& queue)
{
    ASSERT(m_client);
    WebTextureUpdaterImpl updaterImpl(queue);
    return m_client->prepareTexture(updaterImpl);
}

WebGraphicsContext3D* WebExternalTextureLayerImpl::context()
{
    ASSERT(m_client);
    return m_client->context();
}

} // namespace WebKit
