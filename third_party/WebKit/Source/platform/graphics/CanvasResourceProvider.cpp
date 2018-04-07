// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/paint/skia_paint_canvas.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "skia/ext/texture_handle.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_heuristic_parameters.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/skia/include/core/SkColorSpaceXformCanvas.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace blink {

// CanvasResourceProvider_Texture
//==============================================================================
//
// * Renders to a texture managed by skia. Mailboxes are straight GL textures.
// * Layers are not overlay candidates

class CanvasResourceProvider_Texture : public CanvasResourceProvider {
 public:
  CanvasResourceProvider_Texture(
      const IntSize& size,
      unsigned msaa_sample_count,
      const CanvasColorParams color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper)
      : CanvasResourceProvider(size,
                               color_params,
                               std::move(context_provider_wrapper)),
        msaa_sample_count_(msaa_sample_count) {}

  virtual ~CanvasResourceProvider_Texture() = default;

  bool IsValid() const final { return GetSkSurface() && !IsGpuContextLost(); }
  bool IsAccelerated() const final { return true; }

  GLuint GetBackingTextureHandleForOverwrite() override {
    return skia::GrBackendObjectToGrGLTextureInfo(
               GetSkSurface()->getTextureHandle(
                   SkSurface::kDiscardWrite_TextureHandleAccess))
        ->fID;
  }

 protected:
  scoped_refptr<CanvasResource> ProduceFrame() override {
    DCHECK(GetSkSurface());

    if (IsGpuContextLost())
      return nullptr;

    auto gl = ContextGL();
    DCHECK(gl);

    if (ContextProviderWrapper()
            ->ContextProvider()
            ->GetCapabilities()
            .disable_2d_canvas_copy_on_write) {
      // A readback operation may alter the texture parameters, which may affect
      // the compositor's behavior. Therefore, we must trigger copy-on-write
      // even though we are not technically writing to the texture, only to its
      // parameters.
      // If this issue with readback affecting state is ever fixed, then we'll
      // have to do this instead of triggering a copy-on-write:
      // static_cast<AcceleratedStaticBitmapImage*>(image.get())
      //  ->RetainOriginalSkImageForCopyOnWrite();
      GetSkSurface()->notifyContentWillChange(
          SkSurface::kRetain_ContentChangeMode);
    }

    sk_sp<SkImage> skia_image = GetSkSurface()->makeImageSnapshot();
    if (!skia_image)
      return nullptr;
    DCHECK(skia_image->isTextureBacked());

    scoped_refptr<StaticBitmapImage> image =
        StaticBitmapImage::Create(skia_image, ContextProviderWrapper());

    scoped_refptr<CanvasResource> resource =
        CanvasResource_Bitmap::Create(image, CreateWeakPtr(), FilterQuality());
    if (!resource)
      return nullptr;

    return resource;
  }

  virtual sk_sp<SkSurface> CreateSkSurface() const {
    if (IsGpuContextLost())
      return nullptr;
    auto gr = GetGrContext();
    DCHECK(gr);

    SkImageInfo info = SkImageInfo::Make(
        Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
        kPremul_SkAlphaType, ColorParams().GetSkColorSpaceForSkSurfaces());
    return SkSurface::MakeRenderTarget(gr, SkBudgeted::kNo, info,
                                       msaa_sample_count_,
                                       ColorParams().GetSkSurfaceProps());
  }

  unsigned msaa_sample_count_;
};

// CanvasResourceProvider_Texture_GpuMemoryBuffer
//==============================================================================
//
// * Renders to a texture managed by skia. Mailboxes are
//     gpu-accelerated platform native surfaces.
// * Layers are overlay candidates

class CanvasResourceProvider_Texture_GpuMemoryBuffer final
    : public CanvasResourceProvider_Texture {
 public:
  CanvasResourceProvider_Texture_GpuMemoryBuffer(
      const IntSize& size,
      unsigned msaa_sample_count,
      const CanvasColorParams color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper)
      : CanvasResourceProvider_Texture(size,
                                       msaa_sample_count,
                                       color_params,
                                       std::move(context_provider_wrapper)) {}

  virtual ~CanvasResourceProvider_Texture_GpuMemoryBuffer() = default;

 protected:
  scoped_refptr<CanvasResource> CreateResource() final {
    return CanvasResource_GpuMemoryBuffer::Create(
        Size(), ColorParams(), ContextProviderWrapper(), CreateWeakPtr(),
        FilterQuality());
  }

  scoped_refptr<CanvasResource> ProduceFrame() final {
    DCHECK(GetSkSurface());

    if (IsGpuContextLost())
      return nullptr;

    scoped_refptr<CanvasResource> output_resource = NewOrRecycledResource();
    if (!output_resource) {
      // GpuMemoryBuffer creation failed, fallback to Texture resource
      return CanvasResourceProvider_Texture::ProduceFrame();
    }

    sk_sp<SkImage> image = GetSkSurface()->makeImageSnapshot();
    if (!image)
      return nullptr;
    DCHECK(image->isTextureBacked());

    GLuint skia_texture_id =
        skia::GrBackendObjectToGrGLTextureInfo(image->getTextureHandle(true))
            ->fID;

    output_resource->CopyFromTexture(skia_texture_id,
                                     ColorParams().GLInternalFormat(),
                                     ColorParams().GLType());

    return output_resource;
  }
};

