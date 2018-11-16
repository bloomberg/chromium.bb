// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

#include "base/metrics/histogram_functions.h"
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/tiles/software_image_decode_cache.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/canvas_heuristic_parameters.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

void CanvasResourceProvider::RecordTypeToUMA(ResourceProviderType type) {
  base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType", type);
}

// * Renders to a texture managed by Skia. Mailboxes are backed by vanilla GL
//   textures.
// * Layers are not overlay candidates.
class CanvasResourceProviderTexture : public CanvasResourceProvider {
 public:
  CanvasResourceProviderTexture(
      const IntSize& size,
      unsigned msaa_sample_count,
      const CanvasColorParams color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
      bool is_origin_top_left)
      : CanvasResourceProvider(size,
                               color_params,
                               std::move(context_provider_wrapper),
                               std::move(resource_dispatcher)),
        msaa_sample_count_(msaa_sample_count),
        is_origin_top_left_(is_origin_top_left) {
    RecordTypeToUMA(kTexture);
  }

  ~CanvasResourceProviderTexture() override = default;

  bool IsValid() const final { return GetSkSurface() && !IsGpuContextLost(); }
  bool IsAccelerated() const final { return true; }
  bool SupportsDirectCompositing() const override { return true; }

  GLuint GetBackingTextureHandleForOverwrite() override {
    GrBackendTexture backend_texture = GetSkSurface()->getBackendTexture(
        SkSurface::kDiscardWrite_TextureHandleAccess);
    if (!backend_texture.isValid())
      return 0;
    GrGLTextureInfo info;
    if (!backend_texture.getGLTextureInfo(&info))
      return 0;
    return info.fID;
  }

 protected:
  scoped_refptr<CanvasResource> ProduceFrame() override {
    TRACE_EVENT0("blink", "CanvasResourceProviderTexture::ProduceFrame");
    DCHECK(GetSkSurface());

    if (IsGpuContextLost())
      return nullptr;

    auto* gl = ContextGL();
    DCHECK(gl);

    if (ContextProviderWrapper()
            ->ContextProvider()
            ->GetCapabilities()
            .disable_2d_canvas_copy_on_write) {
      // A readback operation may alter the texture parameters, which may affect
      // the compositor's behavior. Therefore, we must trigger copy-on-write
      // even though we are not technically writing to the texture, only to its
      // parameters. This issue is Android-WebView specific: crbug.com/585250.
      // If this issue with readback affecting state is ever fixed, then we'll
      // have to do this instead of triggering a copy-on-write:
      // static_cast<AcceleratedStaticBitmapImage*>(image.get())
      //  ->RetainOriginalSkImageForCopyOnWrite();
      GetSkSurface()->notifyContentWillChange(
          SkSurface::kRetain_ContentChangeMode);
    }

    auto paint_image = MakeImageSnapshot();
    if (!paint_image)
      return nullptr;
    DCHECK(paint_image.GetSkImage()->isTextureBacked());

    scoped_refptr<StaticBitmapImage> image = StaticBitmapImage::Create(
        paint_image.GetSkImage(), ContextProviderWrapper());

    scoped_refptr<CanvasResource> resource = CanvasResourceBitmap::Create(
        image, CreateWeakPtr(), FilterQuality(), ColorParams());
    if (!resource)
      return nullptr;

    return resource;
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    TRACE_EVENT0("blink", "CanvasResourceProviderTexture::CreateSkSurface");

    if (IsGpuContextLost())
      return nullptr;
    auto* gr = GetGrContext();
    DCHECK(gr);

    const SkImageInfo info = SkImageInfo::Make(
        Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
        kPremul_SkAlphaType, ColorParams().GetSkColorSpaceForSkSurfaces());

    const enum GrSurfaceOrigin surface_origin =
        is_origin_top_left_ ? kTopLeft_GrSurfaceOrigin
                            : kBottomLeft_GrSurfaceOrigin;

    return SkSurface::MakeRenderTarget(gr, SkBudgeted::kNo, info,
                                       msaa_sample_count_, surface_origin,
                                       ColorParams().GetSkSurfaceProps());
  }

