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


// This file contains the implementation of RenderSurfaceGLES2 and
// RenderDepthStencilSurfaceGLES2.

#include "core/cross/gles2/render_surface_gles2.h"
#include "core/cross/gles2/utils_gles2-inl.h"
#include "core/cross/renderer.h"

namespace o3d {

RenderSurfaceGLES2::RenderSurfaceGLES2(ServiceLocator *service_locator,
                                       int width,
                                       int height,
                                       GLenum cube_face,
                                       int mip_level,
                                       Texture *texture)
    : RenderSurface(service_locator, width, height, texture),
      cube_face_(cube_face),
      mip_level_(mip_level) {
  DCHECK(texture);
}

RenderSurfaceGLES2::~RenderSurfaceGLES2() {
}

bool RenderSurfaceGLES2::PlatformSpecificGetIntoBitmap(
    Bitmap::Ref bitmap) const {
  Renderer* renderer = service_locator()->GetService<Renderer>();
  DCHECK(renderer);
  DCHECK(bitmap->width() == static_cast<unsigned int>(clip_width()) &&
         bitmap->height() == static_cast<unsigned int>(clip_height()) &&
         bitmap->num_mipmaps() == 1 &&
         bitmap->format() == Texture::ARGB8);

  const RenderSurface* old_render_surface;
  const RenderDepthStencilSurface* old_depth_surface;
  bool old_is_back_buffer;

  renderer->GetRenderSurfaces(&old_render_surface, &old_depth_surface,
                              &old_is_back_buffer);
  renderer->SetRenderSurfaces(this, NULL, false);

  ::glReadPixels(0, 0, clip_width(), clip_height(), GL_BGRA, GL_UNSIGNED_BYTE,
                 bitmap->image_data());

  renderer->SetRenderSurfaces(old_render_surface, old_depth_surface,
                              old_is_back_buffer);

  return true;
}

RenderDepthStencilSurfaceGLES2::RenderDepthStencilSurfaceGLES2(
    ServiceLocator *service_locator,
    int width,
    int height)
    : RenderDepthStencilSurface(service_locator, width, height) {

#ifndef DISABLE_FBO
#if defined(GLES2_BACKEND_DESKTOP_GL)
  // If packed depth stencil is supported, create only one buffer for both
  // depth and stencil.
  // TODO(piman): on GLES, test GL_OES_packed_depth_stencil
  if (GLEW_EXT_packed_depth_stencil) {
    glGenRenderbuffersEXT(1, render_buffers_);
    glBindRenderbufferEXT(GL_RENDERBUFFER, render_buffers_[0]);
    glRenderbufferStorageEXT(GL_RENDERBUFFER,
                             GL_DEPTH24_STENCIL8_EXT,
                             width,
                             height);
    CHECK_GL_ERROR();
    render_buffers_[1] = render_buffers_[0];
    return;
  }
#endif
  glGenRenderbuffersEXT(2, render_buffers_);
  glBindRenderbufferEXT(GL_RENDERBUFFER, render_buffers_[0]);
  glRenderbufferStorageEXT(GL_RENDERBUFFER,
#if defined(GLES2_BACKEND_DESKTOP_GL)
                           GL_DEPTH_COMPONENT24,
#else
                           // On GLES, only 16bit depth is available by default.
                           // TODO(piman): test for GL_OES_depth24 or 32 and
                           // use those.
                           GL_DEPTH_COMPONENT16,
#endif
                           width,
                           height);
  CHECK_GL_ERROR();

  glBindRenderbufferEXT(GL_RENDERBUFFER, render_buffers_[1]);
  glRenderbufferStorageEXT(GL_RENDERBUFFER,
                           GL_STENCIL_INDEX8,
                           width,
                           height);
  CHECK_GL_ERROR();
#endif
}

RenderDepthStencilSurfaceGLES2::~RenderDepthStencilSurfaceGLES2() {
#ifndef DISABLE_FBO
#if defined(GLES2_BACKEND_DESKTOP_GL)
  // TODO(piman): on GLES, test GL_OES_packed_depth_stencil
  if (GLEW_EXT_packed_depth_stencil) {
    glDeleteRenderbuffersEXT(1, render_buffers_);
    return;
  }
#endif
  glDeleteRenderbuffersEXT(2, render_buffers_);
#endif
}

}  // end namespace o3d
