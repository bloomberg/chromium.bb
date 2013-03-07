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
struct WebFloatRect;

class WebExternalTextureLayerImpl : public WebExternalTextureLayer,
                                    public cc::TextureLayerClient {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebExternalTextureLayerImpl(
      WebExternalTextureLayerClient*);
  virtual ~WebExternalTextureLayerImpl();

  // WebExternalTextureLayer implementation.
  virtual WebLayer* layer();
  virtual void setTextureId(unsigned);
  virtual void setFlipped(bool);
  virtual void setUVRect(const WebFloatRect&);
  virtual void setOpaque(bool);
  virtual void setPremultipliedAlpha(bool);
  virtual void willModifyTexture();
  virtual void setRateLimitContext(bool);

  // TextureLayerClient implementation.
  virtual unsigned prepareTexture(cc::ResourceUpdateQueue&) OVERRIDE;
  virtual WebGraphicsContext3D* context() OVERRIDE;

 private:
  WebExternalTextureLayerClient* client_;
  scoped_ptr<WebLayerImpl> layer_;
};

}

#endif  // WebExternalTextureLayerImpl_h
