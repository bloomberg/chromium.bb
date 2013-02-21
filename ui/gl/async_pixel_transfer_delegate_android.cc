// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/async_pixel_transfer_delegate_android.h"

#include <string>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "ui/gl/async_pixel_transfer_delegate.h"
#include "ui/gl/async_pixel_transfer_delegate_stub.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/safe_shared_memory_pool.h"

// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
#include <sys/resource.h>

using base::SharedMemory;
using base::SharedMemoryHandle;

namespace gfx {

namespace {

class TextureUploadStats
    : public base::RefCountedThreadSafe<TextureUploadStats> {
 public:
  TextureUploadStats() : texture_upload_count_(0) {}

  void AddUpload(base::TimeDelta transfer_time) {
    base::AutoLock scoped_lock(lock_);
    texture_upload_count_++;
    total_texture_upload_time_ += transfer_time;
  }

  int GetStats(base::TimeDelta* total_texture_upload_time) {
    base::AutoLock scoped_lock(lock_);
    if (total_texture_upload_time)
      *total_texture_upload_time = total_texture_upload_time_;
    return texture_upload_count_;
  }

 private:
  friend class RefCountedThreadSafe<TextureUploadStats>;

  ~TextureUploadStats() {}

  int texture_upload_count_;
  base::TimeDelta total_texture_upload_time_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(TextureUploadStats);
};

const char kAsyncTransferThreadName[] = "AsyncTransferThread";

bool CheckErrors(const char* file, int line) {
  EGLint eglerror;
  GLenum glerror;
  bool success = true;
  while ((eglerror = eglGetError()) != EGL_SUCCESS) {
     LOG(ERROR) << "Async transfer EGL error at "
                << file << ":" << line << " " << eglerror;
     success = false;
  }
  while ((glerror = glGetError()) != GL_NO_ERROR) {
     LOG(ERROR) << "Async transfer OpenGL error at "
                << file << ":" << line << " " << glerror;
     success = false;
  }
  return success;
}
#define CHECK_GL() CheckErrors(__FILE__, __LINE__)

// Regular glTexImage2D call.
void DoTexImage2D(const AsyncTexImage2DParams& tex_params, void* data) {
  glTexImage2D(
      GL_TEXTURE_2D, tex_params.level, tex_params.internal_format,
      tex_params.width, tex_params.height,
      tex_params.border, tex_params.format, tex_params.type, data);
}

// Regular glTexSubImage2D call.
void DoTexSubImage2D(const AsyncTexSubImage2DParams& tex_params, void* data) {
  glTexSubImage2D(
      GL_TEXTURE_2D, tex_params.level,
      tex_params.xoffset, tex_params.yoffset,
      tex_params.width, tex_params.height,
      tex_params.format, tex_params.type, data);
}

// Full glTexSubImage2D call, from glTexImage2D params.
void DoFullTexSubImage2D(const AsyncTexImage2DParams& tex_params, void* data) {
  glTexSubImage2D(
      GL_TEXTURE_2D, tex_params.level,
      0, 0, tex_params.width, tex_params.height,
      tex_params.format, tex_params.type, data);
}

// Gets the address of the data from shared memory.
void* GetAddress(SharedMemory* shared_memory, uint32 shm_data_offset) {
  // Memory bounds have already been validated, so there
  // is just DCHECKS here.
  CHECK(shared_memory);
  CHECK(shared_memory->memory());
  return static_cast<int8*>(shared_memory->memory()) + shm_data_offset;
}

class TransferThread : public base::Thread {
 public:
  TransferThread() : base::Thread(kAsyncTransferThreadName) {
    Start();
  }
  virtual ~TransferThread() {
    Stop();
  }

  virtual void Init() OVERRIDE {
    GLShareGroup* share_group = NULL;
    bool software = false;
    surface_ = new gfx::PbufferGLSurfaceEGL(software, gfx::Size(1,1));
    surface_->Initialize();
    context_ = gfx::GLContext::CreateGLContext(share_group,
                                               surface_,
                                               gfx::PreferDiscreteGpu);
    bool is_current = context_->MakeCurrent(surface_);
    DCHECK(is_current);

    // TODO(epenner): Move thread priorities to base. (crbug.com/170549)
    int nice_value = 10; // Idle priority.
    setpriority(PRIO_PROCESS, base::PlatformThread::CurrentId(), nice_value);
  }

