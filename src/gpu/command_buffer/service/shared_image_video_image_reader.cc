// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_video_image_reader.h"

#include <utility>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/task/bind_post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image_representation_skia_gl.h"
#include "gpu/command_buffer/service/shared_image_representation_skia_vk_android.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/texture_owner.h"
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
#include "ui/gl/gl_image_ahardwarebuffer.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

namespace {
void CreateAndBindEglImageFromAHB(AHardwareBuffer* buffer, GLuint service_id) {
  DCHECK(buffer);

  AHardwareBuffer_Desc desc;
  base::AndroidHardwareBufferCompat::GetInstance().Describe(buffer, &desc);
  auto egl_image = base::MakeRefCounted<gl::GLImageAHardwareBuffer>(
      gfx::Size(desc.width, desc.height));
  if (egl_image->Initialize(buffer, false)) {
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, service_id);
    egl_image->BindTexImage(GL_TEXTURE_EXTERNAL_OES);
  } else {
    LOG(ERROR) << "Failed to create EGL image ";
  }
}

class VideoImage : public gl::GLImage {
 public:
  VideoImage() = default;

  VideoImage(AHardwareBuffer* buffer, base::ScopedFD begin_read_fence)
      : handle_(base::android::ScopedHardwareBufferHandle::Create(buffer)),
        begin_read_fence_(std::move(begin_read_fence)) {}

  // gl::GLImage:
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override {
    if (!handle_.is_valid())
      return nullptr;

    return std::make_unique<ScopedHardwareBufferFenceSyncImpl>(
        this, base::android::ScopedHardwareBufferHandle::Create(handle_.get()),
        std::move(begin_read_fence_));
  }

  base::ScopedFD TakeEndReadFence() { return std::move(end_read_fence_); }

 protected:
  ~VideoImage() override = default;

 private:
  class ScopedHardwareBufferFenceSyncImpl
      : public base::android::ScopedHardwareBufferFenceSync {
   public:
    ScopedHardwareBufferFenceSyncImpl(
        scoped_refptr<VideoImage> image,
        base::android::ScopedHardwareBufferHandle handle,
        base::ScopedFD fence_fd)
        : ScopedHardwareBufferFenceSync(std::move(handle),
                                        std::move(fence_fd),
                                        base::ScopedFD(),
                                        /*is_video=*/true),
          image_(std::move(image)) {}
    ~ScopedHardwareBufferFenceSyncImpl() override = default;

    void SetReadFence(base::ScopedFD fence_fd, bool has_context) override {
      image_->end_read_fence_ =
          gl::MergeFDs(std::move(image_->end_read_fence_), std::move(fence_fd));
    }

   private:
    scoped_refptr<VideoImage> image_;
  };

  base::android::ScopedHardwareBufferHandle handle_;

  // This fence should be waited upon before reading from the buffer.
  base::ScopedFD begin_read_fence_;

  // This fence should be waited upon to ensure that the reader is finished
  // reading from the buffer.
  base::ScopedFD end_read_fence_;
};

}  // namespace

SharedImageVideoImageReader::SharedImageVideoImageReader(
    const Mailbox& mailbox,
    const gfx::Size& size,
    const gfx::ColorSpace color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
    scoped_refptr<SharedContextState> context_state,
    scoped_refptr<RefCountedLock> drdc_lock)
    : SharedImageVideo(mailbox,
                       size,
                       color_space,
                       surface_origin,
                       alpha_type,
                       !!drdc_lock),
      RefCountedLockHelperDrDc(std::move(drdc_lock)),
      stream_texture_sii_(std::move(stream_texture_sii)),
      gpu_main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(stream_texture_sii_);

  context_lost_helper_ = std::make_unique<ContextLostObserverHelper>(
      std::move(context_state), stream_texture_sii_, gpu_main_task_runner_,
      GetDrDcLock());
}

SharedImageVideoImageReader::~SharedImageVideoImageReader() {
  // This backing is created on gpu main thread but can be destroyed on DrDc
  // thread if the last representation was on DrDc thread.
  // |context_lost_helper_| is destroyed here by posting task to the
  // |gpu_main_thread_| to ensure that resources are cleaned up correvtly on
  // the gpu main thread.
  if (!gpu_main_task_runner_->RunsTasksInCurrentSequence()) {
    auto helper_destruction_cb = base::BindPostTask(
        gpu_main_task_runner_,
        base::BindOnce(
            [](std::unique_ptr<ContextLostObserverHelper> context_lost_helper,
               scoped_refptr<StreamTextureSharedImageInterface>
                   stream_texture_sii) {
              // Reset the |stream_texture_sii| first so that its ref in the
              // |context_lost_helper| gets reset under the DrDc lock.
              stream_texture_sii.reset();
              context_lost_helper.reset();
            }));
    std::move(helper_destruction_cb)
        .Run(std::move(context_lost_helper_), std::move(stream_texture_sii_));
  }
}

