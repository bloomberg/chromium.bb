// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebExternalTextureLayerImpl_h
#define WebExternalTextureLayerImpl_h

#include "TextureLayerChromiumClient.h"
#include <public/WebExternalTextureLayer.h>
#include <wtf/OwnPtr.h>

namespace WebKit {

class WebLayerImpl;

class WebExternalTextureLayerImpl : public WebExternalTextureLayer,
                                    public cc::TextureLayerChromiumClient {
public:
    explicit WebExternalTextureLayerImpl(WebExternalTextureLayerClient*);
    virtual ~WebExternalTextureLayerImpl();

    // WebExternalTextureLayer implementation.
    virtual WebLayer* layer() OVERRIDE;
    virtual void setTextureId(unsigned) OVERRIDE;
    virtual void setFlipped(bool) OVERRIDE;
    virtual void setUVRect(const WebFloatRect&) OVERRIDE;
    virtual void setOpaque(bool) OVERRIDE;
    virtual void setPremultipliedAlpha(bool) OVERRIDE;
    virtual void willModifyTexture() OVERRIDE;
    virtual void setRateLimitContext(bool) OVERRIDE;

    // TextureLayerChromiumClient implementation.
    virtual unsigned prepareTexture(cc::CCTextureUpdateQueue&) OVERRIDE;
    virtual WebGraphicsContext3D* context() OVERRIDE;

private:
    WebExternalTextureLayerClient* m_client;
    OwnPtr<WebLayerImpl> m_layer;
};

}

#endif // WebExternalTextureLayerImpl_h