  virtual void CleanUp() OVERRIDE {
    surface_ = NULL;
    context_->ReleaseCurrent(surface_);
    context_ = NULL;
  }

  SafeSharedMemoryPool* safe_shared_memory_pool() {
      return &safe_shared_memory_pool_;
  }

 private:
  scoped_refptr<gfx::GLContext> context_;
  scoped_refptr<gfx::GLSurface> surface_;

  SafeSharedMemoryPool safe_shared_memory_pool_;

  DISALLOW_COPY_AND_ASSIGN(TransferThread);
};

base::LazyInstance<TransferThread>
    g_transfer_thread = LAZY_INSTANCE_INITIALIZER;

base::MessageLoopProxy* transfer_message_loop_proxy() {
  return g_transfer_thread.Pointer()->message_loop_proxy();
}

SafeSharedMemoryPool* safe_shared_memory_pool() {
  return g_transfer_thread.Pointer()->safe_shared_memory_pool();
}


} // namespace

// Class which holds async pixel transfers state (EGLImage).
// The EGLImage is accessed by either thread, but everything
// else accessed only on the main thread.
class TransferStateInternal
    : public base::RefCountedThreadSafe<TransferStateInternal> {
 public:
  explicit TransferStateInternal(GLuint texture_id,
                                 bool wait_for_uploads,
                                 bool use_image_preserved)
      : texture_id_(texture_id),
        thread_texture_id_(0),
        needs_late_bind_(false),
        egl_image_(EGL_NO_IMAGE_KHR),
        wait_for_uploads_(wait_for_uploads),
        use_image_preserved_(use_image_preserved) {
    static const AsyncTexImage2DParams zero_params = {0, 0, 0, 0, 0, 0, 0, 0};
    late_bind_define_params_ = zero_params;
    base::subtle::Acquire_Store(&transfer_in_progress_, 0);
  }

  // Implement AsyncPixelTransferState:
  bool TransferIsInProgress() {
    return base::subtle::Acquire_Load(&transfer_in_progress_) == 1;
  }

  void BindTransfer(AsyncTexImage2DParams* bound_params) {
    TRACE_EVENT2("gpu", "BindAsyncTransfer glEGLImageTargetTexture2DOES",
                 "width", late_bind_define_params_.width,
                 "height", late_bind_define_params_.height);
    DCHECK(bound_params);
    DCHECK(texture_id_);
    *bound_params = late_bind_define_params_;
    if (!needs_late_bind_)
      return;
    DCHECK_NE(EGL_NO_IMAGE_KHR, egl_image_);

    // We can only change the active texture and unit 0,
    // as that is all that will be restored.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, egl_image_);
    needs_late_bind_ = false;

    DCHECK(CHECK_GL());
  }

  void CreateEglImage(GLuint texture_id) {
    TRACE_EVENT0("gpu", "eglCreateImageKHR");
    DCHECK(texture_id);
    DCHECK_EQ(egl_image_, EGL_NO_IMAGE_KHR);

    EGLDisplay egl_display = eglGetCurrentDisplay();
    EGLContext egl_context = eglGetCurrentContext();
    EGLenum egl_target = EGL_GL_TEXTURE_2D_KHR;
    EGLClientBuffer egl_buffer =
        reinterpret_cast<EGLClientBuffer>(texture_id);

    EGLint image_preserved = use_image_preserved_ ? EGL_TRUE : EGL_FALSE;
    EGLint egl_attrib_list[] = {
        EGL_GL_TEXTURE_LEVEL_KHR, 0, // mip-level.
        EGL_IMAGE_PRESERVED_KHR, image_preserved,
        EGL_NONE
    };
    egl_image_ = eglCreateImageKHR(
        egl_display,
        egl_context,
        egl_target,
        egl_buffer,
        egl_attrib_list);

    DCHECK_NE(EGL_NO_IMAGE_KHR, egl_image_);
  }

  void CreateEglImageOnUploadThread() {
    CreateEglImage(thread_texture_id_);
  }

  void CreateEglImageOnMainThreadIfNeeded() {
    if (egl_image_ == EGL_NO_IMAGE_KHR)
      CreateEglImage(texture_id_);
  }

  void WaitForLastUpload() {
    // This glFinish is just a safe-guard for if uploads have some
    // GPU action that needs to occur. We could use fences and try
    // to do this less often. However, on older drivers fences are
    // not always reliable (eg. Mali-400 just blocks forever).
    if (wait_for_uploads_) {
      TRACE_EVENT0("gpu", "glFinish");
      glFinish();
    }
  }

  void MarkAsTransferIsInProgress() {
    base::subtle::Atomic32 old_value = base::subtle::Acquire_CompareAndSwap(
        &transfer_in_progress_, 0, 1);
    DCHECK_EQ(old_value, 0);
  }

  void MarkAsCompleted() {
    base::subtle::Atomic32 old_value = base::subtle::Release_CompareAndSwap(
        &transfer_in_progress_, 1, 0);
    DCHECK_EQ(old_value, 1);
  }

 protected:
  friend class base::RefCountedThreadSafe<TransferStateInternal>;
  friend class AsyncPixelTransferDelegateAndroid;

  static void DeleteTexture(GLuint id) {
    glDeleteTextures(1, &id);
  }

  virtual ~TransferStateInternal() {
    if (egl_image_ != EGL_NO_IMAGE_KHR) {
      EGLDisplay display = eglGetCurrentDisplay();
      eglDestroyImageKHR(display, egl_image_);
    }
    if (thread_texture_id_) {
      transfer_message_loop_proxy()->PostTask(FROM_HERE,
          base::Bind(&DeleteTexture, thread_texture_id_));
    }
  }

  // The 'real' texture.
  GLuint texture_id_;

  // The EGLImage sibling on the upload thread.
  GLuint thread_texture_id_;

  // Indicates there is a new EGLImage and the 'real'
  // texture needs to be bound to it as an EGLImage target.
  bool needs_late_bind_;

  // Definition params for texture that needs binding.
  AsyncTexImage2DParams late_bind_define_params_;

  // Indicates that an async transfer is in progress.
  base::subtle::Atomic32 transfer_in_progress_;

  // It would be nice if we could just create a new EGLImage for
  // every upload, but I found that didn't work, so this stores
  // one for the lifetime of the texture.
  EGLImageKHR egl_image_;

  // Customize when we block on fences (these are work-arounds).
  bool wait_for_uploads_;
  bool use_image_preserved_;
};

// Android needs thread-safe ref-counting, so this just wraps
// an internal thread-safe ref-counted state object.
class AsyncTransferStateAndroid : public AsyncPixelTransferState {
 public:
  explicit AsyncTransferStateAndroid(GLuint texture_id,
                                     bool wait_for_uploads,
                                     bool use_image_preserved)
      : internal_(new TransferStateInternal(texture_id,
                                            wait_for_uploads,
                                            use_image_preserved)) {
  }
  virtual ~AsyncTransferStateAndroid() {}
  virtual bool TransferIsInProgress() {
      return internal_->TransferIsInProgress();
  }
  virtual void BindTransfer(AsyncTexImage2DParams* bound_params) {
      internal_->BindTransfer(bound_params);
  }
  scoped_refptr<TransferStateInternal> internal_;
};

// Class which handles async pixel transfers on Android (using
// EGLImageKHR and another upload thread)
class AsyncPixelTransferDelegateAndroid
    : public AsyncPixelTransferDelegate,
      public base::SupportsWeakPtr<AsyncPixelTransferDelegateAndroid> {
 public:
  AsyncPixelTransferDelegateAndroid();
  virtual ~AsyncPixelTransferDelegateAndroid();

  // implement AsyncPixelTransferDelegate:
  virtual void AsyncNotifyCompletion(
      const AsyncMemoryParams& mem_params,
      const CompletionCallback& callback) OVERRIDE;
  virtual void AsyncTexImage2D(
      AsyncPixelTransferState* state,
      const AsyncTexImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params) OVERRIDE;
  virtual void AsyncTexSubImage2D(
      AsyncPixelTransferState* state,
      const AsyncTexSubImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params) OVERRIDE;
  virtual uint32 GetTextureUploadCount() OVERRIDE;
  virtual base::TimeDelta GetTotalTextureUploadTime() OVERRIDE;

 private:
  // implement AsyncPixelTransferDelegate:
  virtual AsyncPixelTransferState*
      CreateRawPixelTransferState(GLuint texture_id) OVERRIDE;

  static void PerformNotifyCompletion(
      AsyncMemoryParams mem_params,
      ScopedSafeSharedMemory* safe_shared_memory,
      const CompletionCallback& callback);
  static void PerformAsyncTexImage2D(
      TransferStateInternal* state,
      AsyncTexImage2DParams tex_params,
      AsyncMemoryParams mem_params,
      ScopedSafeSharedMemory* safe_shared_memory);
  static void PerformAsyncTexSubImage2D(
      TransferStateInternal* state,
      AsyncTexSubImage2DParams tex_params,
      AsyncMemoryParams mem_params,
      ScopedSafeSharedMemory* safe_shared_memory,
      scoped_refptr<TextureUploadStats> texture_upload_stats);

  // Returns true if a work-around was used.
  bool WorkAroundAsyncTexImage2D(
      TransferStateInternal* state,
      const AsyncTexImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params);
  bool WorkAroundAsyncTexSubImage2D(
      TransferStateInternal* state,
      const AsyncTexSubImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params);

  scoped_refptr<TextureUploadStats> texture_upload_stats_;
  bool is_imagination_;
  bool is_qualcomm_;

  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferDelegateAndroid);
};

