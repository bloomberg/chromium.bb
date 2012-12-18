// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/async_pixel_transfer_delegate_android.h"

#include <sys/resource.h>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gl/async_pixel_transfer_delegate.h"
#include "ui/gl/async_pixel_transfer_delegate_stub.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"

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
     LOG(ERROR) << "Async transfer eglerror at "
                << file << ":" << line << " " << eglerror;
     success = false;
  }
  while ((glerror = glGetError()) != GL_NO_ERROR) {
     LOG(ERROR) << "Async transfer openglerror at "
                << file << ":" << line << " " << glerror;
     success = false;
  }
  return success;
}
#define CHK() CheckErrors(__FILE__, __LINE__)

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
void* GetAddress(SharedMemory* shared_memory,
                 uint32 shm_size,
                 uint32 shm_data_offset,
                 uint32 shm_data_size) {
  // Memory bounds have already been validated, so there
  // is just DCHECKS here.
  DCHECK(shared_memory);
  DCHECK(shared_memory->memory());
  DCHECK_LE(shm_data_offset + shm_data_size, shm_size);
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
    // Lower the thread priority for uploads.
    // TODO(epenner): What's a good value? Without lowering this, uploads
    // would often execute immediately and block the GPU thread.
    setpriority(PRIO_PROCESS, base::PlatformThread::CurrentId(), 5);

    GLShareGroup* share_group = NULL;
    bool software = false;
    surface_ = new gfx::PbufferGLSurfaceEGL(software, gfx::Size(1,1));
    surface_->Initialize();
    context_ = gfx::GLContext::CreateGLContext(share_group,
                                               surface_,
                                               gfx::PreferDiscreteGpu);
    bool is_current = context_->MakeCurrent(surface_);
    DCHECK(is_current);
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

} // namespace

// Class which holds async pixel transfers state (EGLImage).
// The EGLImage is accessed by either thread, but everything
// else accessed only on the main thread.
class TransferStateInternal
    : public base::RefCountedThreadSafe<TransferStateInternal> {
 public:
  explicit TransferStateInternal(GLuint texture_id)
      : texture_id_(texture_id),
        needs_late_bind_(false),
        transfer_in_progress_(false),
        egl_image_(EGL_NO_IMAGE_KHR) {
    static const AsyncTexImage2DParams zero_params = {0, 0, 0, 0, 0, 0, 0, 0};
    late_bind_define_params_ = zero_params;
  }

  // Implement AsyncPixelTransferState:
  bool TransferIsInProgress() {
    return transfer_in_progress_;
  }

  void BindTransfer(AsyncTexImage2DParams* bound_params) {
    DCHECK(bound_params);
    DCHECK(needs_late_bind_);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, egl_image_);
    *bound_params = late_bind_define_params_;
    needs_late_bind_ = false;
  }

  // Completion callbacks.
  void TexImage2DCompleted() {
    needs_late_bind_ = true;
    transfer_in_progress_ = false;
  }
  void TexSubImage2DCompleted() {
    transfer_in_progress_ = false;
  }

protected:
  friend class base::RefCountedThreadSafe<TransferStateInternal>;
  friend class AsyncPixelTransferDelegateAndroid;

  virtual ~TransferStateInternal() {
    if (egl_image_) {
      EGLDisplay display = eglGetCurrentDisplay();
      eglDestroyImageKHR(display, egl_image_);
    }
  }

  // The 'real' texture.
  GLuint texture_id_;

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
class AsyncPixelTransferDelegateAndroid : public AsyncPixelTransferDelegate {
 public:
  AsyncPixelTransferDelegateAndroid() {}
  virtual ~AsyncPixelTransferDelegateAndroid() {}

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

 private:
  // implement AsyncPixelTransferDelegate:
  virtual AsyncPixelTransferState*
      CreateRawPixelTransferState(GLuint texture_id) OVERRIDE;

