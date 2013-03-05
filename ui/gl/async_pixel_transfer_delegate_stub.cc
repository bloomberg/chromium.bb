// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/async_pixel_transfer_delegate_stub.h"

#include "base/shared_memory.h"
#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"

using base::SharedMemory;
using base::SharedMemoryHandle;

namespace {
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
} // namespace

namespace gfx {

#if !defined(OS_ANDROID)
scoped_ptr<AsyncPixelTransferDelegate>
    AsyncPixelTransferDelegate::Create(gfx::GLContext* context) {
  return AsyncPixelTransferDelegateStub::Create(context);
}
#endif

scoped_ptr<AsyncPixelTransferDelegate>
      AsyncPixelTransferDelegateStub::Create(gfx::GLContext* context) {
  return make_scoped_ptr(
      static_cast<AsyncPixelTransferDelegate*>(
          new AsyncPixelTransferDelegateStub()));
}

AsyncTransferStateStub::AsyncTransferStateStub(GLuint texture_id) {
  static const AsyncTexImage2DParams zero_params = {0, 0, 0, 0, 0, 0, 0, 0};
  late_bind_define_params_ = zero_params;
  needs_late_bind_ = false;
}

AsyncTransferStateStub::~AsyncTransferStateStub() {
}

bool AsyncTransferStateStub::TransferIsInProgress() {
  return false;
}

void AsyncTransferStateStub::BindTransfer(AsyncTexImage2DParams* out_params) {
  DCHECK(out_params);
  DCHECK(needs_late_bind_);
  *out_params = late_bind_define_params_;
  needs_late_bind_ = false;
}

AsyncPixelTransferDelegateStub::AsyncPixelTransferDelegateStub()
    : texture_upload_count_(0) {
}

AsyncPixelTransferDelegateStub::~AsyncPixelTransferDelegateStub() {
}

AsyncPixelTransferState*
    AsyncPixelTransferDelegateStub::CreateRawPixelTransferState(
        GLuint texture_id) {
  return static_cast<AsyncPixelTransferState*>(
      new AsyncTransferStateStub(texture_id));
}

void AsyncPixelTransferDelegateStub::AsyncNotifyCompletion(
    const AsyncMemoryParams& mem_params,
    const CompletionCallback& callback) {
  callback.Run(mem_params);
}

void AsyncPixelTransferDelegateStub::AsyncTexImage2D(
    AsyncPixelTransferState* transfer_state,
    const AsyncTexImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params) {
  // Save the define params to return later during deferred
  // binding of the transfer texture.
  DCHECK(transfer_state);
  AsyncTransferStateStub* state =
      static_cast<AsyncTransferStateStub*>(transfer_state);
  // We don't actually need a late bind since this stub does
  // everything synchronously, but this tries to be similar
  // as an async implementation.
  state->needs_late_bind_ = true;
  state->late_bind_define_params_ = tex_params;
  void* data = GetAddress(mem_params.shared_memory,
                          mem_params.shm_size,
                          mem_params.shm_data_offset,
                          mem_params.shm_data_size);
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

void AsyncPixelTransferDelegateStub::AsyncTexSubImage2D(
    AsyncPixelTransferState* transfer_state,
    const AsyncTexSubImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params) {
  void* data = GetAddress(mem_params.shared_memory,
                          mem_params.shm_size,
                          mem_params.shm_data_offset,
                          mem_params.shm_data_size);
  DCHECK(transfer_state);
  AsyncTransferStateStub* state =
      static_cast<AsyncTransferStateStub*>(transfer_state);
  DCHECK(!state->needs_late_bind_);
  base::TimeTicks begin_time(base::TimeTicks::HighResNow());
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
  texture_upload_count_++;
  total_texture_upload_time_ += base::TimeTicks::HighResNow() - begin_time;
}

void AsyncPixelTransferDelegateStub::WaitForTransferCompletion(
    AsyncPixelTransferState* state) {
  // Already done.
}

uint32 AsyncPixelTransferDelegateStub::GetTextureUploadCount() {
  return texture_upload_count_;
}

base::TimeDelta AsyncPixelTransferDelegateStub::GetTotalTextureUploadTime() {
  return total_texture_upload_time_;
}

}  // namespace gfx

