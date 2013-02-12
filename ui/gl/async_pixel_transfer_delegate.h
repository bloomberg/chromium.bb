// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ASYNC_TASK_DELEGATE_H_
#define UI_GL_ASYNC_TASK_DELEGATE_H_

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"

namespace base {
class SharedMemory;
}

namespace gfx {

struct AsyncTexImage2DParams {
  GLenum target;
  GLint level;
  GLenum internal_format;
  GLsizei width;
  GLsizei height;
  GLint border;
  GLenum format;
  GLenum type;
};

struct AsyncTexSubImage2DParams {
  GLenum target;
  GLint level;
  GLint xoffset;
  GLint yoffset;
  GLsizei width;
  GLsizei height;
  GLenum format;
  GLenum type;
};

struct AsyncMemoryParams {
  base::SharedMemory* shared_memory;
  uint32 shm_size;
  uint32 shm_data_offset;
  uint32 shm_data_size;
};

// AsyncPixelTransferState holds the resources required to do async
// transfers on one texture. It should stay alive for the lifetime
// of the texture to allow multiple transfers.
class GL_EXPORT AsyncPixelTransferState :
    public base::SupportsWeakPtr<AsyncPixelTransferState> {
 public:
  virtual ~AsyncPixelTransferState() {}

  // Returns true if there is a transfer in progress.
  virtual bool TransferIsInProgress() = 0;

  // Perform any custom binding of the transfer (currently only
  // needed for AsyncTexImage2D). The params used to define the texture
  // are returned in level_params.
  //
  // The transfer must be complete to call this (!TransferIsInProgress).
  virtual void BindTransfer(AsyncTexImage2DParams* level_params) = 0;

 protected:
  friend class base::RefCounted<AsyncPixelTransferState>;
  AsyncPixelTransferState() {}


 private:
  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferState);
};

class GL_EXPORT AsyncPixelTransferDelegate {
 public:
  static scoped_ptr<AsyncPixelTransferDelegate>
      Create(gfx::GLContext* context);
  virtual ~AsyncPixelTransferDelegate() {}

  // This wouldn't work with MOCK_METHOD, so it routes through
  // a virtual protected version which returns a raw pointer.
  scoped_ptr<AsyncPixelTransferState>
      CreatePixelTransferState(GLuint texture_id) {
    return make_scoped_ptr(CreateRawPixelTransferState(texture_id));
  }

  virtual void AsyncNotifyCompletion(
      const base::Closure& notify_task) = 0;

  virtual void AsyncTexImage2D(
      AsyncPixelTransferState* state,
      const AsyncTexImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params) = 0;

  virtual void AsyncTexSubImage2D(
      AsyncPixelTransferState* state,
      const AsyncTexSubImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params) = 0;

  virtual uint32 GetTextureUploadCount() = 0;
  virtual base::TimeDelta GetTotalTextureUploadTime() = 0;

 protected:
  AsyncPixelTransferDelegate() {}
  // For testing, as returning scoped_ptr wouldn't work with MOCK_METHOD.
  virtual AsyncPixelTransferState*
      CreateRawPixelTransferState(GLuint texture_id) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferDelegate);
};

}  // namespace gfx

#endif  // UI_GL_ASYNC_TASK_DELEGATE_H_

