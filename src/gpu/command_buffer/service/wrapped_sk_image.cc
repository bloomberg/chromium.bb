// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/wrapped_sk_image.h"

#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/trace_util.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif

namespace gpu {
namespace raster {

class WrappedSkImageFactory;

namespace {

size_t EstimatedSize(viz::ResourceFormat format, const gfx::Size& size) {
  size_t estimated_size = 0;
  viz::ResourceSizes::MaybeSizeInBytes(size, format, &estimated_size);
  return estimated_size;
}

SkImageInfo MakeSkImageInfo(const gfx::Size& size, viz::ResourceFormat format) {
  return SkImageInfo::Make(size.width(), size.height(),
                           ResourceFormatToClosestSkColorType(
                               /*gpu_compositing=*/true, format),
                           kOpaque_SkAlphaType);
}

class WrappedSkImage : public ClearTrackingSharedImageBacking {
 public:
  WrappedSkImage(base::PassKey<WrappedSkImageFactory>,
                 const Mailbox& mailbox,
                 viz::ResourceFormat format,
                 const gfx::Size& size,
                 const gfx::ColorSpace& color_space,
                 GrSurfaceOrigin surface_origin,
                 SkAlphaType alpha_type,
                 uint32_t usage,
                 size_t estimated_size,
                 scoped_refptr<SharedContextState> context_state,
                 const bool thread_safe)
      : ClearTrackingSharedImageBacking(mailbox,
                                        format,
                                        size,
                                        color_space,
                                        surface_origin,
                                        alpha_type,
                                        usage,
                                        estimated_size,
                                        thread_safe),
        context_state_(std::move(context_state)) {
    DCHECK(!!context_state_);

    // If the backing is meant to be thread safe, then grab the task runner to
    // destroy the object later on same thread on which it was created on. Note
    // that SkSurface and GrBackendTexture are not thread safe and hence should
    // be destroyed on same thread on which it was created on.
    if (is_thread_safe()) {
      // If backing is thread safe, then ensure that we have a task runner to
      // destroy backing on correct thread. Webview doesn't have a task runner
      // but it uses and shares this backing on a single thread (on render
      // passes for display compositor) and DrDc is disabled on webview. Hence
      // using is_thread_safe() to grab task_runner is enough to ensure
      // correctness.
      DCHECK(base::ThreadTaskRunnerHandle::IsSet());
      task_runner_ = base::ThreadTaskRunnerHandle::Get();
    }
  }

  WrappedSkImage(const WrappedSkImage&) = delete;
  WrappedSkImage& operator=(const WrappedSkImage&) = delete;

  ~WrappedSkImage() override {
    auto destroy_resources = [](scoped_refptr<SharedContextState> context_state,
                                sk_sp<SkPromiseImageTexture> promise_texture,
                                GrBackendTexture backend_texture) {
      context_state->MakeCurrent(nullptr);

      // Note that if we fail to initialize this backing, |promise_texture| will
      // not be created and hence could be null while backing is destroyed after
      // a failed init.
      if (promise_texture)
        context_state->EraseCachedSkSurface(promise_texture.get());
      promise_texture.reset();

      if (backend_texture.isValid())
        DeleteGrBackendTexture(context_state.get(), &backend_texture);

      if (!context_state->context_lost())
        context_state->set_need_context_state_reset(true);
    };

    // Since the representation from this backing can be created on either gpu
    // main or drdc thread, the last representation ref and hence the backing
    // could be destroyed in any thread irrespective of the thread it was
    // created on. Hence we need to ensure that the resources are destroyed on
    // the thread they were created on.
    if (task_runner_ && !task_runner_->BelongsToCurrentThread()) {
      auto destruction_cb =
          base::BindPostTask(task_runner_, base::BindOnce(destroy_resources));
      std::move(destruction_cb)
          .Run(std::move(context_state_), std::move(promise_texture_),
               std::move(backend_texture_));
    } else {
      destroy_resources(std::move(context_state_), std::move(promise_texture_),
                        std::move(backend_texture_));
    }
  }

