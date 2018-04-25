// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/image_layer_bridge.h"

#include "cc/resources/cross_thread_shared_bitmap.h"
#include "components/viz/common/quads/shared_bitmap.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_compositor_support.h"
#include "third_party/blink/public/platform/web_external_texture_layer.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"

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
    image_->PaintImageForCurrentFrame().GetSkImage()->getBackendTexture(
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
    const gfx::Size size(image_for_compositor->width(),
                         image_for_compositor->height());
    RegisteredBitmap registered = CreateOrRecycleBitmap(size, bitmap_registrar);

    sk_sp<SkImage> sk_image =
        image_for_compositor->PaintImageForCurrentFrame().GetSkImage();
    if (!sk_image)
      return false;

    SkImageInfo dst_info = SkImageInfo::MakeN32Premul(
        size.width(), size.height(), sk_image->refColorSpace());
    void* pixels = registered.bitmap->shared_memory()->memory();

    // Copy from SkImage into SharedMemory owned by |registered|.
    if (!sk_image->readPixels(dst_info, pixels, dst_info.minRowBytes(), 0, 0))
      return false;

    *out_resource = viz::TransferableResource::MakeSoftware(
        registered.bitmap->id(), size, viz::RGBA_8888);
    auto func = WTF::Bind(&ImageLayerBridge::ResourceReleasedSoftware,
                          WrapWeakPersistent(this), std::move(registered));
    *out_release_callback = viz::SingleReleaseCallback::Create(std::move(func));
  }

  // TODO(junov): Figure out how to get the color space info.
  // out_resource->color_space = ...;

  return true;
}

ImageLayerBridge::RegisteredBitmap ImageLayerBridge::CreateOrRecycleBitmap(
    const gfx::Size& size,
    cc::SharedBitmapIdRegistrar* bitmap_registrar) {
  auto* it = std::remove_if(recycled_bitmaps_.begin(), recycled_bitmaps_.end(),
                            [&size](const RegisteredBitmap& registered) {
                              return registered.bitmap->size() != size;
                            });
  recycled_bitmaps_.Shrink(it - recycled_bitmaps_.begin());

  if (!recycled_bitmaps_.IsEmpty()) {
    RegisteredBitmap registered = std::move(recycled_bitmaps_.back());
    recycled_bitmaps_.pop_back();
    DCHECK(registered.bitmap->size() == size);
    return registered;
  }

  // There are no bitmaps to recycle so allocate a new one.
  viz::SharedBitmapId id = viz::SharedBitmap::GenerateId();
  std::unique_ptr<base::SharedMemory> shm =
      viz::bitmap_allocation::AllocateMappedBitmap(size, viz::RGBA_8888);

  RegisteredBitmap registered;
  registered.bitmap = base::MakeRefCounted<cc::CrossThreadSharedBitmap>(
      id, std::move(shm), size, viz::RGBA_8888);
  registered.registration =
      bitmap_registrar->RegisterSharedBitmapId(id, registered.bitmap);

  return registered;
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
    RegisteredBitmap registered,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  DCHECK(!sync_token.HasData());  // No sync tokens for software resources.
  if (!disposed_ && !lost_resource)
    recycled_bitmaps_.push_back(std::move(registered));
}

WebLayer* ImageLayerBridge::PlatformLayer() const {
  return layer_->Layer();
}

ImageLayerBridge::RegisteredBitmap::RegisteredBitmap() = default;
ImageLayerBridge::RegisteredBitmap::RegisteredBitmap(RegisteredBitmap&& other) =
    default;
ImageLayerBridge::RegisteredBitmap& ImageLayerBridge::RegisteredBitmap::
operator=(RegisteredBitmap&& other) = default;

}  // namespace blink
