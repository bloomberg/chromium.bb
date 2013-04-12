// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEB_EXTERNAL_TEXTURE_LAYER_IMPL_H_
#define WEBKIT_COMPOSITOR_BINDINGS_WEB_EXTERNAL_TEXTURE_LAYER_IMPL_H_

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/resources/texture_mailbox.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebExternalTextureLayer.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace WebKit {
struct WebFloatRect;
struct WebExternalTextureMailbox;
}

namespace webkit {

class WebLayerImpl;

class WebExternalTextureLayerImpl
    : public WebKit::WebExternalTextureLayer,
      public cc::TextureLayerClient,
      public base::SupportsWeakPtr<WebExternalTextureLayerImpl> {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebExternalTextureLayerImpl(
      WebKit::WebExternalTextureLayerClient*,
      bool mailbox);
  virtual ~WebExternalTextureLayerImpl();

  // WebKit::WebExternalTextureLayer implementation.
  virtual WebKit::WebLayer* layer();
  virtual void clearTexture();
  virtual void setTextureId(unsigned texture_id);
  virtual void setFlipped(bool flipped);
  virtual void setUVRect(const WebKit::WebFloatRect& uv_rect);
  virtual void setOpaque(bool opaque);
  virtual void setPremultipliedAlpha(bool premultiplied);

  virtual void willModifyTexture();
  virtual void setRateLimitContext(bool rate_limit);

  // TextureLayerClient implementation.
  virtual unsigned PrepareTexture(cc::ResourceUpdateQueue*) OVERRIDE;
  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE;
  virtual bool PrepareTextureMailbox(cc::TextureMailbox* mailbox) OVERRIDE;

 private:
  void DidReleaseMailbox(const WebKit::WebExternalTextureMailbox& mailbox,
                         unsigned sync_point,
                         bool lost_resource);

  WebKit::WebExternalTextureLayerClient* client_;
  scoped_ptr<WebLayerImpl> layer_;
  bool uses_mailbox_;

  DISALLOW_COPY_AND_ASSIGN(WebExternalTextureLayerImpl);
};

}  // namespace webkit

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEB_EXTERNAL_TEXTURE_LAYER_IMPL_H_
