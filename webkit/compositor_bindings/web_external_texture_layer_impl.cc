// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_external_texture_layer_impl.h"

#include "cc/layers/texture_layer.h"
#include "cc/resources/resource_update_queue.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebExternalTextureLayerClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebExternalTextureMailbox.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "webkit/compositor_bindings/web_layer_impl.h"

using cc::TextureLayer;
using cc::ResourceUpdateQueue;

namespace webkit {

WebExternalTextureLayerImpl::WebExternalTextureLayerImpl(
    WebKit::WebExternalTextureLayerClient* client,
    bool mailbox)
    : client_(client),
      uses_mailbox_(mailbox) {
  scoped_refptr<TextureLayer> layer;
  cc::TextureLayerClient* cc_client = client_ ? this : NULL;
  if (mailbox)
    layer = TextureLayer::CreateForMailbox(cc_client);
  else
    layer = TextureLayer::Create(cc_client);
  layer->SetIsDrawable(true);
  layer_.reset(new WebLayerImpl(layer));
}

WebExternalTextureLayerImpl::~WebExternalTextureLayerImpl() {
  static_cast<TextureLayer*>(layer_->layer())->ClearClient();
}

WebKit::WebLayer* WebExternalTextureLayerImpl::layer() { return layer_.get(); }

void WebExternalTextureLayerImpl::clearTexture() {
  if (uses_mailbox_) {
    static_cast<TextureLayer*>(layer_->layer())->SetTextureMailbox(
        cc::TextureMailbox());
  } else {
    static_cast<TextureLayer*>(layer_->layer())->SetTextureId(0);
  }
}

void WebExternalTextureLayerImpl::setTextureId(unsigned id) {
  static_cast<TextureLayer*>(layer_->layer())->SetTextureId(id);
}

void WebExternalTextureLayerImpl::setFlipped(bool flipped) {
  static_cast<TextureLayer*>(layer_->layer())->SetFlipped(flipped);
}

void WebExternalTextureLayerImpl::setUVRect(const WebKit::WebFloatRect& rect) {
  static_cast<TextureLayer*>(layer_->layer())->SetUV(
      gfx::PointF(rect.x, rect.y),
      gfx::PointF(rect.x + rect.width, rect.y + rect.height));
}

void WebExternalTextureLayerImpl::setOpaque(bool opaque) {
  static_cast<TextureLayer*>(layer_->layer())->SetContentsOpaque(opaque);
}

void WebExternalTextureLayerImpl::setPremultipliedAlpha(
    bool premultiplied_alpha) {
  static_cast<TextureLayer*>(layer_->layer())->SetPremultipliedAlpha(
      premultiplied_alpha);
}

void WebExternalTextureLayerImpl::willModifyTexture() {
  static_cast<TextureLayer*>(layer_->layer())->WillModifyTexture();
}

void WebExternalTextureLayerImpl::setRateLimitContext(bool rate_limit) {
  static_cast<TextureLayer*>(layer_->layer())->SetRateLimitContext(rate_limit);
}

class WebTextureUpdaterImpl : public WebKit::WebTextureUpdater {
 public:
  explicit WebTextureUpdaterImpl(ResourceUpdateQueue* queue) : queue_(queue) {}

  virtual void appendCopy(unsigned source_texture,
                          unsigned destination_texture,
                          WebKit::WebSize size) OVERRIDE {
    cc::TextureCopier::Parameters copy = { source_texture, destination_texture,
                                           size };
    queue_->AppendCopy(copy);
  }

 private:
  ResourceUpdateQueue* queue_;
};

unsigned WebExternalTextureLayerImpl::PrepareTexture(
    ResourceUpdateQueue* queue) {
  DCHECK(client_);
  WebTextureUpdaterImpl updater_impl(queue);
  return client_->prepareTexture(updater_impl);
}

WebKit::WebGraphicsContext3D* WebExternalTextureLayerImpl::Context3d() {
  DCHECK(client_);
  return client_->context();
}

bool WebExternalTextureLayerImpl::PrepareTextureMailbox(
    cc::TextureMailbox* mailbox) {
  WebKit::WebExternalTextureMailbox client_mailbox;
  if (!client_->prepareMailbox(&client_mailbox)) {
    return false;
  }
  gpu::Mailbox name;
  name.SetName(client_mailbox.name);
  cc::TextureMailbox::ReleaseCallback callback =
      base::Bind(&WebExternalTextureLayerImpl::DidReleaseMailbox,
                 this->AsWeakPtr(),
                 client_mailbox);
  *mailbox = cc::TextureMailbox(name, callback, client_mailbox.syncPoint);
  return true;
}

void WebExternalTextureLayerImpl::DidReleaseMailbox(
    const WebKit::WebExternalTextureMailbox& mailbox,
    unsigned sync_point,
    bool lost_resource) {
  if (lost_resource)
    return;

  WebKit::WebExternalTextureMailbox available_mailbox;
  memcpy(available_mailbox.name, mailbox.name, sizeof(available_mailbox.name));
  available_mailbox.syncPoint = sync_point;
  client_->mailboxReleased(available_mailbox);
}

}  // namespace webkit