size_t SharedImageVideoImageReader::EstimatedSizeForMemTracking() const {
  base::AutoLockMaybe auto_lock(GetDrDcLockPtr());

  // This backing contributes to gpu memory only if its bound to the texture
  // and not when the backing is created.
  return stream_texture_sii_->IsUsingGpuMemory() ? estimated_size() : 0;
}

std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
SharedImageVideoImageReader::GetAHardwareBuffer() {
  base::AutoLockMaybe auto_lock(GetDrDcLockPtr());

  DCHECK(stream_texture_sii_);
  return stream_texture_sii_->GetAHardwareBuffer();
}

// Representation of SharedImageVideoImageReader as a GL Texture.
class SharedImageVideoImageReader::SharedImageRepresentationGLTextureVideo
    : public SharedImageRepresentationGLTexture,
      public RefCountedLockHelperDrDc {
 public:
  SharedImageRepresentationGLTextureVideo(
      SharedImageManager* manager,
      SharedImageVideoImageReader* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<gles2::AbstractTexture> texture,
      scoped_refptr<RefCountedLock> drdc_lock)
      : SharedImageRepresentationGLTexture(manager, backing, tracker),
        RefCountedLockHelperDrDc(std::move(drdc_lock)),
        texture_(std::move(texture)) {}

  // Disallow copy and assign.
  SharedImageRepresentationGLTextureVideo(
      const SharedImageRepresentationGLTextureVideo&) = delete;
  SharedImageRepresentationGLTextureVideo& operator=(
      const SharedImageRepresentationGLTextureVideo&) = delete;

  gles2::Texture* GetTexture() override {
    auto* texture = gles2::Texture::CheckedCast(texture_->GetTextureBase());
    DCHECK(texture);

    return texture;
  }

  bool BeginAccess(GLenum mode) override {
    // This representation should only be called for read.
    DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

    auto* video_backing = static_cast<SharedImageVideoImageReader*>(backing());
    {
      base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
      scoped_hardware_buffer_ =
          video_backing->stream_texture_sii_->GetAHardwareBuffer();
    }
    if (!scoped_hardware_buffer_) {
      LOG(ERROR) << "Failed to get the hardware buffer.";
      return false;
    }
    CreateAndBindEglImageFromAHB(scoped_hardware_buffer_->buffer(),
                                 texture_->service_id());
    return true;
  }

  void EndAccess() override {
    DCHECK(scoped_hardware_buffer_);

    base::ScopedFD sync_fd = CreateEglFenceAndExportFd();
    scoped_hardware_buffer_->SetReadFence(std::move(sync_fd), true);
    scoped_hardware_buffer_ = nullptr;
  }

 private:
  std::unique_ptr<gles2::AbstractTexture> texture_;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
      scoped_hardware_buffer_;
};

// Representation of SharedImageVideoImageReader as a GL Texture.
class SharedImageVideoImageReader::
    SharedImageRepresentationGLTexturePassthroughVideo
    : public SharedImageRepresentationGLTexturePassthrough,
      public RefCountedLockHelperDrDc {
 public:
  SharedImageRepresentationGLTexturePassthroughVideo(
      SharedImageManager* manager,
      SharedImageVideoImageReader* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<gles2::AbstractTexture> abstract_texture,
      scoped_refptr<RefCountedLock> drdc_lock)
      : SharedImageRepresentationGLTexturePassthrough(manager,
                                                      backing,
                                                      tracker),
        RefCountedLockHelperDrDc(std::move(drdc_lock)),
        abstract_texture_(std::move(abstract_texture)),
        passthrough_texture_(gles2::TexturePassthrough::CheckedCast(
            abstract_texture_->GetTextureBase())) {
    // TODO(https://crbug.com/1172769): Remove this CHECK.
    CHECK(passthrough_texture_);
  }

  // Disallow copy and assign.
  SharedImageRepresentationGLTexturePassthroughVideo(
      const SharedImageRepresentationGLTexturePassthroughVideo&) = delete;
  SharedImageRepresentationGLTexturePassthroughVideo& operator=(
      const SharedImageRepresentationGLTexturePassthroughVideo&) = delete;

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override {
    return passthrough_texture_;
  }

  bool BeginAccess(GLenum mode) override {
    // This representation should only be called for read.
    DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

    auto* video_backing = static_cast<SharedImageVideoImageReader*>(backing());

    {
      base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
      scoped_hardware_buffer_ =
          video_backing->stream_texture_sii_->GetAHardwareBuffer();
    }
    if (!scoped_hardware_buffer_) {
      LOG(ERROR) << "Failed to get the hardware buffer.";
      return false;
    }
    CreateAndBindEglImageFromAHB(scoped_hardware_buffer_->buffer(),
                                 passthrough_texture_->service_id());
    return true;
  }

  void EndAccess() override {
    DCHECK(scoped_hardware_buffer_);

    base::ScopedFD sync_fd = CreateEglFenceAndExportFd();
    scoped_hardware_buffer_->SetReadFence(std::move(sync_fd), true);
    scoped_hardware_buffer_ = nullptr;
  }

 private:
  std::unique_ptr<gles2::AbstractTexture> abstract_texture_;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
      scoped_hardware_buffer_;
};

