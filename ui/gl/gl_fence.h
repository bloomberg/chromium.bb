// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_FENCE_H_
#define UI_GL_GL_FENCE_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_export.h"

namespace gl {

class GL_EXPORT GLFence {
 public:
  GLFence();
  virtual ~GLFence();

  static bool IsSupported();
  static std::unique_ptr<GLFence> Create();

  // GpuFenceHandle integration.
  static bool IsGpuFenceSupported();

  // Consumes the GpuFenceHandle to create a paired local GL fence.
  static std::unique_ptr<GLFence> CreateFromGpuFence(
      const gfx::GpuFence& gpu_fence);

  // Create a new GLFence that can be used with GetGpuFence.
  static std::unique_ptr<GLFence> CreateForGpuFence();

  virtual bool HasCompleted() = 0;
  virtual void ClientWait() = 0;

  // Will block the server if supported, but might fall back to blocking the
  // client.
  virtual void ServerWait() = 0;

  // Returns true if re-setting state is supported.
  virtual bool ResetSupported();

  // Resets the fence to the original state.
  virtual void ResetState();

  // Loses the reference to the fence. Useful if the context is lost.
  virtual void Invalidate();

  // Returns a GpuFence. Only valid on a GLFence created by
  // CreateForGpuFence.
  virtual std::unique_ptr<gfx::GpuFence> GetGpuFence();

 private:
  DISALLOW_COPY_AND_ASSIGN(GLFence);
};

}  // namespace gl

#endif  // UI_GL_GL_FENCE_H_