// We only used threaded uploads when we can:
// - Create EGLImages out of OpenGL textures (EGL_KHR_gl_texture_2D_image)
// - Bind EGLImages to OpenGL textures (GL_OES_EGL_image)
// - Use fences (to test for upload completion).
scoped_ptr<AsyncPixelTransferDelegate>
    AsyncPixelTransferDelegate::Create(gfx::GLContext* context) {
  DCHECK(context);
  if (context->HasExtension("EGL_KHR_fence_sync") &&
      context->HasExtension("EGL_KHR_image") &&
      context->HasExtension("EGL_KHR_image_base") &&
      context->HasExtension("EGL_KHR_gl_texture_2D_image") &&
      context->HasExtension("GL_OES_EGL_image")) {
    return make_scoped_ptr(
        static_cast<AsyncPixelTransferDelegate*>(
            new AsyncPixelTransferDelegateAndroid()));
  } else {
    LOG(INFO) << "Async pixel transfers not supported";
    return make_scoped_ptr(
        static_cast<AsyncPixelTransferDelegate*>(
            new AsyncPixelTransferDelegateStub()));
  }
}

AsyncPixelTransferDelegateAndroid::AsyncPixelTransferDelegateAndroid() {
  std::string vendor;
  vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  is_imagination_ = vendor.find("Imagination") != std::string::npos;
  is_qualcomm_ = vendor.find("Qualcomm") != std::string::npos;
  // TODO(reveman): Skip this if --enable-gpu-benchmarking is not present.
  texture_upload_stats_ = make_scoped_refptr(new TextureUploadStats);
}

