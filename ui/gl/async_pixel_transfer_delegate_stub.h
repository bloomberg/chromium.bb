// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ASYNC_TASK_DELEGATE_STUB_H_
#define UI_GL_ASYNC_TASK_DELEGATE_STUB_H_

#include "ui/gl/async_pixel_transfer_delegate.h"

namespace gfx {

class AsyncTransferStateStub : public AsyncPixelTransferState {
 public:
  // implement AsyncPixelTransferState:
  virtual bool TransferIsInProgress() OVERRIDE;

 private:
  friend class AsyncPixelTransferDelegateStub;

  explicit AsyncTransferStateStub(GLuint texture_id);
  virtual ~AsyncTransferStateStub();
  DISALLOW_COPY_AND_ASSIGN(AsyncTransferStateStub);
};

// Class which handles async pixel transfers (as a fallback).
// This class just does the uploads synchronously.
class AsyncPixelTransferDelegateStub : public AsyncPixelTransferDelegate {
 public:
  static scoped_ptr<AsyncPixelTransferDelegate>
      Create(gfx::GLContext* context);

  AsyncPixelTransferDelegateStub();
  virtual ~AsyncPixelTransferDelegateStub();

  // implement AsyncPixelTransferDelegate:
  virtual bool BindCompletedAsyncTransfers() OVERRIDE;
  virtual void AsyncNotifyCompletion(
      const AsyncMemoryParams& mem_params,
      const CompletionCallback& callback) OVERRIDE;
  virtual void AsyncTexImage2D(
      AsyncPixelTransferState* state,
      const AsyncTexImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params,
      const base::Closure& bind_callback) OVERRIDE;
  virtual void AsyncTexSubImage2D(
      AsyncPixelTransferState* transfer_state,
      const AsyncTexSubImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params) OVERRIDE;
  virtual void WaitForTransferCompletion(
      AsyncPixelTransferState* state) OVERRIDE;
  virtual uint32 GetTextureUploadCount() OVERRIDE;
  virtual base::TimeDelta GetTotalTextureUploadTime() OVERRIDE;
  virtual bool ProcessMorePendingTransfers() OVERRIDE;
  virtual bool NeedsProcessMorePendingTransfers() OVERRIDE;
 private:
  // implement AsyncPixelTransferDelegate:
  virtual AsyncPixelTransferState*
      CreateRawPixelTransferState(GLuint texture_id,
          const AsyncTexImage2DParams& define_params) OVERRIDE;

  int texture_upload_count_;
  base::TimeDelta total_texture_upload_time_;

  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferDelegateStub);
};

}  // namespace gfx

#endif  // UI_GL_ASYNC_TASK_DELEGATE_ANDROID_H_

