// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_ahardwarebuffer.h"

#include <sync/sync.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/posix/eintr_wrapper.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image_representation_skia_gl.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/common/android/android_image_reader_utils.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/android/android_surface_control_compat.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {
namespace {

class OverlayImage final : public gl::GLImage {
 public:
  explicit OverlayImage(AHardwareBuffer* buffer)
      : handle_(base::android::ScopedHardwareBufferHandle::Create(buffer)) {}

  void SetBeginFence(base::ScopedFD fence_fd) {
    DCHECK(!end_read_fence_.is_valid());
    DCHECK(!begin_read_fence_.is_valid());
    begin_read_fence_ = std::move(fence_fd);
  }

  base::ScopedFD TakeEndFence() {
    DCHECK(!begin_read_fence_.is_valid());
    return std::move(end_read_fence_);
  }

  // gl::GLImage:
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override {
    return std::make_unique<ScopedHardwareBufferFenceSyncImpl>(
        this, base::android::ScopedHardwareBufferHandle::Create(handle_.get()),
        std::move(begin_read_fence_));
  }

 protected:
  ~OverlayImage() override = default;

 private:
  class ScopedHardwareBufferFenceSyncImpl
      : public base::android::ScopedHardwareBufferFenceSync {
   public:
    ScopedHardwareBufferFenceSyncImpl(
        scoped_refptr<OverlayImage> image,
        base::android::ScopedHardwareBufferHandle handle,
        base::ScopedFD fence_fd)
        : ScopedHardwareBufferFenceSync(std::move(handle), std::move(fence_fd)),
          image_(std::move(image)) {}
    ~ScopedHardwareBufferFenceSyncImpl() override = default;

    void SetReadFence(base::ScopedFD fence_fd, bool has_context) override {
      DCHECK(!image_->begin_read_fence_.is_valid());
      DCHECK(!image_->end_read_fence_.is_valid());
      image_->end_read_fence_ = std::move(fence_fd);
    }

   private:
    scoped_refptr<OverlayImage> image_;
  };

  base::android::ScopedHardwareBufferHandle handle_;

  // The fence for overlay controller to wait on before scanning out.
  base::ScopedFD begin_read_fence_;

  // The fence for overlay controller to set to indicate scanning out
  // completion. The image content should not be modified before passing this
  // fence.
  base::ScopedFD end_read_fence_;
};

}  // namespace

// Implementation of SharedImageBacking that holds an AHardwareBuffer. This
// can be used to create a GL texture or a VK Image from the AHardwareBuffer
// backing.
class SharedImageBackingAHB : public ClearTrackingSharedImageBacking {
 public:
  SharedImageBackingAHB(const Mailbox& mailbox,
                        viz::ResourceFormat format,
                        const gfx::Size& size,
                        const gfx::ColorSpace& color_space,
                        uint32_t usage,
                        base::android::ScopedHardwareBufferHandle handle,
                        size_t estimated_size,
                        bool is_thread_safe,
                        base::ScopedFD initial_upload_fd);

  ~SharedImageBackingAHB() override;

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  // We never generate LegacyMailboxes in threadsafe mode, so exclude this
  // function from thread safety analysis.
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager)
      NO_THREAD_SAFETY_ANALYSIS override;
  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;

  base::android::ScopedHardwareBufferHandle GetAhbHandle() const;

  bool BeginWrite(base::ScopedFD* fd_to_wait_on);
  void EndWrite(base::ScopedFD end_write_fd);
  bool BeginRead(const SharedImageRepresentation* reader,
                 base::ScopedFD* fd_to_wait_on);
  void EndRead(const SharedImageRepresentation* reader,
               base::ScopedFD end_read_fd);
  gl::GLImage* BeginOverlayAccess();
  void EndOverlayAccess();

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  std::unique_ptr<SharedImageRepresentationOverlay> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

 private:
  gles2::Texture* GenGLTexture();
  const base::android::ScopedHardwareBufferHandle hardware_buffer_handle_;

  // Not guarded by |lock_| as we do not use legacy_texture_ in threadsafe
  // mode.
  gles2::Texture* legacy_texture_ = nullptr;

  // All reads and writes must wait for exiting writes to complete.
  base::ScopedFD write_sync_fd_ GUARDED_BY(lock_);
  bool is_writing_ GUARDED_BY(lock_) = false;

  // All writes must wait for existing reads to complete.
  base::ScopedFD read_sync_fd_ GUARDED_BY(lock_);
  base::flat_set<const SharedImageRepresentation*> active_readers_
      GUARDED_BY(lock_);

  scoped_refptr<OverlayImage> overlay_image_ GUARDED_BY(lock_);
  bool is_overlay_accessing_ GUARDED_BY(lock_) = false;

  DISALLOW_COPY_AND_ASSIGN(SharedImageBackingAHB);
};