AsyncPixelTransferDelegateAndroid::~AsyncPixelTransferDelegateAndroid() {
}

AsyncPixelTransferState*
    AsyncPixelTransferDelegateAndroid::
        CreateRawPixelTransferState(GLuint texture_id) {

  // We can't wait on uploads on imagination (it can take 200ms+).
  // In practice, they are complete when the CPU glTexSubImage2D completes.
  bool wait_for_uploads = !is_imagination_;

  // Qualcomm has a race when using image_preserved=FALSE,
  // which can result in black textures even after the first upload.
  // Since using FALSE is mainly for performance (to avoid layout changes),
  // but Qualcomm itself doesn't seem to get any performance benefit,
  // we just using image_preservedd=TRUE on Qualcomm as a work-around.
  bool use_image_preserved = is_qualcomm_ || is_imagination_;

  return static_cast<AsyncPixelTransferState*>(
      new AsyncTransferStateAndroid(texture_id,
                                    wait_for_uploads,
                                    use_image_preserved));
}

void AsyncPixelTransferDelegateAndroid::AsyncNotifyCompletion(
    const AsyncMemoryParams& mem_params,
    const CompletionCallback& callback) {
  DCHECK(mem_params.shared_memory);
  DCHECK_LE(mem_params.shm_data_offset + mem_params.shm_data_size,
            mem_params.shm_size);
  // Post a PerformNotifyCompletion task to the upload thread. This task
  // will run after all async transfers are complete.
  transfer_message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&AsyncPixelTransferDelegateAndroid::PerformNotifyCompletion,
                 mem_params,
                 base::Owned(
                     new ScopedSafeSharedMemory(safe_shared_memory_pool(),
                                                mem_params.shared_memory,
                                                mem_params.shm_size)),
                 callback));
}