class SharedImageVideoImageReader::SharedImageRepresentationVideoSkiaVk
    : public SharedImageRepresentationSkiaVkAndroid,
      public RefCountedLockHelperDrDc {
 public:
  SharedImageRepresentationVideoSkiaVk(
      SharedImageManager* manager,
      SharedImageBackingAndroid* backing,
      scoped_refptr<SharedContextState> context_state,
      MemoryTypeTracker* tracker,
      scoped_refptr<RefCountedLock> drdc_lock)
      : SharedImageRepresentationSkiaVkAndroid(manager,
                                               backing,
                                               std::move(context_state),
                                               tracker),
        RefCountedLockHelperDrDc(std::move(drdc_lock)) {}

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    // Writes are not intended to used for video backed representations.
    NOTIMPLEMENTED();
    return nullptr;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override { NOTIMPLEMENTED(); }

  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());

    DCHECK(!scoped_hardware_buffer_);
    auto* video_backing = static_cast<SharedImageVideoImageReader*>(backing());
    DCHECK(video_backing);
    auto* stream_texture_sii = video_backing->stream_texture_sii_.get();

    // GetAHardwareBuffer() renders the latest image and gets AHardwareBuffer
    // from it.
    scoped_hardware_buffer_ = stream_texture_sii->GetAHardwareBuffer();
    if (!scoped_hardware_buffer_) {
      LOG(ERROR) << "Failed to get the hardware buffer.";
      return nullptr;
    }
    DCHECK(scoped_hardware_buffer_->buffer());

    // Wait on the sync fd attached to the buffer to make sure buffer is
    // ready before the read. This is done by inserting the sync fd semaphore
    // into begin_semaphore vector which client will wait on.
    init_read_fence_ = scoped_hardware_buffer_->TakeFence();

    if (!vulkan_image_) {
      DCHECK(!promise_texture_);

      vulkan_image_ = CreateVkImageFromAhbHandle(
          scoped_hardware_buffer_->TakeBuffer(), context_state(), size(),
          format(), VK_QUEUE_FAMILY_FOREIGN_EXT);
      if (!vulkan_image_)
        return nullptr;

      // We always use VK_IMAGE_TILING_OPTIMAL while creating the vk image in
      // VulkanImplementationAndroid::CreateVkImageAndImportAHB. Hence pass
      // the tiling parameter as VK_IMAGE_TILING_OPTIMAL to below call rather
      // than passing |vk_image_info.tiling|. This is also to ensure that the
      // promise image created here at [1] as well the fulfill image created
      // via the current function call are consistent and both are using
      // VK_IMAGE_TILING_OPTIMAL. [1] -
      // https://cs.chromium.org/chromium/src/components/viz/service/display_embedder/skia_output_surface_impl.cc?rcl=db5ffd448ba5d66d9d3c5c099754e5067c752465&l=789.
      DCHECK_EQ(static_cast<int32_t>(vulkan_image_->image_tiling()),
                static_cast<int32_t>(VK_IMAGE_TILING_OPTIMAL));

      // TODO(bsalomon): Determine whether it makes sense to attempt to reuse
      // this if the vk_info stays the same on subsequent calls.
      promise_texture_ = SkPromiseImageTexture::Make(
          GrBackendTexture(size().width(), size().height(),
                           CreateGrVkImageInfo(vulkan_image_.get())));
      DCHECK(promise_texture_);
    }

    return SharedImageRepresentationSkiaVkAndroid::BeginReadAccess(
        begin_semaphores, end_semaphores, end_state);
  }

  void EndReadAccess() override {
    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
    DCHECK(scoped_hardware_buffer_);

    SharedImageRepresentationSkiaVkAndroid::EndReadAccess();

    // Pass the end read access sync fd to the scoped hardware buffer. This
    // will make sure that the AImage associated with the hardware buffer will
    // be deleted only when the read access is ending.
    scoped_hardware_buffer_->SetReadFence(android_backing()->TakeReadFence(),
                                          true);
    scoped_hardware_buffer_ = nullptr;
  }

 private:
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
      scoped_hardware_buffer_;
};

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageVideoImageReader::ProduceGLTexture(SharedImageManager* manager,
                                              MemoryTypeTracker* tracker) {
  base::AutoLockMaybe auto_lock(GetDrDcLockPtr());

  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!stream_texture_sii_->HasTextureOwner())
    return nullptr;

  // Generate an abstract texture.
  auto texture = GenAbstractTexture(/*passthrough=*/false);
  if (!texture)
    return nullptr;

  return std::make_unique<SharedImageRepresentationGLTextureVideo>(
      manager, this, tracker, std::move(texture), GetDrDcLock());
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageVideoImageReader::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  base::AutoLockMaybe auto_lock(GetDrDcLockPtr());

  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!stream_texture_sii_->HasTextureOwner())
    return nullptr;

  // Generate an abstract texture.
  auto texture = GenAbstractTexture(/*passthrough=*/true);
  if (!texture)
    return nullptr;

  return std::make_unique<SharedImageRepresentationGLTexturePassthroughVideo>(
      manager, this, tracker, std::move(texture), GetDrDcLock());
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageVideoImageReader::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  base::AutoLockMaybe auto_lock(GetDrDcLockPtr());

  DCHECK(context_state);

  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!stream_texture_sii_->HasTextureOwner())
    return nullptr;

  if (context_state->GrContextIsVulkan()) {
    return std::make_unique<SharedImageRepresentationVideoSkiaVk>(
        manager, this, std::move(context_state), tracker, GetDrDcLock());
  }

  DCHECK(context_state->GrContextIsGL());
  auto* texture_base = stream_texture_sii_->GetTextureBase();
  DCHECK(texture_base);
  const bool passthrough =
      (texture_base->GetType() == gpu::TextureBase::Type::kPassthrough);

  auto texture = GenAbstractTexture(passthrough);
  if (!texture)
    return nullptr;

  std::unique_ptr<gpu::SharedImageRepresentationGLTextureBase>
      gl_representation;
  if (passthrough) {
    gl_representation =
        std::make_unique<SharedImageRepresentationGLTexturePassthroughVideo>(
            manager, this, tracker, std::move(texture), GetDrDcLock());
  } else {
    gl_representation =
        std::make_unique<SharedImageRepresentationGLTextureVideo>(
            manager, this, tracker, std::move(texture), GetDrDcLock());
  }
  return SharedImageRepresentationSkiaGL::Create(std::move(gl_representation),
                                                 std::move(context_state),
                                                 manager, this, tracker);
}