// Representation of a SharedImageBackingAHB as a GL Texture.
class SharedImageRepresentationGLTextureAHB
    : public SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureAHB(SharedImageManager* manager,
                                        SharedImageBacking* backing,
                                        MemoryTypeTracker* tracker,
                                        gles2::Texture* texture)
      : SharedImageRepresentationGLTexture(manager, backing, tracker),
        texture_(texture) {}

  ~SharedImageRepresentationGLTextureAHB() override {
    EndAccess();

    if (texture_)
      texture_->RemoveLightweightRef(has_context());
  }

  gles2::Texture* GetTexture() override { return texture_; }

  bool BeginAccess(GLenum mode) override {
    if (mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM) {
      base::ScopedFD write_sync_fd;
      if (!ahb_backing()->BeginRead(this, &write_sync_fd))
        return false;
      if (!InsertEglFenceAndWait(std::move(write_sync_fd)))
        return false;
    } else if (mode == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM) {
      base::ScopedFD sync_fd;
      if (!ahb_backing()->BeginWrite(&sync_fd))
        return false;

      if (!InsertEglFenceAndWait(std::move(sync_fd)))
        return false;
    }

    if (mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM) {
      mode_ = RepresentationAccessMode::kRead;
    } else if (mode == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM) {
      mode_ = RepresentationAccessMode::kWrite;
    }
    return true;
  }

  void EndAccess() override {
    if (mode_ == RepresentationAccessMode::kNone)
      return;

    base::ScopedFD sync_fd = CreateEglFenceAndExportFd();
    // Pass this fd to its backing.
    if (mode_ == RepresentationAccessMode::kRead) {
      ahb_backing()->EndRead(this, std::move(sync_fd));
    } else if (mode_ == RepresentationAccessMode::kWrite) {
      ahb_backing()->EndWrite(std::move(sync_fd));
    }

    mode_ = RepresentationAccessMode::kNone;
  }

 private:
  SharedImageBackingAHB* ahb_backing() {
    return static_cast<SharedImageBackingAHB*>(backing());
  }

  gles2::Texture* texture_;
  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;
  DISALLOW_COPY_AND_ASSIGN(SharedImageRepresentationGLTextureAHB);
};

