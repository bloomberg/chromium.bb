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

// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
#include <sys/resource.h>

using base::SharedMemory;
using base::SharedMemoryHandle;

namespace gfx {

namespace {

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

// We duplicate shared memory to avoid use-after-free issues. This could also
// be solved by ref-counting something, or with a destruction callback. There
// wasn't an obvious hook or ref-counted container, so for now we dup/mmap.
SharedMemory* DuplicateSharedMemory(SharedMemory* shared_memory, uint32 size) {
  // Duplicate the handle.
  SharedMemoryHandle duped_shared_memory_handle;
  if (!shared_memory->ShareToProcess(
      base::GetCurrentProcessHandle(),
      &duped_shared_memory_handle))
    return NULL;
  scoped_ptr<SharedMemory> duped_shared_memory(
      new SharedMemory(duped_shared_memory_handle, false));
  // Map the shared memory into this process. This validates the size.
  if (!duped_shared_memory->Map(size))
    return NULL;
  return duped_shared_memory.release();
}

// Gets the address of the data from shared memory.
void* GetAddress(SharedMemory* shared_memory, uint32 shm_data_offset) {
  // Memory bounds have already been validated, so there
  // is just DCHECKS here.
  DCHECK(shared_memory);
  DCHECK(shared_memory->memory());
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

 private:
  scoped_refptr<gfx::GLContext> context_;
  scoped_refptr<gfx::GLSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(TransferThread);
};

base::LazyInstance<TransferThread>
    g_transfer_thread = LAZY_INSTANCE_INITIALIZER;

base::MessageLoopProxy* transfer_message_loop_proxy() {
  return g_transfer_thread.Pointer()->message_loop_proxy();
}

} // namespace

// Class which holds async pixel transfers state (EGLImage).
// The EGLImage is accessed by either thread, but everything
// else accessed only on the main thread.
class TransferStateInternal
    : public base::RefCountedThreadSafe<TransferStateInternal> {
 public:
  explicit TransferStateInternal(GLuint texture_id)
      : texture_id_(texture_id),
        thread_texture_id_(0),
        needs_late_bind_(false),
        transfer_in_progress_(false),
        egl_image_(EGL_NO_IMAGE_KHR),
        egl_image_created_sync_(EGL_NO_SYNC_KHR) {
    static const AsyncTexImage2DParams zero_params = {0, 0, 0, 0, 0, 0, 0, 0};
    late_bind_define_params_ = zero_params;
  }

  // Implement AsyncPixelTransferState:
  bool TransferIsInProgress() {
    return transfer_in_progress_;
  }

  void BindTransfer(AsyncTexImage2DParams* bound_params) {
    TRACE_EVENT2("gpu", "BindAsyncTransfer glEGLImageTargetTexture2DOES",
                 "width", late_bind_define_params_.width,
                 "height", late_bind_define_params_.height);
    DCHECK(bound_params);
    DCHECK(needs_late_bind_);
    DCHECK(texture_id_);
    DCHECK_NE(EGL_NO_IMAGE_KHR, egl_image_);
    // We can only change the active texture and unit 0,
    // as that is all that will be restored.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, egl_image_);
    *bound_params = late_bind_define_params_;
    needs_late_bind_ = false;
  }

  void CreateEglImage(GLuint texture_id) {
    TRACE_EVENT0("gpu", "eglCreateImageKHR");
    DCHECK(texture_id);
    DCHECK_EQ(egl_image_, EGL_NO_IMAGE_KHR);
    DCHECK_EQ(egl_image_created_sync_, EGL_NO_SYNC_KHR);

    EGLDisplay egl_display = eglGetCurrentDisplay();
    EGLContext egl_context = eglGetCurrentContext();
    EGLenum egl_target = EGL_GL_TEXTURE_2D_KHR;
    EGLClientBuffer egl_buffer =
        reinterpret_cast<EGLClientBuffer>(texture_id);
    EGLint egl_attrib_list[] = {
        EGL_GL_TEXTURE_LEVEL_KHR, 0,         // mip-level to reference.
        EGL_IMAGE_PRESERVED_KHR, EGL_FALSE,  // throw away texture data.
        EGL_NONE
    };
    egl_image_ = eglCreateImageKHR(
        egl_display,
        egl_context,
        egl_target,
        egl_buffer,
        egl_attrib_list);

    // TODO(epenner): Remove for other GPUs if it's just a Qualcomm bug.
    // BUG=crbug.com/168356
    egl_image_created_sync_ =
        eglCreateSyncKHR(egl_display, EGL_SYNC_FENCE_KHR, NULL);
  }

  void CreateEglImageOnUploadThread() {
    CreateEglImage(thread_texture_id_);
  }

  void CreateEglImageOnMainThreadIfNeeded() {
    if (egl_image_ == EGL_NO_IMAGE_KHR)
      CreateEglImage(texture_id_);
  }

  void WaitOnEglImageCreation() {
    // The first time we use the EGL image for upload, first
    // wait on it's creation to be completed. This seems to only be
    // needed for Qualcomm (even when uploading on the same thread).
    // TODO(epenner): Remove for other GPUs if it's just a Qualcomm bug.
    // BUG=crbug.com/168356
    if (egl_image_created_sync_ != EGL_NO_SYNC_KHR) {
      TRACE_EVENT0("gpu", "eglWaitSync");
      EGLint flags = EGL_SYNC_FLUSH_COMMANDS_BIT_KHR;
      EGLTimeKHR time = EGL_FOREVER_KHR;
      EGLDisplay display = eglGetCurrentDisplay();
      eglClientWaitSyncKHR(display, egl_image_created_sync_, flags, time);
      eglDestroySyncKHR(display, egl_image_created_sync_);
      egl_image_created_sync_ = EGL_NO_SYNC_KHR;
    }
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
      if (thread_texture_id_) {
        transfer_message_loop_proxy()->PostTask(FROM_HERE,
            base::Bind(&DeleteTexture, thread_texture_id_));
      }
    }
    if (egl_image_created_sync_ != EGL_NO_SYNC_KHR) {
      EGLDisplay display = eglGetCurrentDisplay();
      eglDestroySyncKHR(display, egl_image_created_sync_);
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
  bool transfer_in_progress_;

  // It would be nice if we could just create a new EGLImage for
  // every upload, but I found that didn't work, so this stores
  // one for the lifetime of the texture.
  EGLImageKHR egl_image_;
  EGLSyncKHR egl_image_created_sync_;

  // Time spent performing last transfer.
  base::TimeDelta last_transfer_time_;
};

// Android needs thread-safe ref-counting, so this just wraps
// an internal thread-safe ref-counted state object.
class AsyncTransferStateAndroid : public AsyncPixelTransferState {
 public:
  explicit AsyncTransferStateAndroid(GLuint texture_id)
      : internal_(new TransferStateInternal(texture_id)) {
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
      const base::Closure& task) OVERRIDE;
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

  void AsyncTexImage2DCompleted(scoped_refptr<TransferStateInternal> state);
  void AsyncTexSubImage2DCompleted(scoped_refptr<TransferStateInternal> state);

  static void PerformAsyncTexImage2D(
      TransferStateInternal* state,
      AsyncTexImage2DParams tex_params,
      base::SharedMemory* shared_memory,
      uint32 shared_memory_data_offset);
  static void PerformAsyncTexSubImage2D(
      TransferStateInternal* state,
      AsyncTexSubImage2DParams tex_params,
      base::SharedMemory* shared_memory,
      uint32 shared_memory_data_offset);

  int texture_upload_count_;
  base::TimeDelta total_texture_upload_time_;

  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferDelegateAndroid);
};

namespace {
// Imagination has some odd problems still.
bool IsImagination() {
  std::string vendor;
  vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  return vendor.find("Imagination") != std::string::npos;
}
}

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
      context->HasExtension("GL_OES_EGL_image") &&
      !IsImagination()) {
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

AsyncPixelTransferDelegateAndroid::AsyncPixelTransferDelegateAndroid()
    : texture_upload_count_(0) {
}

AsyncPixelTransferDelegateAndroid::~AsyncPixelTransferDelegateAndroid() {
}

AsyncPixelTransferState*
    AsyncPixelTransferDelegateAndroid::
        CreateRawPixelTransferState(GLuint texture_id) {
  return static_cast<AsyncPixelTransferState*>(
      new AsyncTransferStateAndroid(texture_id));
}

namespace {
// Dummy function to measure completion on
// the upload thread.
void NoOp() {}
} // namespace

void AsyncPixelTransferDelegateAndroid::AsyncNotifyCompletion(
      const base::Closure& task) {
  // Post a no-op task to the upload thread followed
  // by a reply to the callback. The reply will then occur after
  // all async transfers are complete.
  transfer_message_loop_proxy()->PostTaskAndReply(FROM_HERE,
      base::Bind(&NoOp), task);
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
  DCHECK(!state->transfer_in_progress_);
  DCHECK_EQ(state->egl_image_, EGL_NO_IMAGE_KHR);
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), tex_params.target);
  DCHECK_EQ(tex_params.level, 0);

  // Mark the transfer in progress and save define params for lazy binding.
  state->transfer_in_progress_ = true;
  state->late_bind_define_params_ = tex_params;

  // Duplicate the shared memory so there are no way we can get
  // a use-after-free of the raw pixels.
  transfer_message_loop_proxy()->PostTaskAndReply(FROM_HERE,
      base::Bind(
          &AsyncPixelTransferDelegateAndroid::PerformAsyncTexImage2D,
          base::Unretained(state.get()),  // This is referenced in reply below.
          tex_params,
          base::Owned(DuplicateSharedMemory(mem_params.shared_memory,
                                            mem_params.shm_size)),
          mem_params.shm_data_offset),
      base::Bind(
          &AsyncPixelTransferDelegateAndroid::AsyncTexImage2DCompleted,
          AsWeakPtr(),
          state));

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
  DCHECK(!state->transfer_in_progress_);
  DCHECK(mem_params.shared_memory);
  DCHECK_LE(mem_params.shm_data_offset + mem_params.shm_data_size,
            mem_params.shm_size);
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), tex_params.target);
  DCHECK_EQ(tex_params.level, 0);

  // Mark the transfer in progress.
  state->transfer_in_progress_ = true;

  // If this wasn't async allocated, we don't have an EGLImage yet.
  // Create the EGLImage if it hasn't already been created.
  state->CreateEglImageOnMainThreadIfNeeded();

  // Duplicate the shared memory so there are no way we can get
  // a use-after-free of the raw pixels.
  transfer_message_loop_proxy()->PostTaskAndReply(FROM_HERE,
      base::Bind(
          &AsyncPixelTransferDelegateAndroid::PerformAsyncTexSubImage2D,
          base::Unretained(state.get()),  // This is referenced in reply below.
          tex_params,
          base::Owned(DuplicateSharedMemory(mem_params.shared_memory,
                                            mem_params.shm_size)),
          mem_params.shm_data_offset),
      base::Bind(
          &AsyncPixelTransferDelegateAndroid::AsyncTexSubImage2DCompleted,
          AsWeakPtr(),
          state));

  DCHECK(CHECK_GL());
}

