// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_external_texture_layer_impl.h"

#include "cc/resource_update_queue.h"
#include "cc/texture_layer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebExternalTextureLayerClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "webkit/compositor_bindings/web_layer_impl.h"

using namespace cc;

namespace WebKit {

WebExternalTextureLayerImpl::WebExternalTextureLayerImpl(
    WebExternalTextureLayerClient* client)
    : client_(client) {
  scoped_refptr<TextureLayer> layer;
  if (client_)
    layer = TextureLayer::create(this);
  else
    layer = TextureLayer::create(0);
  layer->setIsDrawable(true);
  layer_.reset(new WebLayerImpl(layer));
}

WebExternalTextureLayerImpl::~WebExternalTextureLayerImpl() {
  static_cast<TextureLayer*>(layer_->layer())->clearClient();
}

WebLayer* WebExternalTextureLayerImpl::layer() { return layer_.get(); }

void WebExternalTextureLayerImpl::setTextureId(unsigned id) {
  static_cast<TextureLayer*>(layer_->layer())->setTextureId(id);
}

void WebExternalTextureLayerImpl::setFlipped(bool flipped) {
  static_cast<TextureLayer*>(layer_->layer())->setFlipped(flipped);
}

void WebExternalTextureLayerImpl::setUVRect(const WebFloatRect& rect) {
  static_cast<TextureLayer*>(layer_->layer())
      ->setUV(gfx::PointF(rect.x, rect.y),
              gfx::PointF(rect.x + rect.width, rect.y + rect.height));
}

void WebExternalTextureLayerImpl::setOpaque(bool opaque) {
  static_cast<TextureLayer*>(layer_->layer())->setContentsOpaque(opaque);
}

void WebExternalTextureLayerImpl::setPremultipliedAlpha(
    bool premultiplied_alpha) {
  static_cast<TextureLayer*>(layer_->layer())
      ->setPremultipliedAlpha(premultiplied_alpha);
}

void WebExternalTextureLayerImpl::willModifyTexture() {
  static_cast<TextureLayer*>(layer_->layer())->willModifyTexture();
}

void WebExternalTextureLayerImpl::setRateLimitContext(bool rate_limit) {
  static_cast<TextureLayer*>(layer_->layer())->setRateLimitContext(rate_limit);
}

class WebTextureUpdaterImpl : public WebTextureUpdater {
 public:
  explicit WebTextureUpdaterImpl(ResourceUpdateQueue& queue) : queue_(queue) {}

  virtual void appendCopy(unsigned source_texture,
                          unsigned destination_texture,
                          WebSize size) OVERRIDE {
    TextureCopier::Parameters copy = { source_texture, destination_texture,
                                       size };
    queue_.appendCopy(copy);
  }

 private:
  ResourceUpdateQueue& queue_;
};

unsigned WebExternalTextureLayerImpl::prepareTexture(
    ResourceUpdateQueue& queue) {
  DCHECK(client_);
  WebTextureUpdaterImpl updater_impl(queue);
  return client_->prepareTexture(updater_impl);
}

WebGraphicsContext3D* WebExternalTextureLayerImpl::context() {
  DCHECK(client_);
  return client_->context();
}

}  // namespace WebKit