// CanvasResourceProvider_Bitmap
//==============================================================================
//
// * Renders to a skia RAM-backed bitmap
// * Mailboxing is not supported : cannot be directly composited

class CanvasResourceProvider_Bitmap final : public CanvasResourceProvider {
 public:
  CanvasResourceProvider_Bitmap(const IntSize& size,
                                const CanvasColorParams color_params)
      : CanvasResourceProvider(size,
                               color_params,
                               nullptr /*context_provider_wrapper*/) {}

  ~CanvasResourceProvider_Bitmap() = default;

  bool IsValid() const final { return GetSkSurface(); }
  bool IsAccelerated() const final { return false; }

 private:
  scoped_refptr<CanvasResource> ProduceFrame() final {
    NOTREACHED();  // Not directly compositable.
    return nullptr;
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    SkImageInfo info = SkImageInfo::Make(
        Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
        kPremul_SkAlphaType, ColorParams().GetSkColorSpaceForSkSurfaces());
    return SkSurface::MakeRaster(info, ColorParams().GetSkSurfaceProps());
  }

  sk_sp<SkSurface> surface_;
};

// CanvasResourceProvider base class implementation
//==============================================================================

enum ResourceType {
  kTextureGpuMemoryBufferResourceType,
  kTextureResourceType,
  kBitmapResourceType,
};

constexpr ResourceType kSoftwareCompositedFallbackList[] = {
    kBitmapResourceType,
};

constexpr ResourceType kSoftwareFallbackList[] = {
    kBitmapResourceType,
};

constexpr ResourceType kAcceleratedFallbackList[] = {
    kTextureResourceType, kBitmapResourceType,
};

constexpr ResourceType kAcceleratedCompositedFallbackList[] = {
    kTextureGpuMemoryBufferResourceType, kTextureResourceType,
    kBitmapResourceType,
};

std::unique_ptr<CanvasResourceProvider> CanvasResourceProvider::Create(
    const IntSize& size,
    ResourceUsage usage,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    unsigned msaa_sample_count,
    const CanvasColorParams& colorParams) {
  const ResourceType* resource_type_fallback_list = nullptr;
  size_t list_length = 0;

  switch (usage) {
    case kSoftwareResourceUsage:
      resource_type_fallback_list = kSoftwareFallbackList;
      list_length = arraysize(kSoftwareFallbackList);
      break;
    case kSoftwareCompositedResourceUsage:
      resource_type_fallback_list = kSoftwareCompositedFallbackList;
      list_length = arraysize(kSoftwareCompositedFallbackList);
      break;
    case kAcceleratedResourceUsage:
      resource_type_fallback_list = kAcceleratedFallbackList;
      list_length = arraysize(kAcceleratedFallbackList);
      break;
    case kAcceleratedCompositedResourceUsage:
      resource_type_fallback_list = kAcceleratedCompositedFallbackList;
      list_length = arraysize(kAcceleratedCompositedFallbackList);
      break;
  }

  std::unique_ptr<CanvasResourceProvider> provider;
  for (size_t i = 0; i < list_length; ++i) {
    switch (resource_type_fallback_list[i]) {
      case kTextureGpuMemoryBufferResourceType:
        DCHECK(SharedGpuContext::IsGpuCompositingEnabled());
        if (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled()) {
          if (!gpu::IsImageFromGpuMemoryBufferFormatSupported(
                  colorParams.GetBufferFormat(),
                  context_provider_wrapper->ContextProvider()
                      ->GetCapabilities()))
            continue;
          if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(
                  gfx::Size(size), colorParams.GetBufferFormat()))
            continue;
          DCHECK(gpu::IsImageFormatCompatibleWithGpuMemoryBufferFormat(
              colorParams.GLInternalFormat(), colorParams.GetBufferFormat()));

          provider =
              std::make_unique<CanvasResourceProvider_Texture_GpuMemoryBuffer>(
                  size, msaa_sample_count, colorParams,
                  context_provider_wrapper);
        }
        break;
      case kTextureResourceType:
        // TODO(xlai): Check gpu acclereration mode before using this Resource
        // Type of CanvasResourceProvider and then Add
        // "DCHECK(SharedGpuContext::IsGpuCompositingEnabled());" here.
        // See crbug.com/802053.
        provider = std::make_unique<CanvasResourceProvider_Texture>(
            size, msaa_sample_count, colorParams, context_provider_wrapper);
        break;
      case kBitmapResourceType:
        provider =
            std::make_unique<CanvasResourceProvider_Bitmap>(size, colorParams);
        break;
    }
    if (provider && provider->IsValid())
      return provider;
  }

  return nullptr;
}