  const unsigned msaa_sample_count_;
  const bool is_origin_top_left_;
};

// * Renders to a texture managed by Skia. Mailboxes are GPU-accelerated
//   platform native surfaces.
// * Layers are overlay candidates.
class CanvasResourceProviderTextureGpuMemoryBuffer final
    : public CanvasResourceProviderTexture {
 public:
  CanvasResourceProviderTextureGpuMemoryBuffer(
      const IntSize& size,
      unsigned msaa_sample_count,
      const CanvasColorParams color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
      bool is_origin_top_left)
      : CanvasResourceProviderTexture(size,
                                      msaa_sample_count,
                                      color_params,
                                      std::move(context_provider_wrapper),
                                      std::move(resource_dispatcher),
                                      is_origin_top_left) {
    RecordTypeToUMA(kTextureGpuMemoryBuffer);
  }

  ~CanvasResourceProviderTextureGpuMemoryBuffer() override = default;
  bool SupportsDirectCompositing() const override { return true; }
  bool SupportsSingleBuffering() const override { return true; }

 private:
  scoped_refptr<CanvasResource> CreateResource() final {
    TRACE_EVENT0(
        "blink",
        "CanvasResourceProviderTextureGpuMemoreBuffer::CreateResource");
    constexpr bool is_accelerated = true;
    return CanvasResourceGpuMemoryBuffer::Create(
        Size(), ColorParams(), ContextProviderWrapper(), CreateWeakPtr(),
        FilterQuality(), is_accelerated);
  }

  scoped_refptr<CanvasResource> ProduceFrame() final {
    TRACE_EVENT0("blink",
                 "CanvasResourceProviderTextureGpuMemoreBuffer::ProduceFrame");
    DCHECK(GetSkSurface());

    if (IsGpuContextLost())
      return nullptr;

    scoped_refptr<CanvasResource> output_resource = NewOrRecycledResource();
    if (!output_resource) {
      // GpuMemoryBuffer creation failed, fallback to Texture resource
      return CanvasResourceProviderTexture::ProduceFrame();
    }

    auto paint_image = MakeImageSnapshot();
    if (!paint_image)
      return nullptr;
    DCHECK(paint_image.GetSkImage()->isTextureBacked());

    GrBackendTexture backend_texture =
        paint_image.GetSkImage()->getBackendTexture(true);
    DCHECK(backend_texture.isValid());

    GrGLTextureInfo info;
    if (!backend_texture.getGLTextureInfo(&info))
      return nullptr;

    GLuint skia_texture_id = info.fID;
    output_resource->CopyFromTexture(skia_texture_id,
                                     ColorParams().GLUnsizedInternalFormat(),
                                     ColorParams().GLType());
    return output_resource;
  }
};