void AsyncPixelTransferDelegateAndroid::AsyncTexImage2D(
    AsyncPixelTransferState* transfer_state,
    const AsyncTexImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params) {
  scoped_refptr<TransferStateInternal> state =
      static_cast<AsyncTransferStateAndroid*>(transfer_state)->internal_.get();
  DCHECK(mem_params.shared_memory);
  DCHECK_LE(mem_params.shm_data_offset + mem_params.shm_data_size,
            mem_params.shm_size);
  DCHECK(state);
  DCHECK(state->texture_id_);
  DCHECK(!state->needs_late_bind_);
  DCHECK(!state->TransferIsInProgress());
  DCHECK_EQ(state->egl_image_, EGL_NO_IMAGE_KHR);
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), tex_params.target);
  DCHECK_EQ(tex_params.level, 0);

  if (WorkAroundAsyncTexImage2D(state, tex_params, mem_params))
    return;

  // Mark the transfer in progress and save define params for lazy binding.
  state->needs_late_bind_ = true;
  state->late_bind_define_params_ = tex_params;

  // Mark the transfer in progress.
  state->MarkAsTransferIsInProgress();

  // Duplicate the shared memory so there are no way we can get
  // a use-after-free of the raw pixels.
  transfer_message_loop_proxy()->PostTask(FROM_HERE,
      base::Bind(
          &AsyncPixelTransferDelegateAndroid::PerformAsyncTexImage2D,
          state,
          tex_params,
          mem_params,
          base::Owned(new ScopedSafeSharedMemory(safe_shared_memory_pool(),
                                                 mem_params.shared_memory,
                                                 mem_params.shm_size))));

  DCHECK(CHECK_GL());
}

void AsyncPixelTransferDelegateAndroid::AsyncTexSubImage2D(
    AsyncPixelTransferState* transfer_state,
    const AsyncTexSubImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params) {
  TRACE_EVENT2("gpu", "AsyncTexSubImage2D",
               "width", tex_params.width,
               "height", tex_params.height);
  scoped_refptr<TransferStateInternal> state =
      static_cast<AsyncTransferStateAndroid*>(transfer_state)->internal_.get();
  DCHECK(state->texture_id_);
  DCHECK(!state->TransferIsInProgress());
  DCHECK(mem_params.shared_memory);
  DCHECK_LE(mem_params.shm_data_offset + mem_params.shm_data_size,
            mem_params.shm_size);
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), tex_params.target);
  DCHECK_EQ(tex_params.level, 0);

  if (WorkAroundAsyncTexSubImage2D(state, tex_params, mem_params))
    return;

  // Mark the transfer in progress.
  state->MarkAsTransferIsInProgress();

  // If this wasn't async allocated, we don't have an EGLImage yet.
  // Create the EGLImage if it hasn't already been created.
  state->CreateEglImageOnMainThreadIfNeeded();

  // Duplicate the shared memory so there are no way we can get
  // a use-after-free of the raw pixels.
  transfer_message_loop_proxy()->PostTask(FROM_HERE,
      base::Bind(
          &AsyncPixelTransferDelegateAndroid::PerformAsyncTexSubImage2D,
          state,
          tex_params,
          mem_params,
          base::Owned(new ScopedSafeSharedMemory(safe_shared_memory_pool(),
                                                 mem_params.shared_memory,
                                                 mem_params.shm_size)),
          texture_upload_stats_));

  DCHECK(CHECK_GL());
}

uint32 AsyncPixelTransferDelegateAndroid::GetTextureUploadCount() {
  CHECK(texture_upload_stats_);
  return texture_upload_stats_->GetStats(NULL);
}

base::TimeDelta AsyncPixelTransferDelegateAndroid::GetTotalTextureUploadTime() {
  CHECK(texture_upload_stats_);
  base::TimeDelta total_texture_upload_time;
  texture_upload_stats_->GetStats(&total_texture_upload_time);
  return total_texture_upload_time;
}