  // SharedImageBacking implementation.
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    return false;
  }

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {
    if (shared_memory_wrapper_.IsValid()) {
      DCHECK(!in_fence);

      if (context_state_->context_lost())
        return;

      DCHECK(context_state_->IsCurrent(nullptr));

      SkImageInfo info = MakeSkImageInfo(size(), format());
      SkPixmap pixmap(info, shared_memory_wrapper_.GetMemory(),
                      shared_memory_wrapper_.GetStride());
      if (!context_state_->gr_context()->updateBackendTexture(
              backend_texture_, &pixmap, /*numLevels=*/1, nullptr, nullptr)) {
        DLOG(ERROR) << "Failed to update WrappedSkImage texture";
      }
    }
  }

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {
    // Add a |service_guid| which expresses shared ownership between the
    // various GPU dumps.
    auto client_guid = GetSharedImageGUIDForTracing(mailbox());
    auto service_guid = gl::GetGLTextureServiceGUIDForTracing(tracing_id_);
    pmd->CreateSharedGlobalAllocatorDump(service_guid);

    std::string format_dump_name =
        base::StringPrintf("%s/format=%d", dump_name.c_str(), format());
    base::trace_event::MemoryAllocatorDump* format_dump =
        pmd->CreateAllocatorDump(format_dump_name);
    format_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
        static_cast<uint64_t>(EstimatedSizeForMemTracking()));

    // TODO(piman): coalesce constant with TextureManager::DumpTextureRef.
    int importance = 2;  // This client always owns the ref.
    pmd->AddOwnershipEdge(client_guid, service_guid, importance);
  }

  SkColorType GetSkColorType() {
    return viz::ResourceFormatToClosestSkColorType(
        /*gpu_compositing=*/true, format());
  }

  sk_sp<SkSurface> GetSkSurface(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      scoped_refptr<SharedContextState> context_state) {
    // This method should only be called on the same thread on which this
    // backing is created on. Hence adding a dcheck on context_state to ensure
    // this.
    DCHECK_EQ(context_state_, context_state);
    if (context_state_->context_lost())
      return nullptr;
    DCHECK(context_state_->IsCurrent(nullptr));

    // Note that we are using |promise_texture_| as a key to the cache below
    // since it is safe to do so. |promise_texture_| is not destroyed until we
    // remove the entry from the cache.
    DCHECK(promise_texture_);
    auto surface = context_state_->GetCachedSkSurface(promise_texture_.get());
    if (!surface || final_msaa_count != surface_msaa_count_ ||
        surface_props != surface->props()) {
      surface = SkSurface::MakeFromBackendTexture(
          context_state_->gr_context(), backend_texture_, surface_origin(),
          final_msaa_count, GetSkColorType(), color_space().ToSkColorSpace(),
          &surface_props);
      if (!surface) {
        LOG(ERROR) << "MakeFromBackendTexture() failed.";
        context_state_->EraseCachedSkSurface(promise_texture_.get());
        return nullptr;
      }
      surface_msaa_count_ = final_msaa_count;
      context_state_->CacheSkSurface(promise_texture_.get(), surface);
    }
    return surface;
  }

  bool SkSurfaceUnique(scoped_refptr<SharedContextState> context_state) {
    // This method should only be called on the same thread on which this
    // backing is created on. Hence adding a dcheck on context_state to ensure
    // this.
    DCHECK_EQ(context_state_, context_state);
    DCHECK(promise_texture_);
    return context_state_->CachedSkSurfaceIsUnique(promise_texture_.get());
  }

  sk_sp<SkPromiseImageTexture> promise_texture() { return promise_texture_; }

  const SharedMemoryRegionWrapper& shared_memory_wrapper() {
    return shared_memory_wrapper_;
  }