// Vk backed Skia representation of SharedImageBackingAHB.
class SharedImageRepresentationSkiaVkAHB
    : public SharedImageRepresentationSkia {
 public:
  SharedImageRepresentationSkiaVkAHB(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      scoped_refptr<SharedContextState> context_state,
      std::unique_ptr<VulkanImage> vulkan_image,
      MemoryTypeTracker* tracker)
      : SharedImageRepresentationSkia(manager, backing, tracker),
        vulkan_image_(std::move(vulkan_image)),
        context_state_(std::move(context_state)) {
    DCHECK(vulkan_image_);
    DCHECK(context_state_);
    DCHECK(context_state_->vk_context_provider());
    // TODO(bsalomon): Determine whether it makes sense to attempt to reuse this
    // if the vk_info stays the same on subsequent calls.
    promise_texture_ = SkPromiseImageTexture::Make(
        GrBackendTexture(size().width(), size().height(),
                         CreateGrVkImageInfo(vulkan_image_.get())));
    DCHECK(promise_texture_);
  }

  ~SharedImageRepresentationSkiaVkAHB() override {
    DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
    surface_.reset();
    DCHECK(vulkan_image_);
    VulkanFenceHelper* fence_helper = context_state_->vk_context_provider()
                                          ->GetDeviceQueue()
                                          ->GetFenceHelper();
    fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(vulkan_image_));
  }

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    DCHECK_EQ(mode_, RepresentationAccessMode::kNone);

    if (!BeginAccess(false /* readonly */, begin_semaphores, end_semaphores))
      return nullptr;

    auto* gr_context = context_state_->gr_context();
    if (gr_context->abandoned()) {
      LOG(ERROR) << "GrContext is abandoned.";
      return nullptr;
    }

    if (!surface_ || final_msaa_count != surface_msaa_count_ ||
        surface_props != surface_->props()) {
      SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
          /*gpu_compositing=*/true, format());
      surface_ = SkSurface::MakeFromBackendTextureAsRenderTarget(
          gr_context, promise_texture_->backendTexture(),
          kTopLeft_GrSurfaceOrigin, final_msaa_count, sk_color_type,
          color_space().ToSkColorSpace(), &surface_props);
      if (!surface_) {
        LOG(ERROR) << "MakeFromBackendTextureAsRenderTarget() failed.";
        return nullptr;
      }
      surface_msaa_count_ = final_msaa_count;
    }
    return surface_;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override {
    DCHECK_EQ(mode_, RepresentationAccessMode::kWrite);
    DCHECK_EQ(surface.get(), surface_.get());

    surface.reset();
    DCHECK(surface_->unique());
    // TODO(penghuang): reset canvas cached in |surface_|, when skia provides an
    // API to do it.
    // Currently, the |surface_| is only used with SkSurface::draw(ddl), it
    // doesn't create a canvas and change the state of it, so we don't get any
    // render issues. But we shouldn't assume this backing will only be used in
    // this way.
    EndAccess(false /* readonly */);
  }

  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    DCHECK_EQ(mode_, RepresentationAccessMode::kNone);
    DCHECK(!surface_);

    if (!BeginAccess(true /* readonly */, begin_semaphores, end_semaphores))
      return nullptr;
    return promise_texture_;
  }

  void EndReadAccess() override {
    DCHECK_EQ(mode_, RepresentationAccessMode::kRead);
    DCHECK(!surface_);

    EndAccess(true /* readonly */);
  }

 private:
  SharedImageBackingAHB* ahb_backing() {
    return static_cast<SharedImageBackingAHB*>(backing());
  }

  gpu::VulkanImplementation* vk_implementation() {
    return context_state_->vk_context_provider()->GetVulkanImplementation();
  }

  VkDevice vk_device() {
    return context_state_->vk_context_provider()
        ->GetDeviceQueue()
        ->GetVulkanDevice();
  }

  VkPhysicalDevice vk_phy_device() {
    return context_state_->vk_context_provider()
        ->GetDeviceQueue()
        ->GetVulkanPhysicalDevice();
  }

  VkQueue vk_queue() {
    return context_state_->vk_context_provider()
        ->GetDeviceQueue()
        ->GetVulkanQueue();
  }

  bool BeginAccess(bool readonly,
                   std::vector<GrBackendSemaphore>* begin_semaphores,
                   std::vector<GrBackendSemaphore>* end_semaphores) {
    DCHECK(begin_semaphores);
    DCHECK(end_semaphores);
    DCHECK(end_access_semaphore_ == VK_NULL_HANDLE);

    // Synchronise the read access with the writes.
    base::ScopedFD sync_fd;
    if (readonly) {
      if (!ahb_backing()->BeginRead(this, &sync_fd))
        return false;
    } else {
      if (!ahb_backing()->BeginWrite(&sync_fd))
        return false;
    }

    VkSemaphore begin_access_semaphore = VK_NULL_HANDLE;
    if (sync_fd.is_valid()) {
      begin_access_semaphore = vk_implementation()->ImportSemaphoreHandle(
          vk_device(),
          SemaphoreHandle(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
                          std::move(sync_fd)));
      if (begin_access_semaphore == VK_NULL_HANDLE) {
        DLOG(ERROR) << "Failed to import semaphore from sync_fd.";
        return false;
      }
    }

    end_access_semaphore_ =
        vk_implementation()->CreateExternalSemaphore(vk_device());

    if (end_access_semaphore_ == VK_NULL_HANDLE) {
      DLOG(ERROR) << "Failed to create the external semaphore.";
      if (begin_access_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(vk_device(), begin_access_semaphore,
                           nullptr /* pAllocator */);
      }
      return false;
    }

    if (begin_access_semaphore != VK_NULL_HANDLE) {
      begin_semaphores->emplace_back();
      begin_semaphores->back().initVulkan(begin_access_semaphore);
    }
    end_semaphores->emplace_back();
    end_semaphores->back().initVulkan(end_access_semaphore_);

    mode_ = readonly ? RepresentationAccessMode::kRead
                     : RepresentationAccessMode::kWrite;
    return true;
  }

  void EndAccess(bool readonly) {
    // There should be a surface_ from the BeginWriteAccess().
    DCHECK(end_access_semaphore_ != VK_NULL_HANDLE);

    SemaphoreHandle semaphore_handle = vk_implementation()->GetSemaphoreHandle(
        vk_device(), end_access_semaphore_);
    auto sync_fd = semaphore_handle.TakeHandle();
    DCHECK(sync_fd.is_valid());
    if (readonly)
      ahb_backing()->EndRead(this, std::move(sync_fd));
    else
      ahb_backing()->EndWrite(std::move(sync_fd));
    VulkanFenceHelper* fence_helper = context_state_->vk_context_provider()
                                          ->GetDeviceQueue()
                                          ->GetFenceHelper();
    fence_helper->EnqueueSemaphoreCleanupForSubmittedWork(
        end_access_semaphore_);
    end_access_semaphore_ = VK_NULL_HANDLE;
    mode_ = RepresentationAccessMode::kNone;
  }

  std::unique_ptr<VulkanImage> vulkan_image_;
  sk_sp<SkPromiseImageTexture> promise_texture_;
  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;
  int surface_msaa_count_ = 0;
  sk_sp<SkSurface> surface_;
  scoped_refptr<SharedContextState> context_state_;
  VkSemaphore end_access_semaphore_ = VK_NULL_HANDLE;
};