namespace {
void SetGlParametersForEglImageTexture() {
  // These params are needed for EGLImage creation to succeed on several
  // Android devices. I couldn't find this requirement in the EGLImage
  // extension spec, but several devices fail without it.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
} // namespace

void AsyncPixelTransferDelegateAndroid::PerformNotifyCompletion(
    AsyncMemoryParams mem_params,
    ScopedSafeSharedMemory* safe_shared_memory,
    const CompletionCallback& callback) {
  TRACE_EVENT0("gpu", "PerformNotifyCompletion");
  gfx::AsyncMemoryParams safe_mem_params = mem_params;
  safe_mem_params.shared_memory = safe_shared_memory->shared_memory();
  callback.Run(safe_mem_params);
}

void AsyncPixelTransferDelegateAndroid::PerformAsyncTexImage2D(
    TransferStateInternal* state,
    AsyncTexImage2DParams tex_params,
    AsyncMemoryParams mem_params,
    ScopedSafeSharedMemory* safe_shared_memory) {
  TRACE_EVENT2("gpu", "PerformAsyncTexImage",
               "width", tex_params.width,
               "height", tex_params.height);
  DCHECK(state);
  DCHECK(!state->thread_texture_id_);
  DCHECK_EQ(0, tex_params.level);
  DCHECK_EQ(EGL_NO_IMAGE_KHR, state->egl_image_);

  void* data = GetAddress(safe_shared_memory->shared_memory(),
                          mem_params.shm_data_offset);
  {
    TRACE_EVENT0("gpu", "glTexImage2D no data");
    glGenTextures(1, &state->thread_texture_id_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state->thread_texture_id_);

    SetGlParametersForEglImageTexture();

    // If we need to use image_preserved, we pass the data with
    // the allocation. Otherwise we use a NULL allocation to
    // try to avoid any costs associated with creating the EGLImage.
    if (state->use_image_preserved_)
       DoTexImage2D(tex_params, data);
    else
       DoTexImage2D(tex_params, NULL);
  }

  state->CreateEglImageOnUploadThread();

  {
    TRACE_EVENT0("gpu", "glTexSubImage2D with data");

    // If we didn't use image_preserved, we haven't uploaded
    // the data yet, so we do this with a full texSubImage.
    if (!state->use_image_preserved_)
      DoFullTexSubImage2D(tex_params, data);
  }

  state->WaitForLastUpload();
  state->MarkAsCompleted();

  DCHECK(CHECK_GL());
}

void AsyncPixelTransferDelegateAndroid::PerformAsyncTexSubImage2D(
    TransferStateInternal* state,
    AsyncTexSubImage2DParams tex_params,
    AsyncMemoryParams mem_params,
    ScopedSafeSharedMemory* safe_shared_memory,
    scoped_refptr<TextureUploadStats> texture_upload_stats) {
  TRACE_EVENT2("gpu", "PerformAsyncTexSubImage2D",
               "width", tex_params.width,
               "height", tex_params.height);

  DCHECK(state);
  DCHECK_NE(EGL_NO_IMAGE_KHR, state->egl_image_);
  DCHECK_EQ(0, tex_params.level);

  void* data = GetAddress(safe_shared_memory->shared_memory(),
                          mem_params.shm_data_offset);

  base::TimeTicks begin_time;
  if (texture_upload_stats)
    begin_time = base::TimeTicks::HighResNow();

  if (!state->thread_texture_id_) {
    TRACE_EVENT0("gpu", "glEGLImageTargetTexture2DOES");
    glGenTextures(1, &state->thread_texture_id_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state->thread_texture_id_);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, state->egl_image_);
  } else {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state->thread_texture_id_);
  }
  {
    TRACE_EVENT0("gpu", "glTexSubImage2D");
    DoTexSubImage2D(tex_params, data);
  }
  state->WaitForLastUpload();
  state->MarkAsCompleted();

  DCHECK(CHECK_GL());
  if (texture_upload_stats) {
    texture_upload_stats->AddUpload(
        base::TimeTicks::HighResNow() - begin_time);
  }
}

namespace {
bool IsPowerOfTwo (unsigned int x) {
  return ((x != 0) && !(x & (x - 1)));
}

bool IsMultipleOfEight(unsigned int x) {
  return (x & 7) == 0;
}

bool DimensionsSupportImgFastPath(int width, int height) {
  // Multiple of eight, but not a power of two.
  return IsMultipleOfEight(width) &&
         IsMultipleOfEight(height) &&
         !(IsPowerOfTwo(width) &&
           IsPowerOfTwo(height));
}
} // namespace