CanvasResourceProvider::CanvasImageProvider::CanvasImageProvider(
    cc::ImageDecodeCache* cache,
    const gfx::ColorSpace& target_color_space)
    : playback_image_provider_(cache,
                               target_color_space,
                               cc::PlaybackImageProvider::Settings()) {}

CanvasResourceProvider::CanvasImageProvider::~CanvasImageProvider() = default;

cc::ImageProvider::ScopedDecodedDrawImage
CanvasResourceProvider::CanvasImageProvider::GetDecodedDrawImage(
    const cc::DrawImage& draw_image) {
  auto scoped_decoded_image =
      playback_image_provider_.GetDecodedDrawImage(draw_image);
  if (!scoped_decoded_image.needs_unlock())
    return scoped_decoded_image;

  if (!scoped_decoded_image.decoded_image().is_budgeted()) {
    // If we have exceeded the budget, ReleaseLockedImages any locked decodes.
    ReleaseLockedImages();
  }

  // It is safe to use base::Unretained, since decodes acquired from a provider
  // must not exceed the provider's lifetime.
  auto decoded_draw_image = scoped_decoded_image.decoded_image();
  return ScopedDecodedDrawImage(
      decoded_draw_image,
      base::BindOnce(&CanvasImageProvider::CanUnlockImage,
                     base::Unretained(this), std::move(scoped_decoded_image)));
}

void CanvasResourceProvider::CanvasImageProvider::ReleaseLockedImages() {
  locked_images_.clear();
}

void CanvasResourceProvider::CanvasImageProvider::CanUnlockImage(
    ScopedDecodedDrawImage image) {
  locked_images_.push_back(std::move(image));
}

CanvasResourceProvider::CanvasResourceProvider(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper)
    : weak_ptr_factory_(this),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      size_(size),
      color_params_(color_params),
      snapshot_paint_image_id_(cc::PaintImage::GetNextId()) {
  if (context_provider_wrapper_)
    context_provider_wrapper_->AddObserver(this);
}

CanvasResourceProvider::~CanvasResourceProvider() {
  if (context_provider_wrapper_)
    context_provider_wrapper_->RemoveObserver(this);
}

SkSurface* CanvasResourceProvider::GetSkSurface() const {
  if (!surface_) {
    surface_ = CreateSkSurface();
  }
  return surface_.get();
}

PaintCanvas* CanvasResourceProvider::Canvas() {
  if (!canvas_) {
    DCHECK(!canvas_image_provider_);

    cc::ImageProvider* image_provider = nullptr;
    if (ImageDecodeCache()) {
      canvas_image_provider_.emplace(ImageDecodeCache(),
                                     ColorParams().GetStorageGfxColorSpace());
      image_provider = &*canvas_image_provider_;
    }

    cc::SkiaPaintCanvas::ContextFlushes context_flushes;
    if (IsAccelerated()) {
      context_flushes.enable =
          CanvasHeuristicParameters::kEnableGrContextFlushes;
      context_flushes.max_draws_before_flush =
          CanvasHeuristicParameters::kMaxDrawsBeforeContextFlush;
    }
    if (ColorParams().NeedsSkColorSpaceXformCanvas()) {
      canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
          GetSkSurface()->getCanvas(), ColorParams().GetSkColorSpace(),
          std::move(image_provider), context_flushes);
    } else {
      canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
          GetSkSurface()->getCanvas(), std::move(image_provider),
          context_flushes);
    }
  }

  return canvas_.get();
}

void CanvasResourceProvider::OnContextDestroyed() {
  if (canvas_image_provider_) {
    DCHECK(canvas_);
    canvas_->reset_image_provider();
    canvas_image_provider_.reset();
  }
}