class SharedImageRepresentationOverlayAHB
    : public SharedImageRepresentationOverlay {
 public:
  SharedImageRepresentationOverlayAHB(SharedImageManager* manager,
                                      SharedImageBacking* backing,
                                      MemoryTypeTracker* tracker)
      : SharedImageRepresentationOverlay(manager, backing, tracker) {}

  ~SharedImageRepresentationOverlayAHB() override { EndReadAccess(); }

 private:
  SharedImageBackingAHB* ahb_backing() {
    return static_cast<SharedImageBackingAHB*>(backing());
  }

  void NotifyOverlayPromotion(bool promotion,
                              const gfx::Rect& bounds) override {
    NOTREACHED();
  }

  bool BeginReadAccess() override {
    gl_image_ = ahb_backing()->BeginOverlayAccess();
    return !!gl_image_;
  }

  void EndReadAccess() override {
    if (gl_image_) {
      ahb_backing()->EndOverlayAccess();
      gl_image_ = nullptr;
    }
  }

  gl::GLImage* GetGLImage() override { return gl_image_; }

  gl::GLImage* gl_image_ = nullptr;
};

SharedImageBackingAHB::SharedImageBackingAHB(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    base::android::ScopedHardwareBufferHandle handle,
    size_t estimated_size,
    bool is_thread_safe,
    base::ScopedFD initial_upload_fd)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      usage,
                                      estimated_size,
                                      is_thread_safe),
      hardware_buffer_handle_(std::move(handle)),
      write_sync_fd_(std::move(initial_upload_fd)) {
  DCHECK(hardware_buffer_handle_.is_valid());
}

SharedImageBackingAHB::~SharedImageBackingAHB() {
  // Locking here in destructor since we are accessing member variable
  // |have_context_| via have_context().
  AutoLock auto_lock(this);
  DCHECK(hardware_buffer_handle_.is_valid());
  if (legacy_texture_) {
    legacy_texture_->RemoveLightweightRef(have_context());
    legacy_texture_ = nullptr;
  }
}

gfx::Rect SharedImageBackingAHB::ClearedRect() const {
  AutoLock auto_lock(this);
  // If a |legacy_texture_| exists, defer to that. Once created,
  // |legacy_texture_| is never destroyed, so no need to synchronize with
  // ClearedRectInternal.
  if (legacy_texture_) {
    return legacy_texture_->GetLevelClearedRect(legacy_texture_->target(), 0);
  } else {
    return ClearedRectInternal();
  }
}

void SharedImageBackingAHB::SetClearedRect(const gfx::Rect& cleared_rect) {
  AutoLock auto_lock(this);
  // If a |legacy_texture_| exists, defer to that. Once created,
  // |legacy_texture_| is never destroyed, so no need to synchronize with
  // SetClearedRectInternal.
  if (legacy_texture_) {
    legacy_texture_->SetLevelClearedRect(legacy_texture_->target(), 0,
                                         cleared_rect);
  } else {
    SetClearedRectInternal(cleared_rect);
  }
}

void SharedImageBackingAHB::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
}

bool SharedImageBackingAHB::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  // Legacy mailboxes cannot be used safely in threadsafe mode.
  if (is_thread_safe())
    return false;

  // This doesn't need to take a lock because it is only called at creation
  // time.
  DCHECK(!is_writing_);
  DCHECK_EQ(size_t{0}, active_readers_.size());
  DCHECK(hardware_buffer_handle_.is_valid());
  legacy_texture_ = GenGLTexture();
  if (!legacy_texture_)
    return false;
  // Make sure our |legacy_texture_| has the right initial cleared rect.
  legacy_texture_->SetLevelClearedRect(legacy_texture_->target(), 0,
                                       ClearedRectInternal());
  mailbox_manager->ProduceTexture(mailbox(), legacy_texture_);
  return true;
}

