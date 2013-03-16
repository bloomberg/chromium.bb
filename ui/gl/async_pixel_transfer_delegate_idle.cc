// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/async_pixel_transfer_delegate_idle.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/safe_shared_memory_pool.h"
#include "ui/gl/scoped_make_current.h"

using base::SharedMemory;
using base::SharedMemoryHandle;

namespace gfx {

namespace {

// Gets the address of the data from shared memory.
void* GetAddress(SharedMemory* shared_memory, uint32 shm_data_offset) {
  // Memory bounds have already been validated, so there
  // are just DCHECKS here.
  DCHECK(shared_memory);
  DCHECK(shared_memory->memory());
  return static_cast<int8*>(shared_memory->memory()) + shm_data_offset;
}

base::LazyInstance<SafeSharedMemoryPool> g_safe_shared_memory_pool =
    LAZY_INSTANCE_INITIALIZER;

SafeSharedMemoryPool* safe_shared_memory_pool() {
  return g_safe_shared_memory_pool.Pointer();
}

static uint64 g_next_pixel_transfer_state_id = 1;

} // namespace

#if !defined(OS_ANDROID)
scoped_ptr<AsyncPixelTransferDelegate>
    AsyncPixelTransferDelegate::Create(gfx::GLContext* context) {
  return AsyncPixelTransferDelegateIdle::Create(context);
}
#endif

scoped_ptr<AsyncPixelTransferDelegate>
      AsyncPixelTransferDelegateIdle::Create(gfx::GLContext* context) {
  return make_scoped_ptr(
      static_cast<AsyncPixelTransferDelegate*>(
          new AsyncPixelTransferDelegateIdle()));
}

class AsyncPixelTransferStateIdle : public AsyncPixelTransferState {
 public:
  typedef base::Callback<void(GLuint)> TransferCallback;

  explicit AsyncPixelTransferStateIdle(GLuint texture_id)
      : id_(g_next_pixel_transfer_state_id++),
        texture_id_(texture_id),
        transfer_in_progress_(false) {
  }
  virtual ~AsyncPixelTransferStateIdle() {}

  // Overridden from gfx::AsyncPixelTransferState:
  virtual bool TransferIsInProgress() OVERRIDE {
    return transfer_in_progress_;
  }

  uint64 id() const { return id_; }

  void set_transfer_in_progress(bool transfer_in_progress) {
    transfer_in_progress_ = transfer_in_progress;
  }

  void PerformTransfer(const TransferCallback& callback) {
    DCHECK(texture_id_);
    DCHECK(transfer_in_progress_);
    callback.Run(texture_id_);
    transfer_in_progress_ = false;
  }

 private:
  uint64 id_;
  GLuint texture_id_;
  bool transfer_in_progress_;
};

AsyncPixelTransferDelegateIdle::Transfer::Transfer(
    uint64 id, const base::Closure& task)
    : id(id),
      task(task) {
}

AsyncPixelTransferDelegateIdle::Transfer::~Transfer() {}

AsyncPixelTransferDelegateIdle::AsyncPixelTransferDelegateIdle()
    : texture_upload_count_(0) {
}

AsyncPixelTransferDelegateIdle::~AsyncPixelTransferDelegateIdle() {
}

AsyncPixelTransferState*
    AsyncPixelTransferDelegateIdle::CreateRawPixelTransferState(
        GLuint texture_id,
        const AsyncTexImage2DParams& define_params) {
  return new AsyncPixelTransferStateIdle(texture_id);
}

bool AsyncPixelTransferDelegateIdle::BindCompletedAsyncTransfers() {
  // Everything is already bound.
  return false;
}

void AsyncPixelTransferDelegateIdle::AsyncNotifyCompletion(
    const AsyncMemoryParams& mem_params,
    const CompletionCallback& callback) {
  if (transfers_.empty()) {
    callback.Run(mem_params);
    return;
  }

  transfers_.back().notifications.push(
      base::Bind(
          &AsyncPixelTransferDelegateIdle::PerformNotifyCompletion,
          AsWeakPtr(),
          mem_params,
          base::Owned(new ScopedSafeSharedMemory(safe_shared_memory_pool(),
                                                 mem_params.shared_memory,
                                                 mem_params.shm_size)),
          callback));
}

void AsyncPixelTransferDelegateIdle::AsyncTexImage2D(
    AsyncPixelTransferState* transfer_state,
    const AsyncTexImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params,
    const base::Closure& bind_callback) {
  AsyncPixelTransferStateIdle* state =
      static_cast<AsyncPixelTransferStateIdle*>(transfer_state);
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), tex_params.target);
  DCHECK(mem_params.shared_memory);
  DCHECK_LE(mem_params.shm_data_offset + mem_params.shm_data_size,
            mem_params.shm_size);
  DCHECK(state);

  transfers_.push_back(
      Transfer(
          state->id(),
          base::Bind(
              &AsyncPixelTransferStateIdle::PerformTransfer,
              base::AsWeakPtr(state),
              base::Bind(
                  &AsyncPixelTransferDelegateIdle::PerformAsyncTexImage2D,
                  AsWeakPtr(),
                  tex_params,
                  mem_params,
                  bind_callback,
                  base::Owned(new ScopedSafeSharedMemory(
                                  safe_shared_memory_pool(),
                                  mem_params.shared_memory,
                                  mem_params.shm_size))))));

  state->set_transfer_in_progress(true);
}