uint32 AsyncPixelTransferDelegateAndroid::GetTextureUploadCount() {
  return texture_upload_count_;
}

base::TimeDelta AsyncPixelTransferDelegateAndroid::GetTotalTextureUploadTime() {
  return total_texture_upload_time_;
}

void AsyncPixelTransferDelegateAndroid::AsyncTexImage2DCompleted(
    scoped_refptr<TransferStateInternal> state) {
  state->needs_late_bind_ = true;
  state->transfer_in_progress_ = false;
}

void AsyncPixelTransferDelegateAndroid::AsyncTexSubImage2DCompleted(
    scoped_refptr<TransferStateInternal> state) {
  state->transfer_in_progress_ = false;
  texture_upload_count_++;
  total_texture_upload_time_ += state->last_transfer_time_;
}

namespace {
void WaitForGl() {
  TRACE_EVENT0("gpu", "eglWaitSync");

  // Uploads usually finish on the CPU, but just in case add a fence
  // and guarantee the upload has completed. The flush bit is set to
  // insure we don't wait forever.
  EGLDisplay display = eglGetCurrentDisplay();
  EGLSyncKHR fence = eglCreateSyncKHR(display, EGL_SYNC_FENCE_KHR, NULL);
  EGLint flags = EGL_SYNC_FLUSH_COMMANDS_BIT_KHR;
  EGLTimeKHR time = EGL_FOREVER_KHR;

  // This fence is basically like calling glFinish, which is fine if
  // uploads occur on the CPU. If some upload work occurs on the GPU,
  // we may want to delay blocking on the fence.
  eglClientWaitSyncKHR(display, fence, flags, time);
  eglDestroySyncKHR(display, fence);
}
} // namespace