// * Renders to a Skia RAM-backed bitmap.
// * Mailboxing is not supported : cannot be directly composited.
class CanvasResourceProviderBitmap : public CanvasResourceProvider {
 public:
  CanvasResourceProviderBitmap(
      const IntSize& size,
      const CanvasColorParams color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProvider(size,
                               color_params,
                               std::move(context_provider_wrapper),
                               std::move(resource_dispatcher)) {
    RecordTypeToUMA(kBitmap);
  }

  ~CanvasResourceProviderBitmap() override = default;

  bool IsValid() const final { return GetSkSurface(); }
  bool IsAccelerated() const final { return false; }
  bool SupportsDirectCompositing() const override { return false; }

 private:
  scoped_refptr<CanvasResource> ProduceFrame() override {
    return nullptr;  // Does not support direct compositing
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    TRACE_EVENT0("blink", "CanvasResourceProviderBitmap::CreateSkSurface");

    SkImageInfo info = SkImageInfo::Make(
        Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
        kPremul_SkAlphaType, ColorParams().GetSkColorSpaceForSkSurfaces());
    return SkSurface::MakeRaster(info, ColorParams().GetSkSurfaceProps());
  }
};

// * Renders to a ram memory buffer managed by Skia
// * Uses GpuMemoryBuffer to pass frames to the compositor
// * Layers are overlay candidates
class CanvasResourceProviderBitmapGpuMemoryBuffer final
    : public CanvasResourceProviderBitmap {
 public:
  CanvasResourceProviderBitmapGpuMemoryBuffer(
      const IntSize& size,
      const CanvasColorParams color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProviderBitmap(size,
                                     color_params,
                                     std::move(context_provider_wrapper),
                                     std::move(resource_dispatcher)) {
    RecordTypeToUMA(kBitmapGpuMemoryBuffer);
  }

  ~CanvasResourceProviderBitmapGpuMemoryBuffer() override = default;
  bool SupportsDirectCompositing() const override { return true; }
  bool SupportsSingleBuffering() const override { return true; }

 private:
  scoped_refptr<CanvasResource> CreateResource() final {
    TRACE_EVENT0("blink",
                 "CanvasResourceProviderBitmapGpuMemoryBuffer::CreateResource");

    constexpr bool is_accelerated = false;
    return CanvasResourceGpuMemoryBuffer::Create(
        Size(), ColorParams(), ContextProviderWrapper(), CreateWeakPtr(),
        FilterQuality(), is_accelerated);
  }

  scoped_refptr<CanvasResource> ProduceFrame() final {
    TRACE_EVENT0("blink",
                 "CanvasResourceProviderBitmapGpuMemoryBuffer::ProduceFrame");

    DCHECK(GetSkSurface());

    scoped_refptr<CanvasResource> output_resource = NewOrRecycledResource();
    if (!output_resource) {
      // Not compositable without a GpuMemoryBuffer
      return nullptr;
    }

    auto paint_image = MakeImageSnapshot();
    if (!paint_image)
      return nullptr;
    DCHECK(!paint_image.GetSkImage()->isTextureBacked());

    output_resource->TakeSkImage(paint_image.GetSkImage());

    return output_resource;
  }
};

// * Renders to a shared memory bitmap.
// * Uses SharedBitmaps to pass frames directly to the compositor.
class CanvasResourceProviderSharedBitmap : public CanvasResourceProviderBitmap {
 public:
  CanvasResourceProviderSharedBitmap(
      const IntSize& size,
      const CanvasColorParams color_params,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProviderBitmap(size,
                                     color_params,
                                     nullptr,  // context_provider_wrapper
                                     std::move(resource_dispatcher)) {
    DCHECK(ResourceDispatcher());
    RecordTypeToUMA(kSharedBitmap);
  }
  ~CanvasResourceProviderSharedBitmap() override = default;
  bool SupportsDirectCompositing() const override { return true; }
  bool SupportsSingleBuffering() const override { return true; }

 private:
  scoped_refptr<CanvasResource> CreateResource() final {
    CanvasColorParams color_params = ColorParams();
    if (!IsBitmapFormatSupported(color_params.TransferableResourceFormat())) {
      // If the rendering format is not supported, downgrate to 8-bits.
      // TODO(junov): Should we try 12-12-12-12 and 10-10-10-2?
      color_params.SetCanvasPixelFormat(kRGBA8CanvasPixelFormat);
    }

    return CanvasResourceSharedBitmap::Create(Size(), color_params,
                                              CreateWeakPtr(), FilterQuality());
  }

  scoped_refptr<CanvasResource> ProduceFrame() final {
    DCHECK(GetSkSurface());
    scoped_refptr<CanvasResource> output_resource = NewOrRecycledResource();
    if (!output_resource)
      return nullptr;

    auto paint_image = MakeImageSnapshot();
    if (!paint_image)
      return nullptr;
    DCHECK(!paint_image.GetSkImage()->isTextureBacked());

    output_resource->TakeSkImage(paint_image.GetSkImage());

    return output_resource;
  }
};

enum CanvasResourceType {
  kTextureGpuMemoryBufferResourceType,
  kRamGpuMemoryBufferResourceType,
  kSharedBitmapResourceType,
  kTextureResourceType,
  kBitmapResourceType,
};

constexpr CanvasResourceType kSoftwareCompositedFallbackList[] = {
    kRamGpuMemoryBufferResourceType, kSharedBitmapResourceType,
    // Fallback to no direct compositing support
    kBitmapResourceType,
};

constexpr CanvasResourceType kSoftwareFallbackList[] = {
    kBitmapResourceType,
};

constexpr CanvasResourceType kAcceleratedFallbackList[] = {
    kTextureResourceType,
    // Fallback to software
    kBitmapResourceType,
};

constexpr CanvasResourceType kAcceleratedCompositedFallbackList[] = {
    kTextureGpuMemoryBufferResourceType, kTextureResourceType,
    // Fallback to software composited
    kRamGpuMemoryBufferResourceType, kSharedBitmapResourceType,
    // Fallback to no direct compositing support
    kBitmapResourceType,
};

std::unique_ptr<CanvasResourceProvider> CanvasResourceProvider::Create(
    const IntSize& size,
    ResourceUsage usage,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    unsigned msaa_sample_count,
    const CanvasColorParams& color_params,
    PresentationMode presentation_mode,
    base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
    bool is_origin_top_left) {
  const CanvasResourceType* resource_type_fallback_list = nullptr;
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
    // Note: We are deliberately not using std::move() on
    // context_provider_wrapper and resource_dispatcher to ensure that the
    // pointers remain valid for the next iteration of this loop if necessary.
    switch (resource_type_fallback_list[i]) {
      case kTextureGpuMemoryBufferResourceType:
        if (!SharedGpuContext::IsGpuCompositingEnabled())
          continue;
        if (presentation_mode != kAllowImageChromiumPresentationMode)
          continue;
        if (!context_provider_wrapper)
          continue;
        if (!gpu::IsImageFromGpuMemoryBufferFormatSupported(
                color_params.GetBufferFormat(),
                context_provider_wrapper->ContextProvider()
                    ->GetCapabilities())) {
          continue;
        }
        if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(
                gfx::Size(size), color_params.GetBufferFormat())) {
          continue;
        }
        DCHECK_EQ(color_params.GLUnsizedInternalFormat(),
                  gpu::InternalFormatForGpuMemoryBufferFormat(
                      color_params.GetBufferFormat()));
        provider =
            std::make_unique<CanvasResourceProviderTextureGpuMemoryBuffer>(
                size, msaa_sample_count, color_params, context_provider_wrapper,
                resource_dispatcher, is_origin_top_left);
        break;
      case kRamGpuMemoryBufferResourceType:
        if (!SharedGpuContext::IsGpuCompositingEnabled())
          continue;
        if (presentation_mode != kAllowImageChromiumPresentationMode)
          continue;
        if (!context_provider_wrapper)
          continue;
        if (!Platform::Current()->GetGpuMemoryBufferManager())
          continue;
        if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(
                gfx::Size(size), color_params.GetBufferFormat())) {
          continue;
        }
        provider =
            std::make_unique<CanvasResourceProviderBitmapGpuMemoryBuffer>(
                size, color_params, context_provider_wrapper,
                resource_dispatcher);
        break;
      case kSharedBitmapResourceType:
        if (!resource_dispatcher)
          continue;
        provider = std::make_unique<CanvasResourceProviderSharedBitmap>(
            size, color_params, resource_dispatcher);
        break;
      case kTextureResourceType:
        if (!context_provider_wrapper)
          continue;
        provider = std::make_unique<CanvasResourceProviderTexture>(
            size, msaa_sample_count, color_params, context_provider_wrapper,
            resource_dispatcher, is_origin_top_left);
        break;
      case kBitmapResourceType:
        provider = std::make_unique<CanvasResourceProviderBitmap>(
            size, color_params, context_provider_wrapper, resource_dispatcher);
        break;
    }
    if (provider && provider->IsValid()) {
      base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                                provider->IsAccelerated());
      return provider;
    }
  }

  return nullptr;
}