void CanvasResourceProvider::ReleaseLockedImages() {
  if (canvas_image_provider_)
    canvas_image_provider_->ReleaseLockedImages();
}

scoped_refptr<StaticBitmapImage> CanvasResourceProvider::Snapshot() {
  if (!IsValid())
    return nullptr;

  auto sk_image = GetSkSurface()->makeImageSnapshot();
  auto last_snapshot_sk_image_id = snapshot_sk_image_id_;
  snapshot_sk_image_id_ = sk_image->uniqueID();

  if (ContextProviderWrapper()) {
    return StaticBitmapImage::Create(std::move(sk_image),
                                     ContextProviderWrapper());
  }

  // Ensure that a new PaintImage::ContentId is used only when the underlying
  // SkImage changes. This is necessary to ensure that the same image results
  // in a cache hit in cc's ImageDecodeCache.
  if (snapshot_paint_image_content_id_ == PaintImage::kInvalidContentId ||
      last_snapshot_sk_image_id != snapshot_sk_image_id_) {
    snapshot_paint_image_content_id_ = PaintImage::GetNextContentId();
  }

  auto paint_image =
      PaintImageBuilder::WithDefault()
          .set_id(snapshot_paint_image_id_)
          .set_image(std::move(sk_image), snapshot_paint_image_content_id_)
          .TakePaintImage();
  return StaticBitmapImage::Create(std::move(paint_image));
}

gpu::gles2::GLES2Interface* CanvasResourceProvider::ContextGL() const {
  if (!context_provider_wrapper_)
    return nullptr;
  return context_provider_wrapper_->ContextProvider()->ContextGL();
}

GrContext* CanvasResourceProvider::GetGrContext() const {
  if (!context_provider_wrapper_)
    return nullptr;
  return context_provider_wrapper_->ContextProvider()->GetGrContext();
}

void CanvasResourceProvider::FlushSkia() const {
  GetSkSurface()->flush();
}

void CanvasResourceProvider::RecycleResource(
    scoped_refptr<CanvasResource> resource) {
  DCHECK(resource->HasOneRef());
  if (resource_recycling_enabled_)
    recycled_resources_.push_back(std::move(resource));
}

void CanvasResourceProvider::SetResourceRecyclingEnabled(bool value) {
  resource_recycling_enabled_ = value;
  if (!resource_recycling_enabled_)
    ClearRecycledResources();
}

scoped_refptr<CanvasResource> CanvasResourceProvider::NewOrRecycledResource() {
  if (recycled_resources_.size()) {
    scoped_refptr<CanvasResource> resource =
        std::move(recycled_resources_.back());
    recycled_resources_.pop_back();
    // Recycling implies releasing the old content
    resource->WaitSyncTokenBeforeRelease();
    return resource;
  }
  return CreateResource();
}

bool CanvasResourceProvider::IsGpuContextLost() const {
  auto gl = ContextGL();
  return !gl || gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

bool CanvasResourceProvider::WritePixels(const SkImageInfo& orig_info,
                                         const void* pixels,
                                         size_t row_bytes,
                                         int x,
                                         int y) {
  DCHECK(IsValid());
  return GetSkSurface()->getCanvas()->writePixels(orig_info, pixels, row_bytes,
                                                  x, y);
}

void CanvasResourceProvider::Clear() {
  // Clear the background transparent or opaque, as required. It would be nice
  // if this wasn't required, but the canvas is currently filled with the magic
  // transparency color. Can we have another way to manage this?
  DCHECK(IsValid());
  if (color_params_.GetOpacityMode() == kOpaque) {
    Canvas()->clear(SK_ColorBLACK);
  } else {
    Canvas()->clear(SK_ColorTRANSPARENT);
  }
}

void CanvasResourceProvider::ClearRecycledResources() {
  recycled_resources_.clear();
}

void CanvasResourceProvider::InvalidateSurface() {
  canvas_ = nullptr;
  canvas_image_provider_.reset();
  xform_canvas_ = nullptr;
  surface_ = nullptr;
}

uint32_t CanvasResourceProvider::ContentUniqueID() const {
  return GetSkSurface()->generationID();
}

scoped_refptr<CanvasResource> CanvasResourceProvider::CreateResource() {
  // Needs to be implemented in subclasses that use resource recycling.
  NOTREACHED();
  return nullptr;
}

cc::ImageDecodeCache* CanvasResourceProvider::ImageDecodeCache() {
  // TODO(khushalsagar): Hook up a software cache.
  if (!context_provider_wrapper_)
    return nullptr;
  return context_provider_wrapper_->ContextProvider()->ImageDecodeCache();
}

}  // namespace blink