  base::MessageLoopProxy* transfer_message_loop_proxy() {
    return g_transfer_thread.Pointer()->message_loop_proxy();
  }
  static void PerformAsyncTexImage2D(
      TransferStateInternal* state,
      AsyncTexImage2DParams tex_params,
      AsyncMemoryParams mem_params);
  static void PerformAsyncTexSubImage2D(
      TransferStateInternal* state,
      AsyncTexSubImage2DParams tex_params,
      AsyncMemoryParams mem_params);

  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferDelegateAndroid);
};

// We only used threaded uploads when we can:
// - Create EGLImages out of OpenGL textures (EGL_KHR_gl_texture_2D_image)
// - Bind EGLImages to OpenGL textures (GL_OES_EGL_image)
// - Use fences (to test for upload completion).
scoped_ptr<AsyncPixelTransferDelegate>
    AsyncPixelTransferDelegate::Create(gfx::GLContext* context) {
  DCHECK(context);
  // TODO(epenner): Enable it! Waiting for impl-side painting
  // to be more stable before enabling this.
  bool enable = false;
  if (enable &&
      context->HasExtension("EGL_KHR_fence_sync") &&
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
  TransferStateInternal* state =
      static_cast<AsyncTransferStateAndroid*>(transfer_state)->internal_.get();
  DCHECK(mem_params.shared_memory);
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
  // TODO: Could we pass an own pointer of the new SharedMemory to the task?
  AsyncMemoryParams duped_mem = mem_params;
  duped_mem.shared_memory = DuplicateSharedMemory(mem_params.shared_memory,
                                                  mem_params.shm_size);
  transfer_message_loop_proxy()->PostTaskAndReply(FROM_HERE,
      base::Bind(
          &AsyncPixelTransferDelegateAndroid::PerformAsyncTexImage2D,
          base::Unretained(state), // This is referenced in reply below.
          tex_params,
          duped_mem),
      base::Bind(
          &TransferStateInternal::TexImage2DCompleted,
          state));
}

void AsyncPixelTransferDelegateAndroid::AsyncTexSubImage2D(
    AsyncPixelTransferState* transfer_state,
    const AsyncTexSubImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params) {
  TRACE_EVENT2("gpu", "AsyncTexSubImage2D",
               "width", tex_params.width,
               "height", tex_params.height);
  TransferStateInternal* state =
      static_cast<AsyncTransferStateAndroid*>(transfer_state)->internal_.get();
  DCHECK(state->texture_id_);
  DCHECK(!state->transfer_in_progress_);
  DCHECK(mem_params.shared_memory);
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), tex_params.target);
  DCHECK_EQ(tex_params.level, 0);

  // Mark the transfer in progress.
  state->transfer_in_progress_ = true;

  // Create the EGLImage if it hasn't already been created.
  if (!state->egl_image_) {
    EGLDisplay egl_display = eglGetCurrentDisplay();
    EGLContext egl_context = eglGetCurrentContext();
    EGLenum egl_target = EGL_GL_TEXTURE_2D_KHR;
    EGLClientBuffer egl_buffer =
        reinterpret_cast<EGLClientBuffer>(state->texture_id_);
    EGLint egl_attrib_list[] = {
        EGL_GL_TEXTURE_LEVEL_KHR, tex_params.level, // mip-level to reference.
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, // preserve the data in the texture.
        EGL_NONE
    };
    state->egl_image_ = eglCreateImageKHR(
        egl_display,
        egl_context,
        egl_target,
        egl_buffer,
        egl_attrib_list);
  }

  // Duplicate the shared memory so there are no way we can get
  // a use-after-free of the raw pixels.
  // TODO: Could we pass an own pointer of the new SharedMemory to the task?
  AsyncMemoryParams duped_mem = mem_params;
  duped_mem.shared_memory = DuplicateSharedMemory(mem_params.shared_memory,
                                                  mem_params.shm_size);
  transfer_message_loop_proxy()->PostTaskAndReply(FROM_HERE,
      base::Bind(
          &AsyncPixelTransferDelegateAndroid::PerformAsyncTexSubImage2D,
          base::Unretained(state), // This is referenced in reply below.
          tex_params,
          duped_mem),
      base::Bind(
          &TransferStateInternal::TexSubImage2DCompleted,
          state));
}

