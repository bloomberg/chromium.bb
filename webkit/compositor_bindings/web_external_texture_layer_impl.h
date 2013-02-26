// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebExternalTextureLayerImpl_h
#define WebExternalTextureLayerImpl_h

#include "base/memory/scoped_ptr.h"
#include "cc/texture_layer_client.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebExternalTextureLayer.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace WebKit {

class WebLayerImpl;

class WebExternalTextureLayerImpl : public WebExternalTextureLayer,
                                    public cc::TextureLayerClient {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebExternalTextureLayerImpl(
      WebExternalTextureLayerClient*);
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

  // TextureLayerClient implementation.
  virtual unsigned prepareTexture(cc::ResourceUpdateQueue&) OVERRIDE;
  virtual WebGraphicsContext3D* context() OVERRIDE;

 private:
  WebExternalTextureLayerClient* client_;
  scoped_ptr<WebLayerImpl> layer_;
};

}

#endif  // WebExternalTextureLayerImpl_h