class CanvasResourceProvider::CanvasImageProvider : public cc::ImageProvider {
 public:
  CanvasImageProvider(cc::ImageDecodeCache* cache_n32,
                      cc::ImageDecodeCache* cache_f16,
                      SkColorType target_color_type);
  ~CanvasImageProvider() override = default;

  // cc::ImageProvider implementation.
  ScopedDecodedDrawImage GetDecodedDrawImage(const cc::DrawImage&) override;

  void ReleaseLockedImages() { locked_images_.clear(); }

 private:
  void CanUnlockImage(ScopedDecodedDrawImage);
  void CleanupLockedImages();

  bool cleanup_task_pending_ = false;
  std::vector<ScopedDecodedDrawImage> locked_images_;
  cc::PlaybackImageProvider playback_image_provider_n32_;
  base::Optional<cc::PlaybackImageProvider> playback_image_provider_f16_;

  base::WeakPtrFactory<CanvasImageProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CanvasImageProvider);
};

CanvasResourceProvider::CanvasImageProvider::CanvasImageProvider(
    cc::ImageDecodeCache* cache_n32,
    cc::ImageDecodeCache* cache_f16,
    SkColorType canvas_color_type)
    : playback_image_provider_n32_(cache_n32,
                                   cc::PlaybackImageProvider::Settings()),
      weak_factory_(this) {
  // If the image provider may require to decode to half float instead of
  // uint8, create a f16 PlaybackImageProvider with the passed cache.
  if (canvas_color_type == kRGBA_F16_SkColorType) {
    DCHECK(cache_f16);
    playback_image_provider_f16_.emplace(cache_f16,
                                         cc::PlaybackImageProvider::Settings());
  }
}