 protected:
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  std::unique_ptr<SharedImageRepresentationMemory> ProduceMemory(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

 private:
  friend class gpu::raster::WrappedSkImageFactory;
  class RepresentationSkia;
  class RepresentationMemory;

  bool Initialize() {
    // MakeCurrent to avoid destroying another client's state because Skia may
    // change GL state to create and upload textures (crbug.com/1095679).
    if (!context_state_->MakeCurrent(nullptr))
      return false;
    context_state_->set_need_context_state_reset(true);

    DCHECK_NE(format(), viz::ResourceFormat::ETC1);
    auto mipmap = usage() & SHARED_IMAGE_USAGE_MIPMAP ? GrMipMapped::kYes
                                                      : GrMipMapped::kNo;
#if DCHECK_IS_ON()
    // Initializing to bright green makes it obvious if the pixels are not
    // properly set before they are displayed (e.g. https://crbug.com/956555).
    // We don't do this on release builds because there is a slight overhead.
    backend_texture_ = context_state_->gr_context()->createBackendTexture(
        size().width(), size().height(), GetSkColorType(), SkColors::kBlue,
        mipmap, GrRenderable::kYes, GrProtected::kNo);
#else
    backend_texture_ = context_state_->gr_context()->createBackendTexture(
        size().width(), size().height(), GetSkColorType(), mipmap,
        GrRenderable::kYes, GrProtected::kNo);
#endif

    if (!backend_texture_.isValid()) {
      DLOG(ERROR) << "createBackendTexture() failed with SkColorType:"
                  << GetSkColorType();
      return false;
    }

    promise_texture_ = SkPromiseImageTexture::Make(backend_texture_);
    tracing_id_ = GrBackendTextureTracingID(backend_texture_);

    return true;
  }

  // |pixels| optionally contains pixel data to upload to the texture. If pixel
  // data is provided and the image format is not ETC1 then |stride| is used. If
  // |stride| is non-zero then it's used as the stride, otherwise it will create
  // SkImageInfo from size() and format() and then SkImageInfo::minRowBytes() is
  // used for the stride. For ETC1 textures pixel data must be provided since
  // updating compressed textures is not supported.
  bool InitializeWithData(base::span<const uint8_t> pixels, size_t stride) {
    DCHECK(pixels.data());
    // MakeCurrent to avoid destroying another client's state because Skia may
    // change GL state to create and upload textures (crbug.com/1095679).
    if (!context_state_->MakeCurrent(nullptr))
      return false;
    context_state_->set_need_context_state_reset(true);

      if (format() == viz::ResourceFormat::ETC1) {
        backend_texture_ =
            context_state_->gr_context()->createCompressedBackendTexture(
                size().width(), size().height(), SkImage::kETC1_CompressionType,
                pixels.data(), pixels.size(), GrMipMapped::kNo,
                GrProtected::kNo);
      } else {
        auto info = MakeSkImageInfo(size(), format());
        if (!stride)
          stride = info.minRowBytes();
        SkPixmap pixmap(info, pixels.data(), stride);
        backend_texture_ = context_state_->gr_context()->createBackendTexture(
            pixmap, GrRenderable::kNo, GrProtected::kNo);
      }

      if (!backend_texture_.isValid())
        return false;

      SetCleared();

    promise_texture_ = SkPromiseImageTexture::Make(backend_texture_);
    tracing_id_ = GrBackendTextureTracingID(backend_texture_);

    return true;
  }

  bool InitializeWithGMB(SharedMemoryRegionWrapper shm_wrapper) {
    if (InitializeWithData(shm_wrapper.GetMemoryAsSpan(),
                           shm_wrapper.GetStride())) {
      shared_memory_wrapper_ = std::move(shm_wrapper);
      return true;
    }
    return false;
  }

  scoped_refptr<SharedContextState> context_state_;

  GrBackendTexture backend_texture_;
  sk_sp<SkPromiseImageTexture> promise_texture_;
  int surface_msaa_count_ = 0;

  // Set for shared memory GMB.
  SharedMemoryRegionWrapper shared_memory_wrapper_;

  uint64_t tracing_id_ = 0;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class WrappedSkImage::RepresentationSkia
    : public SharedImageRepresentationSkia {
 public:
  RepresentationSkia(SharedImageManager* manager,
                     SharedImageBacking* backing,
                     MemoryTypeTracker* tracker,
                     scoped_refptr<SharedContextState> context_state)
      : SharedImageRepresentationSkia(manager, backing, tracker),
        context_state_(std::move(context_state)) {}

  ~RepresentationSkia() override { DCHECK(!write_surface_); }

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    auto surface = wrapped_sk_image()->GetSkSurface(
        final_msaa_count, surface_props, context_state_);
    if (!surface)
      return nullptr;
    [[maybe_unused]] int save_count = surface->getCanvas()->save();
    DCHECK_EQ(1, save_count);
    write_surface_ = surface.get();
    return surface;
  }

  sk_sp<SkPromiseImageTexture> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    return wrapped_sk_image()->promise_texture();
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override {
    if (surface) {
      DCHECK_EQ(surface.get(), write_surface_);
      surface->getCanvas()->restoreToCount(1);
      surface.reset();
      write_surface_ = nullptr;

      DCHECK(wrapped_sk_image()->SkSurfaceUnique(context_state_));
    }
  }

  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    DCHECK(!write_surface_);
    return wrapped_sk_image()->promise_texture();
  }

