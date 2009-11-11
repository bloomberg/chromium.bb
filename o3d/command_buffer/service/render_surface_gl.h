/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef GPU_COMMAND_BUFFER_SERVICE_CROSS_GL_RENDER_SURFACE_GL_H__
#define GPU_COMMAND_BUFFER_SERVICE_CROSS_GL_RENDER_SURFACE_GL_H__

// This file contains the definition of the OpenGL versions of
// render surface-related resource classes.

#include "command_buffer/service/texture_gl.h"
#include "command_buffer/service/resource.h"

namespace command_buffer {
namespace o3d {

class RenderSurfaceGL : public RenderSurface {
 public:
  RenderSurfaceGL(int width,
                  int height,
                  int mip_level,
                  int side,
                  TextureGL *texture);
  virtual ~RenderSurfaceGL() {}

  static RenderSurfaceGL* Create(int width,
                                 int height,
                                 int mip_level,
                                 int side,
                                 TextureGL *texture);
  TextureGL* texture() {
    return texture_;
  }

  int width() {
    return width_;
  }

  int height() {
    return height_;
  }

  int mip_level() {
    return mip_level_;
  }

  int side() {
    return side_;
  }
 private:
  unsigned int width_;
  unsigned int height_;
  unsigned int mip_level_;
  unsigned int side_;
  TextureGL* texture_;
  DISALLOW_COPY_AND_ASSIGN(RenderSurfaceGL);
};

class RenderDepthStencilSurfaceGL : public RenderDepthStencilSurface {
 public:
  RenderDepthStencilSurfaceGL(int width,
                              int height);
  virtual ~RenderDepthStencilSurfaceGL() {}

  static RenderDepthStencilSurfaceGL* Create(
    int width,
    int height);

  GLuint depth_buffer() const {
    return render_buffers_[0];
  }

  GLuint stencil_buffer() const {
    return render_buffers_[1];
  }

 private:
  // Handles to the depth and stencil render-buffers, respectively.
  GLuint render_buffers_[2];
  unsigned int width_;
  unsigned int height_;
  DISALLOW_COPY_AND_ASSIGN(RenderDepthStencilSurfaceGL);
};

}  // namespace o3d
}  // namespace command_buffer

#endif  // GPU_COMMAND_BUFFER_SERVICE_WIN_GL_RENDER_SURFACE_GL_H__