cc::ImageProvider::ScopedDecodedDrawImage
CanvasResourceProvider::CanvasImageProvider::GetDecodedDrawImage(
    const cc::DrawImage& draw_image) {
  // If we like to decode high bit depth image source to half float backed
  // image, we need to sniff the image bit depth here to avoid double decoding.
  ImageProvider::ScopedDecodedDrawImage scoped_decoded_image;
  if (playback_image_provider_f16_ &&
      draw_image.paint_image().is_high_bit_depth()) {
    DCHECK(playback_image_provider_f16_);
    scoped_decoded_image =
        playback_image_provider_f16_->GetDecodedDrawImage(draw_image);
  } else {
    scoped_decoded_image =
        playback_image_provider_n32_.GetDecodedDrawImage(draw_image);
  }
  if (!scoped_decoded_image.needs_unlock())
    return scoped_decoded_image;

  constexpr int kMaxLockedImagesCount = 500;
  if (!scoped_decoded_image.decoded_image().is_budgeted() ||
      locked_images_.size() > kMaxLockedImagesCount) {
    // If we have exceeded the budget, ReleaseLockedImages any locked decodes.
    ReleaseLockedImages();
  }

  auto decoded_draw_image = scoped_decoded_image.decoded_image();
  return ScopedDecodedDrawImage(
      decoded_draw_image, base::BindOnce(&CanvasImageProvider::CanUnlockImage,
                                         weak_factory_.GetWeakPtr(),
                                         std::move(scoped_decoded_image)));
}

void CanvasResourceProvider::CanvasImageProvider::CanUnlockImage(
    ScopedDecodedDrawImage image) {
  if (!cleanup_task_pending_) {
    cleanup_task_pending_ = true;
    Thread::Current()->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&CanvasImageProvider::CleanupLockedImages,
                                  weak_factory_.GetWeakPtr()));
  }

  locked_images_.push_back(std::move(image));
}