void AsyncPixelTransferDelegateAndroid::PerformAsyncTexImage2D(
    TransferStateInternal* state,
    AsyncTexImage2DParams tex_params,
    base::SharedMemory* shared_memory,
    uint32 shared_memory_data_offset) {
  TRACE_EVENT2("gpu", "PerformAsyncTexImage",
               "width", tex_params.width,
               "height", tex_params.height);
  DCHECK(state);
  DCHECK(!state->thread_texture_id_);
  DCHECK_EQ(0, tex_params.level);
  DCHECK_EQ(EGL_NO_IMAGE_KHR, state->egl_image_);

  void* data = GetAddress(shared_memory, shared_memory_data_offset);
  {
    TRACE_EVENT0("gpu", "glTexImage2D no data");
    glGenTextures(1, &state->thread_texture_id_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state->thread_texture_id_);

    // These params are needed for EGLImage creation to succeed on several
    // Android devices. I couldn't find this requirement in the EGLImage
    // extension spec, but several devices fail without it.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Allocate first, so we can create the EGLImage without
    // EGL_IMAGE_PRESERVED, which can be costly.
    glTexImage2D(
        GL_TEXTURE_2D,
        tex_params.level,
        tex_params.internal_format,
        tex_params.width,
        tex_params.height,
        tex_params.border,
        tex_params.format,
        tex_params.type,
        NULL);
  }

  state->CreateEglImageOnUploadThread();
  state->WaitOnEglImageCreation();

  {
    TRACE_EVENT0("gpu", "glTexSubImage2D with data");
    glTexSubImage2D(
        GL_TEXTURE_2D,
        tex_params.level,
        0,
        0,
        tex_params.width,
        tex_params.height,
        tex_params.format,
        tex_params.type,
        data);
  }

  WaitForGl();
  DCHECK(CHECK_GL());
}

void AsyncPixelTransferDelegateAndroid::PerformAsyncTexSubImage2D(
    TransferStateInternal* state,
    AsyncTexSubImage2DParams tex_params,
    base::SharedMemory* shared_memory,
    uint32 shared_memory_data_offset) {
  TRACE_EVENT2("gpu", "PerformAsyncTexSubImage2D",
               "width", tex_params.width,
               "height", tex_params.height);

  DCHECK(state);
  DCHECK_NE(EGL_NO_IMAGE_KHR, state->egl_image_);
  DCHECK_EQ(0, tex_params.level);

  state->WaitOnEglImageCreation();

  void* data = GetAddress(shared_memory, shared_memory_data_offset);

  base::TimeTicks begin_time(base::TimeTicks::HighResNow());
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
    glTexSubImage2D(
        GL_TEXTURE_2D,
        tex_params.level,
        tex_params.xoffset,
        tex_params.yoffset,
        tex_params.width,
        tex_params.height,
        tex_params.format,
        tex_params.type,
        data);
  }
  WaitForGl();

  DCHECK(CHECK_GL());
  state->last_transfer_time_ = base::TimeTicks::HighResNow() - begin_time;
}

}  // namespace gfx