// It is very difficult to stream uploads on Imagination GPUs:
// - glTexImage2D defers a swizzle/stall until draw-time
// - glTexSubImage2D will sleep for 16ms on a good day, and 100ms
//   or longer if OpenGL is in heavy use by another thread.
// The one combination that avoids these problems requires:
// a.) Allocations/Uploads must occur on different threads/contexts.
// b.) Texture size must be non-power-of-two.
// When using a+b, uploads will be incorrect/corrupt unless:
// c.) Texture size must be a multiple-of-eight.
//
// To achieve a.) we allocate synchronously on the main thread followed
// by uploading on the upload thread. When b/c are not true we fall back
// on purely synchronous allocation/upload on the main thread.

bool AsyncPixelTransferDelegateAndroid::WorkAroundAsyncTexImage2D(
    TransferStateInternal* state,
    const AsyncTexImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params) {
  if (!is_imagination_)
    return false;

  // On imagination we allocate synchronously all the time, even
  // if the dimensions support fast uploads. This is for part a.)
  // above, so allocations occur on a different thread/context as uploads.
  void* data = GetAddress(mem_params.shared_memory,
                          mem_params.shm_data_offset);
  SetGlParametersForEglImageTexture();

  {
    TRACE_EVENT0("gpu", "glTexImage2D with data");
    DoTexImage2D(tex_params, data);
  }

  // The allocation has already occured, so mark it as finished
  // and ready for binding.
  state->needs_late_bind_ = false;
  state->late_bind_define_params_ = tex_params;
  CHECK(!state->TransferIsInProgress());

  // If the dimensions support fast async uploads, create the
  // EGLImage for future uploads. The late bind should not
  // be needed since the EGLImage was created from the main thread
  // texture, but this is required to prevent an imagination driver crash.
  if (DimensionsSupportImgFastPath(tex_params.width, tex_params.height)) {
    state->CreateEglImageOnMainThreadIfNeeded();
    state->needs_late_bind_ = true;
  }

  DCHECK(CHECK_GL());
  return true;
}

bool AsyncPixelTransferDelegateAndroid::WorkAroundAsyncTexSubImage2D(
    TransferStateInternal* state,
    const AsyncTexSubImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params) {
  if (!is_imagination_)
    return false;

  // If the dimensions support fast async uploads, we can use the
  // normal async upload path for uploads.
  if (DimensionsSupportImgFastPath(tex_params.width, tex_params.height))
    return false;

  // Fall back on a synchronous stub as we don't have a known fast path.
  // Also, older ICS drivers crash when we do any glTexSubImage2D on the
  // same thread. To work around this we do glTexImage2D instead. Since
  // we didn't create an EGLImage for this texture (see above), this is
  // okay, but it limits this API to full updates for now.
  DCHECK(!state->egl_image_);
  DCHECK_EQ(tex_params.xoffset, 0);
  DCHECK_EQ(tex_params.yoffset, 0);
  DCHECK_EQ(state->late_bind_define_params_.width, tex_params.width);
  DCHECK_EQ(state->late_bind_define_params_.height, tex_params.height);
  DCHECK_EQ(state->late_bind_define_params_.level, tex_params.level);
  DCHECK_EQ(state->late_bind_define_params_.format, tex_params.format);
  DCHECK_EQ(state->late_bind_define_params_.type, tex_params.type);

  void* data = GetAddress(mem_params.shared_memory,
                          mem_params.shm_data_offset);
  base::TimeTicks begin_time;
  if (texture_upload_stats_)
    begin_time = base::TimeTicks::HighResNow();
  {
    TRACE_EVENT0("gpu", "glTexSubImage2D");
    // Note we use late_bind_define_params_ instead of tex_params.
    // The DCHECKs above verify this is always the same.
    DoTexImage2D(state->late_bind_define_params_, data);
  }
  if (texture_upload_stats_) {
    texture_upload_stats_->AddUpload(
        base::TimeTicks::HighResNow() - begin_time);
  }

  DCHECK(CHECK_GL());
  return true;
}

}  // namespace gfx