namespace {
void WaitForGlFence() {
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
    AsyncMemoryParams mem_params) {
  // TODO(epenner): This is just to insure it is deleted. Could bind() do this?
  scoped_ptr<SharedMemory> shared_memory =
      make_scoped_ptr(mem_params.shared_memory);

  void* data = GetAddress(mem_params.shared_memory,
                          mem_params.shm_size,
                          mem_params.shm_data_offset,
                          mem_params.shm_data_size);

  // In texImage2D, we do everything on the upload thread. This is
  // because texImage2D can incur CPU allocation cost, and it also
  // 'orphans' any previous EGLImage bound to the texture.
  DCHECK_EQ(0, tex_params.level);
  DCHECK_EQ(EGL_NO_IMAGE_KHR, state->egl_image_);
  TRACE_EVENT2("gpu", "performAsyncTexImage2D",
               "width", tex_params.width,
               "height", tex_params.height);

  // Create a texture from the image and upload to it.
  GLuint temp_texture = 0;
  glGenTextures(1, &temp_texture);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, temp_texture);
  {
    TRACE_EVENT0("gpu", "performAsyncTexSubImage2D glTexImage2D");
    glTexImage2D(
        GL_TEXTURE_2D,
        tex_params.level,
        tex_params.internal_format,
        tex_params.width,
        tex_params.height,
        tex_params.border,
        tex_params.format,
        tex_params.type,
        data);
  }

  // Create the EGLImage, as texSubImage always 'orphan's a previous EGLImage.
  EGLDisplay egl_display = eglGetCurrentDisplay();
  EGLContext egl_context = eglGetCurrentContext();
  EGLenum egl_target = EGL_GL_TEXTURE_2D_KHR;
  EGLClientBuffer egl_buffer = (EGLClientBuffer) temp_texture;
  EGLint egl_attrib_list[] = {
      EGL_GL_TEXTURE_LEVEL_KHR, tex_params.level, // mip-map level.
      EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,          // preserve the data.
      EGL_NONE
  };
  state->egl_image_ = eglCreateImageKHR(
      egl_display,
      egl_context,
      egl_target,
      egl_buffer,
      egl_attrib_list);
  WaitForGlFence();

  // We can delete this thread's texture as the real texture
  // now contains the data.
  glDeleteTextures(1, &temp_texture);
}

void AsyncPixelTransferDelegateAndroid::PerformAsyncTexSubImage2D(
    TransferStateInternal* state,
    AsyncTexSubImage2DParams tex_params,
    AsyncMemoryParams mem_params) {
  // TODO(epenner): This is just to insure it is deleted. Could bind() do this?
  scoped_ptr<SharedMemory> shared_memory =
      make_scoped_ptr(mem_params.shared_memory);

  void* data = GetAddress(mem_params.shared_memory,
                          mem_params.shm_size,
                          mem_params.shm_data_offset,
                          mem_params.shm_data_size);

  // For a texSubImage, the texture must already have been
  // created on the main thread, along with EGLImageKHR.
  DCHECK_NE(EGL_NO_IMAGE_KHR, state->egl_image_);
  DCHECK_EQ(0, tex_params.level);
  TRACE_EVENT2("gpu", "performAsyncTexSubImage2D",
               "width", tex_params.width,
               "height", tex_params.height);

  // Create a texture from the image and upload to it.
  GLuint temp_texture = 0;
  glGenTextures(1, &temp_texture);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, temp_texture);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, state->egl_image_);
  {
    TRACE_EVENT0("gpu", "performAsyncTexSubImage2D glTexSubImage2D");
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
  WaitForGlFence();

  // We can delete this thread's texture as the real texture
  // now contains the data.
  glDeleteTextures(1, &temp_texture);
}

}  // namespace gfx