void SharedImageVideoImageReader::BeginGLReadAccess(const GLuint service_id) {
  AssertAcquiredDrDcLock();
  stream_texture_sii_->UpdateAndBindTexImage(service_id);
}

// Representation of SharedImageVideoImageReader as an overlay plane.
class SharedImageVideoImageReader::SharedImageRepresentationOverlayVideo
    : public gpu::SharedImageRepresentationOverlay,
      public RefCountedLockHelperDrDc {
 public:
  SharedImageRepresentationOverlayVideo(gpu::SharedImageManager* manager,
                                        SharedImageVideoImageReader* backing,
                                        gpu::MemoryTypeTracker* tracker,
                                        scoped_refptr<RefCountedLock> drdc_lock)
      : gpu::SharedImageRepresentationOverlay(manager, backing, tracker),
        RefCountedLockHelperDrDc(std::move(drdc_lock)) {}

  // Disallow copy and assign.
  SharedImageRepresentationOverlayVideo(
      const SharedImageRepresentationOverlayVideo&) = delete;
  SharedImageRepresentationOverlayVideo& operator=(
      const SharedImageRepresentationOverlayVideo&) = delete;

 protected:
  bool BeginReadAccess(std::vector<gfx::GpuFence>* acquire_fences) override {
    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
    // A |CodecImage| is already in a SurfaceView, render content to the
    // overlay.
    if (!stream_image()->HasTextureOwner()) {
      TRACE_EVENT0("media",
                   "SharedImageRepresentationOverlayVideo::BeginReadAccess");
      stream_image()->RenderToOverlay();
    }
    return true;
  }

  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {
    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
    DCHECK(release_fence.is_null());
    if (gl_image_) {
      if (scoped_hardware_buffer_) {
        scoped_hardware_buffer_->SetReadFence(gl_image_->TakeEndReadFence(),
                                              true);
      }
      gl_image_.reset();
      scoped_hardware_buffer_.reset();
    }
  }

  gl::GLImage* GetGLImage() override {
    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
    DCHECK(stream_image()->HasTextureOwner())
        << "The backing is already in a SurfaceView!";

    // Note that we have SurfaceView overlay as well as SurfaceControl.
    // SurfaceView may/may not have TextureOwner whereas SurfaceControl always
    // have TextureOwner. It is not possible to know whether we are in
    // SurfaceView or SurfaceControl mode in Begin/EndReadAccess. Hence
    // |scoped_hardware_buffer_| and |gl_image_| needs to be created here since
    // GetGLImage will only be called for SurfaceControl.
    if (!gl_image_) {
      scoped_hardware_buffer_ = stream_image()->GetAHardwareBuffer();

      // |scoped_hardware_buffer_| could be null for cases when a buffer is
      // not acquired in ImageReader for some reasons and there is no previously
      // acquired image left.
      if (scoped_hardware_buffer_) {
        gl_image_ = base::MakeRefCounted<VideoImage>(
            scoped_hardware_buffer_->buffer(),
            scoped_hardware_buffer_->TakeFence());
      } else {
        // Caller of GetGLImage currently do not expect a null |gl_image_|.
        // Hence creating a valid object with null buffer which results in a
        // blank video frame and is expected. TODO(vikassoni) : Explore option
        // of returning a null GLImage here.
        gl_image_ = base::MakeRefCounted<VideoImage>();
      }
      gl_image_->SetColorSpace(color_space());
    }
    return gl_image_.get();
  }

  void NotifyOverlayPromotion(bool promotion,
                              const gfx::Rect& bounds) override {
    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
    stream_image()->NotifyOverlayPromotion(promotion, bounds);
  }

 private:
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
      scoped_hardware_buffer_;
  scoped_refptr<VideoImage> gl_image_;

  StreamTextureSharedImageInterface* stream_image() {
    auto* video_backing = static_cast<SharedImageVideoImageReader*>(backing());
    DCHECK(video_backing);
    return video_backing->stream_texture_sii_.get();
  }
};

