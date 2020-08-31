// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_SURFACE_MOCK_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_SURFACE_MOCK_H_

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_surface.h"

namespace gpu {

class GLSurfaceMock : public gl::GLSurface {
 public:
  GLSurfaceMock();

  MOCK_METHOD1(Initialize, bool(gl::GLSurfaceFormat format));
  MOCK_METHOD0(Destroy, void());
  MOCK_METHOD4(Resize,
               bool(const gfx::Size& size,
                    float scale_factor,
                    const gfx::ColorSpace& color_space,
                    bool alpha));
  MOCK_METHOD0(IsOffscreen, bool());
  MOCK_METHOD1(SwapBuffers, gfx::SwapResult(PresentationCallback callback));
  MOCK_METHOD5(PostSubBuffer,
               gfx::SwapResult(int x,
                               int y,
                               int width,
                               int height,
                               PresentationCallback callback));
  MOCK_METHOD0(SupportsPostSubBuffer, bool());
  MOCK_METHOD0(GetSize, gfx::Size());
  MOCK_METHOD0(GetHandle, void*());
  MOCK_METHOD0(GetBackingFramebufferObject, unsigned int());
  MOCK_METHOD1(OnMakeCurrent, bool(gl::GLContext* context));
  MOCK_METHOD1(SetBackbufferAllocation, bool(bool allocated));
  MOCK_METHOD1(SetFrontbufferAllocation, void(bool allocated));
  MOCK_METHOD0(GetShareHandle, void*());
  MOCK_METHOD0(GetDisplay, void*());
  MOCK_METHOD0(GetConfig, void*());
  MOCK_METHOD0(GetFormat, gl::GLSurfaceFormat());

 protected:
  ~GLSurfaceMock() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceMock);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_SURFACE_MOCK_H_