base::android::ScopedHardwareBufferHandle SharedImageBackingAHB::GetAhbHandle()
    const {
  return hardware_buffer_handle_.Clone();
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBackingAHB::ProduceGLTexture(SharedImageManager* manager,
                                        MemoryTypeTracker* tracker) {
  // Use same texture for all the texture representations generated from same
  // backing.
  auto* texture = GenGLTexture();
  if (!texture)
    return nullptr;

  return std::make_unique<SharedImageRepresentationGLTextureAHB>(
      manager, this, tracker, std::move(texture));
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingAHB::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  DCHECK(context_state);

  // Check whether we are in Vulkan mode OR GL mode and accordingly create
  // Skia representation.
  if (context_state->GrContextIsVulkan()) {
    auto* device_queue = context_state->vk_context_provider()->GetDeviceQueue();
    gfx::GpuMemoryBufferHandle gmb_handle(GetAhbHandle());
    auto vulkan_image = VulkanImage::CreateFromGpuMemoryBufferHandle(
        device_queue, std::move(gmb_handle), size(), ToVkFormat(format()),
        0 /* usage */);
    if (!vulkan_image)
      return nullptr;

    return std::make_unique<SharedImageRepresentationSkiaVkAHB>(
        manager, this, std::move(context_state), std::move(vulkan_image),
        tracker);
  }
  DCHECK(context_state->GrContextIsGL());

  auto* texture = GenGLTexture();
  if (!texture)
    return nullptr;
  auto gl_representation =
      std::make_unique<SharedImageRepresentationGLTextureAHB>(
          manager, this, tracker, std::move(texture));
  return SharedImageRepresentationSkiaGL::Create(std::move(gl_representation),
                                                 std::move(context_state),
                                                 manager, this, tracker);
}

std::unique_ptr<SharedImageRepresentationOverlay>
SharedImageBackingAHB::ProduceOverlay(SharedImageManager* manager,
                                      MemoryTypeTracker* tracker) {
  return std::make_unique<SharedImageRepresentationOverlayAHB>(manager, this,
                                                               tracker);
}

bool SharedImageBackingAHB::BeginWrite(base::ScopedFD* fd_to_wait_on) {
  AutoLock auto_lock(this);

  if (is_writing_ || !active_readers_.empty() || is_overlay_accessing_) {
    LOG(ERROR) << "BeginWrite should only be called when there are no other "
                  "readers or writers";
    return false;
  }

  is_writing_ = true;
  (*fd_to_wait_on) =
      gl::MergeFDs(std::move(read_sync_fd_), std::move(write_sync_fd_));

  return true;
}

void SharedImageBackingAHB::EndWrite(base::ScopedFD end_write_fd) {
  AutoLock auto_lock(this);

  if (!is_writing_) {
    LOG(ERROR) << "Attempt to end write to a SharedImageBacking without a "
                  "successful begin write";
    return;
  }

  is_writing_ = false;

  write_sync_fd_ = std::move(end_write_fd);
}

bool SharedImageBackingAHB::BeginRead(const SharedImageRepresentation* reader,
                                      base::ScopedFD* fd_to_wait_on) {
  AutoLock auto_lock(this);

  if (is_writing_) {
    LOG(ERROR) << "BeginRead should only be called when there are no writers";
    return false;
  }

  if (active_readers_.contains(reader)) {
    LOG(ERROR) << "BeginRead was called twice on the same representation";
    return false;
  }

  active_readers_.insert(reader);
  if (write_sync_fd_.is_valid()) {
    (*fd_to_wait_on) = base::ScopedFD(HANDLE_EINTR(dup(write_sync_fd_.get())));
  } else {
    // TODO(cblume): Clear the backing
    (*fd_to_wait_on) = base::ScopedFD{};
  }

  return true;
}

void SharedImageBackingAHB::EndRead(const SharedImageRepresentation* reader,
                                    base::ScopedFD end_read_fd) {
  AutoLock auto_lock(this);

  if (!active_readers_.contains(reader)) {
    LOG(ERROR) << "Attempt to end read to a SharedImageBacking without a "
                  "successful begin read";
    return;
  }

  active_readers_.erase(reader);

  read_sync_fd_ =
      gl::MergeFDs(std::move(read_sync_fd_), std::move(end_read_fd));
}

gl::GLImage* SharedImageBackingAHB::BeginOverlayAccess() {
  AutoLock auto_lock(this);

  DCHECK(!is_overlay_accessing_);

  if (is_writing_) {
    LOG(ERROR)
        << "BeginOverlayAccess should only be called when there are no writers";
    return nullptr;
  }

  if (!overlay_image_) {
    overlay_image_ =
        base::MakeRefCounted<OverlayImage>(hardware_buffer_handle_.get());
    overlay_image_->SetColorSpace(color_space());
  }

  if (write_sync_fd_.is_valid()) {
    base::ScopedFD fence_fd(HANDLE_EINTR(dup(write_sync_fd_.get())));
    overlay_image_->SetBeginFence(std::move(fence_fd));
  }

  is_overlay_accessing_ = true;
  return overlay_image_.get();
}

void SharedImageBackingAHB::EndOverlayAccess() {
  AutoLock auto_lock(this);

  DCHECK(is_overlay_accessing_);
  is_overlay_accessing_ = false;

  auto fence_fd = overlay_image_->TakeEndFence();
  read_sync_fd_ = gl::MergeFDs(std::move(read_sync_fd_), std::move(fence_fd));
}

gles2::Texture* SharedImageBackingAHB::GenGLTexture() {
  DCHECK(hardware_buffer_handle_.is_valid());

  // Target for AHB backed egl images.
  // Note that we are not using GL_TEXTURE_EXTERNAL_OES target since sksurface
  // doesn't supports it. As per the egl documentation -
  // https://www.khronos.org/registry/OpenGL/extensions/OES/OES_EGL_image_external.txt
  // if GL_OES_EGL_image is supported then <target> may also be TEXTURE_2D.
  GLenum target = GL_TEXTURE_2D;
  GLenum get_target = GL_TEXTURE_BINDING_2D;

  // Create a gles2 texture using the AhardwareBuffer.
  gl::GLApi* api = gl::g_current_gl_context;
  GLuint service_id = 0;
  api->glGenTexturesFn(1, &service_id);
  GLint old_texture_binding = 0;
  api->glGetIntegervFn(get_target, &old_texture_binding);
  api->glBindTextureFn(target, service_id);
  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Create an egl image using AHardwareBuffer.
  auto egl_image = base::MakeRefCounted<gl::GLImageAHardwareBuffer>(size());
  if (!egl_image->Initialize(hardware_buffer_handle_.get(), false)) {
    LOG(ERROR) << "Failed to create EGL image";
    api->glBindTextureFn(target, old_texture_binding);
    api->glDeleteTexturesFn(1, &service_id);
    return nullptr;
  }
  if (!egl_image->BindTexImage(target)) {
    LOG(ERROR) << "Failed to bind egl image";
    api->glBindTextureFn(target, old_texture_binding);
    api->glDeleteTexturesFn(1, &service_id);
    return nullptr;
  }
  egl_image->SetColorSpace(color_space());

  // Create a gles2 Texture.
  auto* texture = new gles2::Texture(service_id);
  texture->SetLightweightRef();
  texture->SetTarget(target, 1);
  texture->sampler_state_.min_filter = GL_LINEAR;
  texture->sampler_state_.mag_filter = GL_LINEAR;
  texture->sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;
  texture->sampler_state_.wrap_s = GL_CLAMP_TO_EDGE;

  texture->SetLevelInfo(target, 0, egl_image->GetInternalFormat(),
                        size().width(), size().height(), 1, 0,
                        egl_image->GetDataFormat(), egl_image->GetDataType(),
                        ClearedRect());
  texture->SetLevelImage(target, 0, egl_image.get(), gles2::Texture::BOUND);
  texture->SetImmutable(true, false);
  api->glBindTextureFn(target, old_texture_binding);
  return texture;
}

SharedImageBackingFactoryAHB::SharedImageBackingFactoryAHB(
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info) {
  scoped_refptr<gles2::FeatureInfo> feature_info =
      new gles2::FeatureInfo(workarounds, gpu_feature_info);
  feature_info->Initialize(ContextType::CONTEXT_TYPE_OPENGLES2, false,
                           gles2::DisallowedFeatures());
  const gles2::Validators* validators = feature_info->validators();
  const bool is_egl_image_supported =
      gl::g_current_gl_driver->ext.b_GL_OES_EGL_image;

  // Build the feature info for all the resource formats.
  for (int i = 0; i <= viz::RESOURCE_FORMAT_MAX; ++i) {
    auto format = static_cast<viz::ResourceFormat>(i);
    FormatInfo& info = format_info_[i];

    // If AHB does not support this format, we will not be able to create this
    // backing.
    if (!AHardwareBufferSupportedFormat(format))
      continue;

    info.ahb_supported = true;
    info.ahb_format = AHardwareBufferFormat(format);

    // TODO(vikassoni): In future when we use GL_TEXTURE_EXTERNAL_OES target
    // with AHB, we need to check if oes_egl_image_external is supported or
    // not.
    if (!is_egl_image_supported)
      continue;

    // Check if AHB backed GL texture can be created using this format and
    // gather GL related format info.
    // TODO(vikassoni): Add vulkan related information in future.
    GLuint internal_format = viz::GLInternalFormat(format);
    GLenum gl_format = viz::GLDataFormat(format);
    GLenum gl_type = viz::GLDataType(format);

    //  GLImageAHardwareBuffer supports internal format GL_RGBA and GL_RGB.
    if (internal_format != GL_RGBA && internal_format != GL_RGB)
      continue;

    // Validate if GL format, type and internal format is supported.
    if (validators->texture_internal_format.IsValid(internal_format) &&
        validators->texture_format.IsValid(gl_format) &&
        validators->pixel_type.IsValid(gl_type)) {
      info.gl_supported = true;
      info.gl_format = gl_format;
      info.gl_type = gl_type;
      info.internal_format = internal_format;
    }
  }
  // TODO(vikassoni): We are using below GL api calls for now as Vulkan mode
  // doesn't exist. Once we have vulkan support, we shouldn't query GL in this
  // code until we are asked to make a GL representation (or allocate a
  // backing for import into GL)? We may use an AHardwareBuffer exclusively
  // with Vulkan, where there is no need to require that a GL context is
  // current. Maybe we can lazy init this if someone tries to create an
  // AHardwareBuffer with SHARED_IMAGE_USAGE_GLES2 ||
  // !gpu_preferences.enable_vulkan. When in Vulkan mode, we should only need
  // this with GLES2.
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGetIntegervFn(GL_MAX_TEXTURE_SIZE, &max_gl_texture_size_);

  // TODO(vikassoni): Check vulkan image size restrictions also.
  if (workarounds.max_texture_size) {
    max_gl_texture_size_ =
        std::min(max_gl_texture_size_, workarounds.max_texture_size);
  }
  // Ensure max_texture_size_ is less than INT_MAX so that gfx::Rect and friends
  // can be used to accurately represent all valid sub-rects, with overflow
  // cases, clamped to INT_MAX, always invalid.
  max_gl_texture_size_ = std::min(max_gl_texture_size_, INT_MAX - 1);
}

SharedImageBackingFactoryAHB::~SharedImageBackingFactoryAHB() = default;

bool SharedImageBackingFactoryAHB::ValidateUsage(
    uint32_t usage,
    const gfx::Size& size,
    viz::ResourceFormat format) const {
  const FormatInfo& format_info = format_info_[format];

  // Check if the format is supported by AHardwareBuffer.
  if (!format_info.ahb_supported) {
    LOG(ERROR) << "viz::ResourceFormat " << format
               << " not supported by AHardwareBuffer";
    return false;
  }

  // SHARED_IMAGE_USAGE_RASTER is set when we want to write on Skia
  // representation and SHARED_IMAGE_USAGE_DISPLAY is used for cases we want
  // to read from skia representation.
  // TODO(vikassoni): Also check gpu_preferences.enable_vulkan to figure out
  // if skia is using vulkan backing or GL backing.
  const bool use_gles2 =
      (usage & (SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_DISPLAY));

  // If usage flags indicated this backing can be used as a GL texture, then
  // do below gl related checks.
  if (use_gles2) {
    // Check if the GL texture can be created from AHB with this format.
    if (!format_info.gl_supported) {
      LOG(ERROR)
          << "viz::ResourceFormat " << format
          << " can not be used to create a GL texture from AHardwareBuffer.";
      return false;
    }
  }

  // Check if AHB can be created with the current size restrictions.
  // TODO(vikassoni): Check for VK size restrictions for VK import, GL size
  // restrictions for GL import OR both if this backing is needed to be used
  // with both GL and VK.
  if (size.width() < 1 || size.height() < 1 ||
      size.width() > max_gl_texture_size_ ||
      size.height() > max_gl_texture_size_) {
    LOG(ERROR) << "CreateSharedImage: invalid size";
    return false;
  }

  return true;
}

std::unique_ptr<SharedImageBacking> SharedImageBackingFactoryAHB::MakeBacking(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    bool is_thread_safe,
    base::span<const uint8_t> pixel_data) {
  DCHECK(base::AndroidHardwareBufferCompat::IsSupportAvailable());
  DCHECK(format != viz::ETC1);

  if (!ValidateUsage(usage, size, format)) {
    return nullptr;
  }

  // Calculate SharedImage size in bytes.
  size_t estimated_size;
  if (!viz::ResourceSizes::MaybeSizeInBytes(size, format, &estimated_size)) {
    LOG(ERROR) << "Failed to calculate SharedImage size";
    return nullptr;
  }

  const FormatInfo& format_info = format_info_[format];

  // Setup AHardwareBuffer.
  AHardwareBuffer* buffer = nullptr;
  AHardwareBuffer_Desc hwb_desc;
  hwb_desc.width = size.width();
  hwb_desc.height = size.height();
  hwb_desc.format = format_info.ahb_format;

  // Set usage so that gpu can both read as a texture/write as a framebuffer
  // attachment. TODO(vikassoni): Find out if we need to set some more usage
  // flags based on the usage params in the current function call.
  hwb_desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                   AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
  if (usage & SHARED_IMAGE_USAGE_SCANOUT)
    hwb_desc.usage |= gl::SurfaceControl::RequiredUsage();

  // Add WRITE usage as we'll it need to upload data
  if (!pixel_data.empty())
    hwb_desc.usage |= AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY;

  // Number of images in an image array.
  hwb_desc.layers = 1;

  // The following three are not used here.
  hwb_desc.stride = 0;
  hwb_desc.rfu0 = 0;
  hwb_desc.rfu1 = 0;

  // Allocate an AHardwareBuffer.
  base::AndroidHardwareBufferCompat::GetInstance().Allocate(&hwb_desc, &buffer);
  if (!buffer) {
    LOG(ERROR) << "Failed to allocate AHardwareBuffer";
    return nullptr;
  }

  auto handle = base::android::ScopedHardwareBufferHandle::Adopt(buffer);

  base::ScopedFD initial_upload_fd;
  // Upload data if necessary
  if (!pixel_data.empty()) {
    // Get description about buffer to obtain stride
    AHardwareBuffer_Desc hwb_info;
    base::AndroidHardwareBufferCompat::GetInstance().Describe(buffer,
                                                              &hwb_info);
    void* address = nullptr;
    if (int error = base::AndroidHardwareBufferCompat::GetInstance().Lock(
            buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY, -1, 0, &address)) {
      LOG(ERROR) << "Failed to lock AHardwareBuffer: " << error;
      return nullptr;
    }

    int bytes_per_pixel = BitsPerPixel(format) / 8;

    // NOTE: hwb_info.stride is in pixels
    int dst_stride = bytes_per_pixel * hwb_info.stride;
    int src_stride = bytes_per_pixel * size.width();

    for (int y = 0; y < size.height(); y++) {
      void* dst = reinterpret_cast<uint8_t*>(address) + dst_stride * y;
      const void* src = pixel_data.data() + src_stride * y;

      memcpy(dst, src, src_stride);
    }

    int32_t fence = -1;
    base::AndroidHardwareBufferCompat::GetInstance().Unlock(buffer, &fence);
    initial_upload_fd = base::ScopedFD(fence);
  }

  auto backing = std::make_unique<SharedImageBackingAHB>(
      mailbox, format, size, color_space, usage, std::move(handle),
      estimated_size, is_thread_safe, std::move(initial_upload_fd));

  // If we uploaded initial data, set the backing as cleared.
  if (!pixel_data.empty())
    backing->SetCleared();

  return backing;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryAHB::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    bool is_thread_safe) {
  return MakeBacking(mailbox, format, size, color_space, usage, is_thread_safe,
                     base::span<uint8_t>());
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryAHB::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  auto backing =
      MakeBacking(mailbox, format, size, color_space, usage, false, pixel_data);
  if (backing)
    backing->OnWriteSucceeded();
  return backing;
}

bool SharedImageBackingFactoryAHB::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return memory_buffer_type == gfx::ANDROID_HARDWARE_BUFFER;
}

