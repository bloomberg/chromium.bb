// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasResourceProvider.h"

#include "cc/paint/skia_paint_canvas.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "platform/graphics/CanvasResource.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/runtime_enabled_features.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkColorSpaceXformCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
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

  virtual ~CanvasResourceProvider_Texture() {}

  bool IsValid() const final { return GetSkSurface() && !IsGpuContextLost(); }
  bool IsAccelerated() const final { return true; }

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

  virtual ~CanvasResourceProvider_Texture_GpuMemoryBuffer() {}

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

  ~CanvasResourceProvider_Bitmap() {}

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
    unsigned msaa_sample_count,
    const CanvasColorParams& colorParams,
    ResourceUsage usage,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper) {
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
        DCHECK(SharedGpuContext::IsGpuCompositingEnabled());
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

CanvasResourceProvider::CanvasResourceProvider(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper)
    : weak_ptr_factory_(this),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      size_(size),
      color_params_(color_params) {}

CanvasResourceProvider::~CanvasResourceProvider() {}

SkSurface* CanvasResourceProvider::GetSkSurface() const {
  if (!surface_) {
    surface_ = CreateSkSurface();
  }
  return surface_.get();
}

PaintCanvas* CanvasResourceProvider::Canvas() {
  if (!canvas_) {
    if (ColorParams().NeedsSkColorSpaceXformCanvas()) {
      canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
          GetSkSurface()->getCanvas(), ColorParams().GetSkColorSpace());
    } else {
      canvas_ =
          std::make_unique<cc::SkiaPaintCanvas>(GetSkSurface()->getCanvas());
    }
  }
  return canvas_.get();
}

scoped_refptr<StaticBitmapImage> CanvasResourceProvider::Snapshot() {
  if (!IsValid())
    return nullptr;
  scoped_refptr<StaticBitmapImage> image = StaticBitmapImage::Create(
      GetSkSurface()->makeImageSnapshot(), ContextProviderWrapper());
  if (IsAccelerated()) {
    // A readback operation may alter the texture parameters, which may affect
    // the compositor's behavior. Therefore, we must trigger copy-on-write
    // even though we are not technically writing to the texture, only to its
    // parameters.
    // If this issue with readback affecting state is ever fixed, then we'll
    // have to do this instead of triggering a copy-on-write:
    // static_cast<AcceleratedStaticBitmapImage*>(image.get())
    //   ->RetainOriginalSkImageForCopyOnWrite();
    GetSkSurface()->notifyContentWillChange(
        SkSurface::kRetain_ContentChangeMode);
  }
  return image;
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

void CanvasResourceProvider::ClearRecycledResources() {
  recycled_resources_.clear();
}

void CanvasResourceProvider::InvalidateSurface() {
  canvas_ = nullptr;
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

}  // namespace blink
