// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/gpu/ImageLayerBridge.h"

#include "components/viz/common/quads/shared_bitmap.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/graphics/AcceleratedStaticBitmapImage.h"
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

void ImageLayerBridge::SetImage(scoped_refptr<StaticBitmapImage> image) {
  if (disposed_)
    return;

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
    // m_image->EnsureMailbox() call of
    // ImageLayerBridge::PrepareTransferableResource. To prevent a potential
    // memory leak we must flush the GrContext here.
    image_->PaintImageForCurrentFrame().GetSkImage()->getTextureHandle(
        true);  // GrContext flush.
  }
  has_presented_since_last_set_image_ = false;
}

void ImageLayerBridge::SetUV(const FloatPoint left_top,
                             const FloatPoint right_bottom) {
  if (disposed_)
    return;

  layer_->SetUV(WebFloatPoint(left_top.X(), left_top.Y()),
                WebFloatPoint(right_bottom.X(), right_bottom.Y()));
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

bool ImageLayerBridge::PrepareTransferableResource(
    cc::SharedBitmapIdRegistrar* bitmap_registrar,
    viz::TransferableResource* out_resource,
    std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback) {
  if (disposed_)
    return false;

  if (!image_)
    return false;

  if (has_presented_since_last_set_image_)
    return false;

  has_presented_since_last_set_image_ = true;

  bool gpu_compositing = SharedGpuContext::IsGpuCompositingEnabled();
  bool gpu_image = image_->IsTextureBacked();

  // Expect software images for software compositing.
  if (!gpu_compositing && gpu_image)
    return false;

  // If the texture comes from a software image then it does not need to be
  // flipped.
  layer_->SetFlipped(gpu_image);

  scoped_refptr<StaticBitmapImage> image_for_compositor;

  // Upload to a texture if the compositor is expecting one.
  if (gpu_compositing && !image_->IsTextureBacked()) {
    image_for_compositor =
        image_->MakeAccelerated(SharedGpuContext::ContextProviderWrapper());
  } else if (!gpu_compositing && image_->IsTextureBacked()) {
    image_for_compositor = image_->MakeUnaccelerated();
  } else {
    image_for_compositor = image_;
  }
  DCHECK_EQ(image_for_compositor->IsTextureBacked(), gpu_compositing);

  if (gpu_compositing) {
    uint32_t filter =
        filter_quality_ == kNone_SkFilterQuality ? GL_NEAREST : GL_LINEAR;
    image_for_compositor->EnsureMailbox(kUnverifiedSyncToken, filter);
    *out_resource = viz::TransferableResource::MakeGL(
        image_for_compositor->GetMailbox(), filter, GL_TEXTURE_2D,
        image_for_compositor->GetSyncToken());
    auto func =
        WTF::Bind(&ImageLayerBridge::ResourceReleasedGpu,
                  WrapWeakPersistent(this), std::move(image_for_compositor));
    *out_release_callback = viz::SingleReleaseCallback::Create(std::move(func));
  } else {
    std::unique_ptr<viz::SharedBitmap> bitmap =
        CreateOrRecycleBitmap(image_for_compositor->Size());
    if (!bitmap)
      return false;

    sk_sp<SkImage> sk_image =
        image_for_compositor->PaintImageForCurrentFrame().GetSkImage();
    if (!sk_image)
      return false;

    SkImageInfo dst_info =
        SkImageInfo::MakeN32Premul(image_for_compositor->width(), 1);
    dst_info = dst_info.makeColorSpace(sk_image->refColorSpace());
    size_t row_bytes = image_for_compositor->width() * 4;

    // Copy from SkImage into |bitmap|, while flipping the Y axis.
    for (int row = 0; row < image_for_compositor->height(); row++) {
      if (!sk_image->readPixels(dst_info, bitmap->pixels(), row_bytes, 0, 0))
        return false;
    }

    *out_resource = viz::TransferableResource::MakeSoftware(
        bitmap->id(), bitmap->sequence_number(),
        gfx::Size(image_for_compositor->width(),
                  image_for_compositor->height()),
        viz::RGBA_8888);
    auto func = WTF::Bind(&ImageLayerBridge::ResourceReleasedSoftware,
                          WrapWeakPersistent(this), base::Passed(&bitmap),
                          image_for_compositor->Size());
    *out_release_callback = viz::SingleReleaseCallback::Create(std::move(func));
  }

  // TODO(junov): Figure out how to get the color space info.
  // out_resource->color_space = ...;

  return true;
}

std::unique_ptr<viz::SharedBitmap> ImageLayerBridge::CreateOrRecycleBitmap(
    const IntSize& size) {
  auto it = std::remove_if(
      recycled_bitmaps_.begin(), recycled_bitmaps_.end(),
      [&size](const RecycledBitmap& bitmap) { return bitmap.size != size; });
  recycled_bitmaps_.Shrink(it - recycled_bitmaps_.begin());

  if (!recycled_bitmaps_.IsEmpty()) {
    RecycledBitmap recycled = std::move(recycled_bitmaps_.back());
    recycled_bitmaps_.pop_back();
    DCHECK(recycled.size == size);
    return std::move(recycled.bitmap);
  }
  return Platform::Current()->AllocateSharedBitmap(size, viz::RGBA_8888);
}

void ImageLayerBridge::ResourceReleasedGpu(
    scoped_refptr<StaticBitmapImage> image,
    const gpu::SyncToken& token,
    bool lost_resource) {
  if (image && image->IsValid()) {
    DCHECK(image->IsTextureBacked());
    if (token.HasData() && image->ContextProvider() &&
        image->ContextProvider()->ContextGL()) {
      image->ContextProvider()->ContextGL()->WaitSyncTokenCHROMIUM(
          token.GetConstData());
    }
  }
  // let 'image' go out of scope to release gpu resources.
}

void ImageLayerBridge::ResourceReleasedSoftware(
    std::unique_ptr<viz::SharedBitmap> bitmap,
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
