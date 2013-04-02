// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/async_pixel_transfer_delegate_stub.h"

#include "base/memory/shared_memory.h"
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
  // are just DCHECKS here.
  DCHECK(shared_memory);
  DCHECK(shared_memory->memory());
  DCHECK_LE(shm_data_offset + shm_data_size, shm_size);
  return static_cast<int8*>(shared_memory->memory()) + shm_data_offset;
}
} // namespace

namespace gfx {

scoped_ptr<AsyncPixelTransferDelegate>
      AsyncPixelTransferDelegateStub::Create(gfx::GLContext* context) {
  return make_scoped_ptr(
      static_cast<AsyncPixelTransferDelegate*>(
          new AsyncPixelTransferDelegateStub()));
}

AsyncTransferStateStub::AsyncTransferStateStub(GLuint texture_id) {
}

AsyncTransferStateStub::~AsyncTransferStateStub() {
}

bool AsyncTransferStateStub::TransferIsInProgress() {
  return false;
}

AsyncPixelTransferDelegateStub::AsyncPixelTransferDelegateStub()
    : texture_upload_count_(0) {
}

AsyncPixelTransferDelegateStub::~AsyncPixelTransferDelegateStub() {
}

AsyncPixelTransferState*
    AsyncPixelTransferDelegateStub::CreateRawPixelTransferState(
        GLuint texture_id,
        const AsyncTexImage2DParams& define_params) {
  return new AsyncTransferStateStub(texture_id);
}

bool AsyncPixelTransferDelegateStub::BindCompletedAsyncTransfers() {
  // Everything is already bound.
  return false;
}

void AsyncPixelTransferDelegateStub::AsyncNotifyCompletion(
    const AsyncMemoryParams& mem_params,
    const CompletionCallback& callback) {
  callback.Run(mem_params);
}

void AsyncPixelTransferDelegateStub::AsyncTexImage2D(
    AsyncPixelTransferState* transfer_state,
    const AsyncTexImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params,
    const base::Closure& bind_callback) {
  // Save the define params to return later during deferred
  // binding of the transfer texture.
  DCHECK(transfer_state);
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
  // The texture is already fully bound so just call it now.
  bind_callback.Run();
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

bool AsyncPixelTransferDelegateStub::ProcessMorePendingTransfers() {
  return false;
}

bool AsyncPixelTransferDelegateStub::NeedsProcessMorePendingTransfers() {
  return false;
}

}  // namespace gfx