void CanvasResourceProvider::CanvasImageProvider::CleanupLockedImages() {
  cleanup_task_pending_ = false;
  ReleaseLockedImages();
}

CanvasResourceProvider::CanvasResourceProvider(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
    : context_provider_wrapper_(std::move(context_provider_wrapper)),
      resource_dispatcher_(resource_dispatcher),
      size_(size),
      color_params_(color_params),
      snapshot_paint_image_id_(cc::PaintImage::GetNextId()),
      weak_ptr_factory_(this) {
  if (context_provider_wrapper_)
    context_provider_wrapper_->AddObserver(this);
}

CanvasResourceProvider::~CanvasResourceProvider() {
  if (context_provider_wrapper_)
    context_provider_wrapper_->RemoveObserver(this);
}

SkSurface* CanvasResourceProvider::GetSkSurface() const {
  if (!surface_)
    surface_ = CreateSkSurface();
  return surface_.get();
}

cc::PaintCanvas* CanvasResourceProvider::Canvas() {
  if (!canvas_) {
    TRACE_EVENT0("blink", "CanvasResourceProvider::Canvas");

    DCHECK(!canvas_image_provider_);

    // Create an ImageDecodeCache for half float images only if the canvas is
    // using half float back storage.
    cc::ImageDecodeCache* cache_f16 = nullptr;
    if (ColorParams().PixelFormat() == kF16CanvasPixelFormat)
      cache_f16 = ImageDecodeCacheF16();
    canvas_image_provider_ = std::make_unique<CanvasImageProvider>(
        ImageDecodeCacheRGBA8(), cache_f16, color_params_.GetSkColorType());

    cc::SkiaPaintCanvas::ContextFlushes context_flushes;
    if (IsAccelerated() &&
        !ContextProviderWrapper()
             ->ContextProvider()
             ->GetGpuFeatureInfo()
             .IsWorkaroundEnabled(gpu::DISABLE_2D_CANVAS_AUTO_FLUSH)) {
      context_flushes.enable =
          canvas_heuristic_parameters::kEnableGrContextFlushes;
      context_flushes.max_draws_before_flush =
          canvas_heuristic_parameters::kMaxDrawsBeforeContextFlush;
    }
    if (ColorParams().NeedsSkColorSpaceXformCanvas()) {
      canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
          GetSkSurface()->getCanvas(), ColorParams().GetSkColorSpace(),
          canvas_image_provider_.get(), context_flushes);
    } else {
      canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
          GetSkSurface()->getCanvas(), canvas_image_provider_.get(),
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

  auto paint_image = MakeImageSnapshot();
  if (paint_image.GetSkImage()->isTextureBacked() && ContextProviderWrapper()) {
    return StaticBitmapImage::Create(paint_image.GetSkImage(),
                                     ContextProviderWrapper());
  }
  return StaticBitmapImage::Create(std::move(paint_image));
}

cc::PaintImage CanvasResourceProvider::MakeImageSnapshot() {
  auto sk_image = GetSkSurface()->makeImageSnapshot();
  if (!sk_image)
    return cc::PaintImage();

  auto last_snapshot_sk_image_id = snapshot_sk_image_id_;
  snapshot_sk_image_id_ = sk_image->uniqueID();

  // Ensure that a new PaintImage::ContentId is used only when the underlying
  // SkImage changes. This is necessary to ensure that the same image results
  // in a cache hit in cc's ImageDecodeCache.
  if (snapshot_paint_image_content_id_ == PaintImage::kInvalidContentId ||
      last_snapshot_sk_image_id != snapshot_sk_image_id_) {
    snapshot_paint_image_content_id_ = PaintImage::GetNextContentId();
  }

  return PaintImageBuilder::WithDefault()
      .set_id(snapshot_paint_image_id_)
      .set_image(std::move(sk_image), snapshot_paint_image_content_id_)
      .TakePaintImage();
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

bool CanvasResourceProvider::IsGpuContextLost() const {
  auto* gl = ContextGL();
  return !gl || gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

bool CanvasResourceProvider::WritePixels(const SkImageInfo& orig_info,
                                         const void* pixels,
                                         size_t row_bytes,
                                         int x,
                                         int y) {
  TRACE_EVENT0("blink", "CanvasResourceProvider::WritePixels");

  DCHECK(IsValid());
  return GetSkSurface()->getCanvas()->writePixels(orig_info, pixels, row_bytes,
                                                  x, y);
}

void CanvasResourceProvider::Clear() {
  // Clear the background transparent or opaque, as required. It would be nice
  // if this wasn't required, but the canvas is currently filled with the magic
  // transparency color. Can we have another way to manage this?
  DCHECK(IsValid());
  if (color_params_.GetOpacityMode() == kOpaque)
    Canvas()->clear(SK_ColorBLACK);
  else
    Canvas()->clear(SK_ColorTRANSPARENT);
}

uint32_t CanvasResourceProvider::ContentUniqueID() const {
  return GetSkSurface()->generationID();
}

scoped_refptr<CanvasResource> CanvasResourceProvider::CreateResource() {
  // Needs to be implemented in subclasses that use resource recycling.
  NOTREACHED();
  return nullptr;
}

cc::ImageDecodeCache* CanvasResourceProvider::ImageDecodeCacheRGBA8() {
  auto color_space = ColorParams().ColorSpace();
  if (!ColorParams().NeedsSkColorSpaceXformCanvas()) {
    color_space = kSRGBCanvasColorSpace;
  }

  if (IsAccelerated() && context_provider_wrapper_) {
    return context_provider_wrapper_->ContextProvider()->ImageDecodeCache(
        kN32_SkColorType,
        blink::CanvasColorParams::CanvasColorSpaceToSkColorSpace(color_space));
  }

  return Image::SharedCCDecodeCache(color_space, kRGBA8CanvasPixelFormat);
}

cc::ImageDecodeCache* CanvasResourceProvider::ImageDecodeCacheF16() {
  auto color_space = ColorParams().ColorSpace();
  if (!ColorParams().NeedsSkColorSpaceXformCanvas()) {
    color_space = kSRGBCanvasColorSpace;
  }

  if (IsAccelerated() && context_provider_wrapper_) {
    return context_provider_wrapper_->ContextProvider()->ImageDecodeCache(
        kRGBA_F16_SkColorType,
        blink::CanvasColorParams::CanvasColorSpaceToSkColorSpace(color_space));
  }
  return Image::SharedCCDecodeCache(color_space, kF16CanvasPixelFormat);
}

void CanvasResourceProvider::RecycleResource(
    scoped_refptr<CanvasResource> resource) {
  // Need to check HasOneRef() because if there are outstanding references to
  // the resource, it cannot be safely recycled.
  if (resource->HasOneRef() && resource_recycling_enabled_)
    canvas_resources_.push_back(std::move(resource));
}

void CanvasResourceProvider::SetResourceRecyclingEnabled(bool value) {
  resource_recycling_enabled_ = value;
  if (!resource_recycling_enabled_)
    ClearRecycledResources();
}

void CanvasResourceProvider::ClearRecycledResources() {
  canvas_resources_.clear();
}

scoped_refptr<CanvasResource> CanvasResourceProvider::NewOrRecycledResource() {
  if (canvas_resources_.IsEmpty())
    canvas_resources_.push_back(CreateResource());

  if (IsSingleBuffered()) {
    DCHECK_EQ(canvas_resources_.size(), 1u);
    return canvas_resources_.back();
  }

  scoped_refptr<CanvasResource> resource = std::move(canvas_resources_.back());
  canvas_resources_.pop_back();
  return resource;
}

void CanvasResourceProvider::TryEnableSingleBuffering() {
  if (IsSingleBuffered() || !SupportsSingleBuffering())
    return;
  SetResourceRecyclingEnabled(false);
}

}  // namespace blink
