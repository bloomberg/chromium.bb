// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_EXTERNAL_TEXTURE_LAYER_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_EXTERNAL_TEXTURE_LAYER_IMPL_H_

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/resources/texture_mailbox.h"
#include "third_party/WebKit/public/platform/WebExternalTextureLayer.h"
#include "webkit/renderer/compositor_bindings/webkit_compositor_bindings_export.h"

namespace WebKit {
struct WebFloatRect;
struct WebExternalTextureMailbox;
}

namespace webkit {

class WebLayerImpl;
class WebExternalBitmapImpl;

class WebExternalTextureLayerImpl
    : public WebKit::WebExternalTextureLayer,
      public cc::TextureLayerClient,
      public base::SupportsWeakPtr<WebExternalTextureLayerImpl> {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebExternalTextureLayerImpl(
      WebKit::WebExternalTextureLayerClient*);
  virtual ~WebExternalTextureLayerImpl();

  // WebKit::WebExternalTextureLayer implementation.
  virtual WebKit::WebLayer* layer();
  virtual void clearTexture();
  virtual void setOpaque(bool opaque);
  virtual void setPremultipliedAlpha(bool premultiplied);
  virtual void setBlendBackgroundColor(bool blend);
  virtual void setRateLimitContext(bool rate_limit);

  // TextureLayerClient implementation.
  virtual unsigned PrepareTexture() OVERRIDE;
  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE;
  virtual bool PrepareTextureMailbox(cc::TextureMailbox* mailbox,
                                     bool use_shared_memory) OVERRIDE;

 private:
  static void DidReleaseMailbox(
      base::WeakPtr<WebExternalTextureLayerImpl> layer,
      const WebKit::WebExternalTextureMailbox& mailbox,
      WebExternalBitmapImpl* bitmap,
      unsigned sync_point,
      bool lost_resource);

  WebExternalBitmapImpl* AllocateBitmap();

  WebKit::WebExternalTextureLayerClient* client_;
  scoped_ptr<WebLayerImpl> layer_;
  ScopedVector<WebExternalBitmapImpl> free_bitmaps_;

  DISALLOW_COPY_AND_ASSIGN(WebExternalTextureLayerImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_EXTERNAL_TEXTURE_LAYER_IMPL_H_