std::unique_ptr<gpu::SharedImageRepresentationOverlay>
SharedImageVideoImageReader::ProduceOverlay(gpu::SharedImageManager* manager,
                                            gpu::MemoryTypeTracker* tracker) {
  return std::make_unique<SharedImageRepresentationOverlayVideo>(
      manager, this, tracker, GetDrDcLock());
}

SharedImageVideoImageReader::ContextLostObserverHelper::
    ContextLostObserverHelper(
        scoped_refptr<SharedContextState> context_state,
        scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
        scoped_refptr<base::SingleThreadTaskRunner> gpu_main_task_runner,
        scoped_refptr<RefCountedLock> drdc_lock)
    : RefCountedLockHelperDrDc(std::move(drdc_lock)),
      context_state_(std::move(context_state)),
      stream_texture_sii_(std::move(stream_texture_sii)),
      gpu_main_task_runner_(std::move(gpu_main_task_runner)) {
  DCHECK(context_state_);
  DCHECK(stream_texture_sii_);

  context_state_->AddContextLostObserver(this);
}

SharedImageVideoImageReader::ContextLostObserverHelper::
    ~ContextLostObserverHelper() {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  if (context_state_)
    context_state_->RemoveContextLostObserver(this);
  {
    base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
    stream_texture_sii_->ReleaseResources();
    stream_texture_sii_.reset();
  }
}

// SharedContextState::ContextLostObserver implementation.
void SharedImageVideoImageReader::ContextLostObserverHelper::OnContextLost() {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());
  base::AutoLockMaybe auto_lock(GetDrDcLockPtr());

  // We release codec buffers when shared image context is lost. This is
  // because texture owner's texture was created on shared context. Once
  // shared context is lost, no one should try to use that texture.
  stream_texture_sii_->ReleaseResources();
  context_state_->RemoveContextLostObserver(this);
  context_state_ = nullptr;
}

}  // namespace gpu
