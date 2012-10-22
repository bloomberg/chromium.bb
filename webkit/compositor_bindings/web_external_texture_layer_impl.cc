// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web_external_texture_layer_impl.h"

#include "cc/texture_layer.h"
#include "cc/texture_update_queue.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebExternalTextureLayerClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "webcore_convert.h"
#include "web_layer_impl.h"

using namespace cc;

namespace WebKit {

WebExternalTextureLayer* WebExternalTextureLayer::create(WebExternalTextureLayerClient* client)
{
    return new WebExternalTextureLayerImpl(client);
}

WebExternalTextureLayerImpl::WebExternalTextureLayerImpl(WebExternalTextureLayerClient* client)
    : m_client(client)
{
    scoped_refptr<TextureLayer> layer;
    if (m_client)
        layer = TextureLayer::create(this);
    else
        layer = TextureLayer::create(0);
    layer->setIsDrawable(true);
    m_layer.reset(new WebLayerImpl(layer));
}

WebExternalTextureLayerImpl::~WebExternalTextureLayerImpl()
{
    static_cast<TextureLayer*>(m_layer->layer())->clearClient();
}

WebLayer* WebExternalTextureLayerImpl::layer()
{
    return m_layer.get();
}

void WebExternalTextureLayerImpl::setTextureId(unsigned id)
{
    static_cast<TextureLayer*>(m_layer->layer())->setTextureId(id);
}

void WebExternalTextureLayerImpl::setFlipped(bool flipped)
{
    static_cast<TextureLayer*>(m_layer->layer())->setFlipped(flipped);
}

void WebExternalTextureLayerImpl::setUVRect(const WebFloatRect& rect)
{
    static_cast<TextureLayer*>(m_layer->layer())->setUVRect(convert(rect));
}

void WebExternalTextureLayerImpl::setOpaque(bool opaque)
{
    static_cast<TextureLayer*>(m_layer->layer())->setContentsOpaque(opaque);
}

void WebExternalTextureLayerImpl::setPremultipliedAlpha(bool premultipliedAlpha)
{
    static_cast<TextureLayer*>(m_layer->layer())->setPremultipliedAlpha(premultipliedAlpha);
}

void WebExternalTextureLayerImpl::willModifyTexture()
{
    static_cast<TextureLayer*>(m_layer->layer())->willModifyTexture();
}

void WebExternalTextureLayerImpl::setRateLimitContext(bool rateLimit)
{
    static_cast<TextureLayer*>(m_layer->layer())->setRateLimitContext(rateLimit);
}

class WebTextureUpdaterImpl : public WebTextureUpdater {
public:
    explicit WebTextureUpdaterImpl(TextureUpdateQueue& queue)
        : m_queue(queue)
    {
    }

    virtual void appendCopy(unsigned sourceTexture, unsigned destinationTexture, WebSize size) OVERRIDE
    {
        TextureCopier::Parameters copy = { sourceTexture, destinationTexture, convert(size) };
        m_queue.appendCopy(copy);
    }

private:
    TextureUpdateQueue& m_queue;
};

unsigned WebExternalTextureLayerImpl::prepareTexture(TextureUpdateQueue& queue)
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