  void EndReadAccess() override {
    DCHECK(!write_surface_);
    // TODO(ericrk): Handle begin/end correctness checks.
  }

  bool SupportsMultipleConcurrentReadAccess() override { return true; }

 private:
  WrappedSkImage* wrapped_sk_image() {
    return static_cast<WrappedSkImage*>(backing());
  }

  raw_ptr<SkSurface> write_surface_ = nullptr;
  scoped_refptr<SharedContextState> context_state_;
};

class WrappedSkImage::RepresentationMemory
    : public SharedImageRepresentationMemory {
 public:
  RepresentationMemory(SharedImageManager* manager,
                       SharedImageBacking* backing,
                       MemoryTypeTracker* tracker)
      : SharedImageRepresentationMemory(manager, backing, tracker) {}

 protected:
  SkPixmap BeginReadAccess() override {
    SkImageInfo info = MakeSkImageInfo(wrapped_sk_image()->size(),
                                       wrapped_sk_image()->format());
    return SkPixmap(info,
                    wrapped_sk_image()->shared_memory_wrapper().GetMemory(),
                    wrapped_sk_image()->shared_memory_wrapper().GetStride());
  }

 private:
  WrappedSkImage* wrapped_sk_image() {
    return static_cast<WrappedSkImage*>(backing());
  }
};

}  // namespace

WrappedSkImageFactory::WrappedSkImageFactory(
    scoped_refptr<SharedContextState> context_state)
    : context_state_(std::move(context_state)),
      is_drdc_enabled_(features::IsDrDcEnabled()) {}

WrappedSkImageFactory::~WrappedSkImageFactory() = default;

std::unique_ptr<SharedImageBacking> WrappedSkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_thread_safe) {
  // Ensure that the backing is treated as thread safe only when DrDc is enabled
  // for vulkan context.
  // TODO(vikassoni): Wire |is_thread_safe| flag in remaining
  // CreateSharedImage() factory methods also. Without this flag, backing will
  // always be considered as thread safe when DrDc is enabled for vulkan mode
  // even though it might be used on a single thread (RenderPass for example).
  // That should be fine for now since we do not have/use any locks in backing.
  DCHECK(!is_thread_safe ||
         (context_state_->GrContextIsVulkan() && is_drdc_enabled_));
  size_t estimated_size = EstimatedSize(format, size);
  auto texture = std::make_unique<WrappedSkImage>(
      base::PassKey<WrappedSkImageFactory>(), mailbox, format, size,
      color_space, surface_origin, alpha_type, usage, estimated_size,
      context_state_,
      /*is_thread_safe=*/is_thread_safe &&
          context_state_->GrContextIsVulkan() && is_drdc_enabled_);
  if (!texture->Initialize())
    return nullptr;
  return texture;
}

std::unique_ptr<SharedImageBacking> WrappedSkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> data) {
  size_t estimated_size = EstimatedSize(format, size);
  auto texture = std::make_unique<WrappedSkImage>(
      base::PassKey<WrappedSkImageFactory>(), mailbox, format, size,
      color_space, surface_origin, alpha_type, usage, estimated_size,
      context_state_, /*is_thread_safe=*/context_state_->GrContextIsVulkan() &&
                          is_drdc_enabled_);
  if (!texture->InitializeWithData(data, /*stride=*/0))
    return nullptr;
  return texture;
}