bool SharedImageBackingFactoryAHB::IsFormatSupported(
    viz::ResourceFormat format) {
  DCHECK_GE(format, 0);
  DCHECK_LE(format, viz::RESOURCE_FORMAT_MAX);

  return format_info_[format].ahb_supported;
}

SharedImageBackingFactoryAHB::FormatInfo::FormatInfo() = default;
SharedImageBackingFactoryAHB::FormatInfo::~FormatInfo() = default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryAHB::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  // TODO(vasilyt): support SHARED_MEMORY_BUFFER?
  if (handle.type != gfx::ANDROID_HARDWARE_BUFFER) {
    NOTIMPLEMENTED();
    return nullptr;
  }

  auto resource_format = viz::GetResourceFormat(buffer_format);

  if (!ValidateUsage(usage, size, resource_format)) {
    return nullptr;
  }

  size_t estimated_size;
  if (!viz::ResourceSizes::MaybeSizeInBytes(size, resource_format,
                                            &estimated_size)) {
    LOG(ERROR) << "Failed to calculate SharedImage size";
    return nullptr;
  }

  return std::make_unique<SharedImageBackingAHB>(
      mailbox, resource_format, size, color_space, usage,
      std::move(handle.android_hardware_buffer), estimated_size, false,
      base::ScopedFD());
}

}  // namespace gpu