void AsyncPixelTransferDelegateIdle::AsyncTexSubImage2D(
    AsyncPixelTransferState* transfer_state,
    const AsyncTexSubImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params) {
  AsyncPixelTransferStateIdle* state =
      static_cast<AsyncPixelTransferStateIdle*>(transfer_state);
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), tex_params.target);
  DCHECK(mem_params.shared_memory);
  DCHECK_LE(mem_params.shm_data_offset + mem_params.shm_data_size,
            mem_params.shm_size);
  DCHECK(state);

  transfers_.push_back(
      Transfer(
          state->id(),
          base::Bind(
              &AsyncPixelTransferStateIdle::PerformTransfer,
              base::AsWeakPtr(state),
              base::Bind(
                  &AsyncPixelTransferDelegateIdle::PerformAsyncTexSubImage2D,
                  AsWeakPtr(),
                  tex_params,
                  mem_params,
                  base::Owned(new ScopedSafeSharedMemory(
                                  safe_shared_memory_pool(),
                                  mem_params.shared_memory,
                                  mem_params.shm_size))))));

  state->set_transfer_in_progress(true);
}

void AsyncPixelTransferDelegateIdle::WaitForTransferCompletion(
    AsyncPixelTransferState* transfer_state) {
  AsyncPixelTransferStateIdle* state =
      static_cast<AsyncPixelTransferStateIdle*>(transfer_state);
  DCHECK(state);

  for (std::list<Transfer>::iterator iter = transfers_.begin();
       iter != transfers_.end(); ++iter) {
    if (iter->id != state->id())
      continue;

    ProcessTransfer(*iter);
    transfers_.erase(iter);
    break;
  }
}

uint32 AsyncPixelTransferDelegateIdle::GetTextureUploadCount() {
  return texture_upload_count_;
}

base::TimeDelta AsyncPixelTransferDelegateIdle::GetTotalTextureUploadTime() {
  return total_texture_upload_time_;
}

bool AsyncPixelTransferDelegateIdle::ProcessMorePendingTransfers() {
  if (transfers_.empty())
    return false;

  ProcessTransfer(transfers_.front());
  transfers_.pop_front();
  return true;
}

bool AsyncPixelTransferDelegateIdle::NeedsProcessMorePendingTransfers() {
  return !transfers_.empty();
}

void AsyncPixelTransferDelegateIdle::ProcessTransfer(Transfer& transfer) {
  transfer.task.Run();
  while (!transfer.notifications.empty()) {
    transfer.notifications.front().Run();
    transfer.notifications.pop();
  }
}

void AsyncPixelTransferDelegateIdle::PerformNotifyCompletion(
    AsyncMemoryParams mem_params,
    ScopedSafeSharedMemory* safe_shared_memory,
    const CompletionCallback& callback) {
  TRACE_EVENT0("gpu", "PerformNotifyCompletion");
  gfx::AsyncMemoryParams safe_mem_params = mem_params;
  safe_mem_params.shared_memory = safe_shared_memory->shared_memory();
  callback.Run(safe_mem_params);
}

void AsyncPixelTransferDelegateIdle::PerformAsyncTexImage2D(
    AsyncTexImage2DParams tex_params,
    AsyncMemoryParams mem_params,
    const base::Closure& bind_callback,
    ScopedSafeSharedMemory* safe_shared_memory,
    GLuint texture_id) {
  TRACE_EVENT2("gpu", "PerformAsyncTexImage2D",
               "width", tex_params.width,
               "height", tex_params.height);

  void* data = GetAddress(safe_shared_memory->shared_memory(),
                          mem_params.shm_data_offset);

  glBindTexture(tex_params.target, texture_id);

  {
    TRACE_EVENT0("gpu", "glTexImage2D");
    glTexImage2D(
        tex_params.target,
        tex_params.level,
        tex_params.internal_format,
        tex_params.width,
        tex_params.height,
        tex_params.border,
        tex_params.format,
        tex_params.type,
        data);
  }

  // The texture is already fully bound so just call it now.
  bind_callback.Run();
}

void AsyncPixelTransferDelegateIdle::PerformAsyncTexSubImage2D(
    AsyncTexSubImage2DParams tex_params,
    AsyncMemoryParams mem_params,
    ScopedSafeSharedMemory* safe_shared_memory,
    GLuint texture_id) {
  TRACE_EVENT2("gpu", "PerformAsyncTexSubImage2D",
               "width", tex_params.width,
               "height", tex_params.height);

  void* data = GetAddress(safe_shared_memory->shared_memory(),
                          mem_params.shm_data_offset);

  base::TimeTicks begin_time(base::TimeTicks::HighResNow());
  glBindTexture(tex_params.target, texture_id);

  {
    TRACE_EVENT0("gpu", "glTexSubImage2D");
    glTexSubImage2D(
        tex_params.target,
        tex_params.level,
        tex_params.xoffset,
        tex_params.yoffset,
        tex_params.width,
        tex_params.height,
        tex_params.format,
        tex_params.type,
        data);
  }

  texture_upload_count_++;
  total_texture_upload_time_ += base::TimeTicks::HighResNow() - begin_time;
}

}  // namespace gfx
