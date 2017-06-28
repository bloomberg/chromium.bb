// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/gpu/ImageLayerBridge.h"

#include "cc/resources/shared_bitmap.h"
#include "cc/resources/texture_mailbox.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/graphics/ColorBehavior.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebExternalTextureLayer.h"
#include "public/platform/WebGraphicsContext3DProvider.h"

namespace blink {

ImageLayerBridge::ImageLayerBridge(OpacityMode opacity_mode)
    : opacity_mode_(opacity_mode) {
  layer_ = Platform::Current()->CompositorSupport()->CreateExternalTextureLayer(
      this);
  GraphicsLayer::RegisterContentsLayer(layer_->Layer());
  layer_->SetNearestNeighbor(filter_quality_ == kNone_SkFilterQuality);
  if (opacity_mode_ == kOpaque) {
    layer_->SetOpaque(true);
    layer_->SetBlendBackgroundColor(false);
  }
}

ImageLayerBridge::~ImageLayerBridge() {
  if (!disposed_)
    Dispose();
}

void ImageLayerBridge::SetImage(PassRefPtr<StaticBitmapImage> image) {
  image_ = std::move(image);
  if (image_) {
    if (opacity_mode_ == kNonOpaque) {
      layer_->SetOpaque(image_->CurrentFrameKnownToBeOpaque());
      layer_->SetBlendBackgroundColor(!image_->CurrentFrameKnownToBeOpaque());
    }
  }
  if (!has_presented_since_last_set_image_ && image_ &&
      image_->IsTextureBacked()) {
    // If the layer bridge is not presenting, the GrContext may not be getting
    // flushed regularly.  The flush is normally triggered inside the
    // m_image->ensureMailbox() call of
    // ImageLayerBridge::PrepareTextureMailbox. To prevent a potential memory
    // leak we must flush the GrContext here.
    image_->ImageForCurrentFrame()->getTextureHandle(true);  // GrContext flush.
  }
  has_presented_since_last_set_image_ = false;
}

void ImageLayerBridge::Dispose() {
  if (layer_) {
    GraphicsLayer::UnregisterContentsLayer(layer_->Layer());
    layer_->ClearTexture();
    layer_.reset();
  }
  image_ = nullptr;
  disposed_ = true;
}

bool ImageLayerBridge::PrepareTextureMailbox(
    cc::TextureMailbox* out_mailbox,
    std::unique_ptr<cc::SingleReleaseCallback>* out_release_callback) {
  if (disposed_)
    return false;

  if (!image_)
    return false;

  if (has_presented_since_last_set_image_)
    return false;

  has_presented_since_last_set_image_ = true;

  if (image_->IsTextureBacked()) {
    image_->EnsureMailbox();
    *out_mailbox = cc::TextureMailbox(image_->GetMailbox(),
                                      image_->GetSyncToken(), GL_TEXTURE_2D);
    auto func = WTF::Bind(&ImageLayerBridge::MailboxReleasedGpu,
                          WrapWeakPersistent(this), image_);
    *out_release_callback = cc::SingleReleaseCallback::Create(
        ConvertToBaseCallback(std::move(func)));
  } else {
    std::unique_ptr<cc::SharedBitmap> bitmap = CreateOrRecycleBitmap();
    if (!bitmap)
      return false;

    sk_sp<SkImage> sk_image = image_->ImageForCurrentFrame();
    if (!sk_image)
      return false;

    SkImageInfo dst_info = SkImageInfo::MakeN32Premul(image_->width(), 1);
    size_t row_bytes = image_->width() * 4;

    // loop to flip Y
    for (int row = 0; row < image_->height(); row++) {
      if (!sk_image->readPixels(
              dst_info,
              bitmap->pixels() + row_bytes * (image_->height() - 1 - row),
              row_bytes, 0, row))
        return false;
    }

    *out_mailbox = cc::TextureMailbox(
        bitmap.get(), gfx::Size(image_->width(), image_->height()));
    auto func = WTF::Bind(&ImageLayerBridge::MailboxReleasedSoftware,
                          WrapWeakPersistent(this), base::Passed(&bitmap),
                          image_->Size());
    *out_release_callback = cc::SingleReleaseCallback::Create(
        ConvertToBaseCallback(std::move(func)));
  }

  out_mailbox->set_nearest_neighbor(filter_quality_ == kNone_SkFilterQuality);
  // TODO(junov): Figure out how to get the color space info.
  // outMailbox->set_color_space();

  return true;
}

std::unique_ptr<cc::SharedBitmap> ImageLayerBridge::CreateOrRecycleBitmap() {
  auto it = std::remove_if(recycled_bitmaps_.begin(), recycled_bitmaps_.end(),
                           [this](const RecycledBitmap& bitmap) {
                             return bitmap.size != image_->Size();
                           });
  recycled_bitmaps_.Shrink(it - recycled_bitmaps_.begin());

  if (!recycled_bitmaps_.IsEmpty()) {
    RecycledBitmap recycled = std::move(recycled_bitmaps_.back());
    recycled_bitmaps_.pop_back();
    DCHECK(recycled.size == image_->Size());
    return std::move(recycled.bitmap);
  }
  return Platform::Current()->AllocateSharedBitmap(image_->Size());
}

void ImageLayerBridge::MailboxReleasedGpu(RefPtr<StaticBitmapImage> image,
                                          const gpu::SyncToken& token,
                                          bool lost_resource) {
  if (image) {
    DCHECK(image->IsTextureBacked());
    if (token.HasData()) {
      if (image->HasMailbox()) {
        image->UpdateSyncToken(token);
      } else {
        // Wait on sync token now because SkiaTextureHolder does not know
        // about sync tokens.
        gpu::gles2::GLES2Interface* shared_gl = SharedGpuContext::Gl();
        if (shared_gl)
          shared_gl->WaitSyncTokenCHROMIUM(token.GetConstData());
      }
    }
  }
  // let 'image' go out of scope to finalize the release. The
  // destructor will wait on sync token before deleting resource.
}

void ImageLayerBridge::MailboxReleasedSoftware(
    std::unique_ptr<cc::SharedBitmap> bitmap,
    const IntSize& size,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  DCHECK(!sync_token.HasData());  // No sync tokens for software resources.
  if (!disposed_ && !lost_resource) {
    RecycledBitmap recycled = {std::move(bitmap), size};
    recycled_bitmaps_.push_back(std::move(recycled));
  }
}

WebLayer* ImageLayerBridge::PlatformLayer() const {
  return layer_->Layer();
}

}  // namespace blink
