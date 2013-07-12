// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/compositor_bindings/web_external_texture_layer_impl.h"

#include "cc/layers/texture_layer.h"
#include "cc/resources/resource_update_queue.h"
#include "third_party/WebKit/public/platform/WebExternalTextureLayerClient.h"
#include "third_party/WebKit/public/platform/WebExternalTextureMailbox.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "webkit/renderer/compositor_bindings/web_external_bitmap_impl.h"
#include "webkit/renderer/compositor_bindings/web_layer_impl.h"

using cc::TextureLayer;
using cc::ResourceUpdateQueue;

namespace webkit {

WebExternalTextureLayerImpl::WebExternalTextureLayerImpl(
    WebKit::WebExternalTextureLayerClient* client)
    : client_(client) {
  cc::TextureLayerClient* cc_client = client_ ? this : NULL;
  scoped_refptr<TextureLayer> layer = TextureLayer::CreateForMailbox(cc_client);
  layer->SetIsDrawable(true);
  layer_.reset(new WebLayerImpl(layer));
}

WebExternalTextureLayerImpl::~WebExternalTextureLayerImpl() {
  static_cast<TextureLayer*>(layer_->layer())->ClearClient();
}

WebKit::WebLayer* WebExternalTextureLayerImpl::layer() { return layer_.get(); }

void WebExternalTextureLayerImpl::clearTexture() {
  TextureLayer *layer = static_cast<TextureLayer*>(layer_->layer());
  layer->WillModifyTexture();
  layer->SetTextureMailbox(cc::TextureMailbox());
}

void WebExternalTextureLayerImpl::setOpaque(bool opaque) {
  static_cast<TextureLayer*>(layer_->layer())->SetContentsOpaque(opaque);
}

void WebExternalTextureLayerImpl::setPremultipliedAlpha(
    bool premultiplied_alpha) {
  static_cast<TextureLayer*>(layer_->layer())->SetPremultipliedAlpha(
      premultiplied_alpha);
}

void WebExternalTextureLayerImpl::setBlendBackgroundColor(bool blend) {
  static_cast<TextureLayer*>(layer_->layer())->SetBlendBackgroundColor(blend);
}

void WebExternalTextureLayerImpl::setRateLimitContext(bool rate_limit) {
  static_cast<TextureLayer*>(layer_->layer())->SetRateLimitContext(rate_limit);
}

unsigned WebExternalTextureLayerImpl::PrepareTexture() {
  NOTREACHED();
  return 0;
}

WebKit::WebGraphicsContext3D* WebExternalTextureLayerImpl::Context3d() {
  DCHECK(client_);
  return client_->context();
}

bool WebExternalTextureLayerImpl::PrepareTextureMailbox(
    cc::TextureMailbox* mailbox,
    bool use_shared_memory) {
  WebKit::WebExternalTextureMailbox client_mailbox;
  WebExternalBitmapImpl* bitmap = NULL;

  if (use_shared_memory)
    bitmap = AllocateBitmap();
  if (!client_->prepareMailbox(&client_mailbox, bitmap)) {
    if (bitmap)
      free_bitmaps_.push_back(bitmap);
    return false;
  }
  gpu::Mailbox name;
  name.SetName(client_mailbox.name);
  cc::TextureMailbox::ReleaseCallback callback =
      base::Bind(&WebExternalTextureLayerImpl::DidReleaseMailbox,
                 this->AsWeakPtr(),
                 client_mailbox,
                 bitmap);
  if (bitmap) {
    *mailbox =
        cc::TextureMailbox(bitmap->shared_memory(), bitmap->size(), callback);
  } else {
    *mailbox = cc::TextureMailbox(name, callback, client_mailbox.syncPoint);
  }
  return true;
}

WebExternalBitmapImpl* WebExternalTextureLayerImpl::AllocateBitmap() {
  if (!free_bitmaps_.empty()) {
    WebExternalBitmapImpl* result = free_bitmaps_.back();
    free_bitmaps_.weak_erase(free_bitmaps_.end() - 1);
    return result;
  }
  return new WebExternalBitmapImpl;
}

// static
void WebExternalTextureLayerImpl::DidReleaseMailbox(
    base::WeakPtr<WebExternalTextureLayerImpl> layer,
    const WebKit::WebExternalTextureMailbox& mailbox,
    WebExternalBitmapImpl* bitmap,
    unsigned sync_point,
    bool lost_resource) {
  if (lost_resource || !layer) {
    delete bitmap;
    return;
  }

  WebKit::WebExternalTextureMailbox available_mailbox;
  memcpy(available_mailbox.name, mailbox.name, sizeof(available_mailbox.name));
  available_mailbox.syncPoint = sync_point;
  if (bitmap)
    layer->free_bitmaps_.push_back(bitmap);
  layer->client_->mailboxReleased(available_mailbox);
}

}  // namespace webkit