std::unique_ptr<SharedImageBacking> WrappedSkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  DCHECK_EQ(handle.type, gfx::SHARED_MEMORY_BUFFER);

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    DLOG(ERROR) << "Invalid image size for format.";
    return nullptr;
  }

  if (gfx::NumberOfPlanesForLinearBufferFormat(buffer_format) != 1) {
    DLOG(ERROR) << "Invalid image format.";
    return nullptr;
  }
  if (plane != gfx::BufferPlane::DEFAULT) {
    DLOG(ERROR) << "Invalid plane " << gfx::BufferPlaneToString(plane);
    return nullptr;
  }

  viz::ResourceFormat format = viz::GetResourceFormat(buffer_format);

  // The Skia API to handle compressed texture is limited and not compatible
  // with updating the texture or custom strides.
  DCHECK_NE(format, viz::ResourceFormat::ETC1);

  SharedMemoryRegionWrapper shm_wrapper;
  if (!shm_wrapper.Initialize(handle, size, format))
    return nullptr;

  auto info = MakeSkImageInfo(size, format);
  auto texture = std::make_unique<WrappedSkImage>(
      base::PassKey<WrappedSkImageFactory>(), mailbox, format, size,
      color_space, surface_origin, alpha_type, usage, info.computeMinByteSize(),
      context_state_, /*is_thread_safe=*/context_state_->GrContextIsVulkan() &&
                          is_drdc_enabled_);
  if (!texture->InitializeWithGMB(std::move(shm_wrapper)))
    return nullptr;

  return texture;
}

bool WrappedSkImageFactory::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return memory_buffer_type == gfx::SHARED_MEMORY_BUFFER;
}

bool WrappedSkImageFactory::CanUseWrappedSkImage(
    uint32_t usage,
    GrContextType gr_context_type) const {
  // Ignore for mipmap usage.
  usage &= ~SHARED_IMAGE_USAGE_MIPMAP;
  auto kWrappedSkImageUsage = SHARED_IMAGE_USAGE_DISPLAY |
                              SHARED_IMAGE_USAGE_RASTER |
                              SHARED_IMAGE_USAGE_OOP_RASTERIZATION;

  if (gr_context_type != GrContextType::kGL) {
    // For SkiaRenderer/Vulkan+Dawn use WrappedSkImage if the usage is only
    // raster and/or display.
    return (usage & kWrappedSkImageUsage) && !(usage & ~kWrappedSkImageUsage);
  } else {
    // For SkiaRenderer/GL only use WrappedSkImages for OOP-R because
    // CopySubTexture() doesn't use Skia. https://crbug.com/984045
    return (usage == kWrappedSkImageUsage) ||
           (usage == SHARED_IMAGE_USAGE_DISPLAY);
  }
}

bool WrappedSkImageFactory::IsSupported(uint32_t usage,
                                        viz::ResourceFormat format,
                                        bool thread_safe,
                                        gfx::GpuMemoryBufferType gmb_type,
                                        GrContextType gr_context_type,
                                        bool* allow_legacy_mailbox,
                                        bool is_pixel_used) {
  // Note that this backing support thread safety only for vulkan mode because
  // the underlying vulkan resources like vulkan images can be shared across
  // multiple vulkan queues. Also note that this backing currently only supports
  // thread safety for DrDc mode where both gpu main and drdc thread uses/shared
  // a single vulkan queue to submit work and hence do not need to synchronize
  // the reads/writes using semaphores. For this backing to support thread
  // safety across multiple queues, we need to synchronize the reads/writes via
  // semaphores.
  if (thread_safe &&
      (!is_drdc_enabled_ || gr_context_type != GrContextType::kVulkan)) {
    return false;
  }

  if (!CanUseWrappedSkImage(usage, gr_context_type)) {
    return false;
  }
  if (gmb_type != gfx::EMPTY_BUFFER && !CanImportGpuMemoryBuffer(gmb_type)) {
    return false;
  }

  *allow_legacy_mailbox = false;
  return true;
}

std::unique_ptr<SharedImageRepresentationSkia> WrappedSkImage::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (context_state_->context_lost())
    return nullptr;

  return std::make_unique<RepresentationSkia>(manager, this, tracker,
                                              std::move(context_state));
}

std::unique_ptr<SharedImageRepresentationMemory> WrappedSkImage::ProduceMemory(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  if (!shared_memory_wrapper_.IsValid())
    return nullptr;

  return std::make_unique<RepresentationMemory>(manager, this, tracker);
}

}  // namespace raster
}  // namespace gpu
