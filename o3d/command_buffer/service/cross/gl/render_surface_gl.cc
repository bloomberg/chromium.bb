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


// This file implements the OpenGL versions of the render surface resources,
// as well as the related GAPIGL function implementations.

#include "command_buffer/service/cross/gl/gapi_gl.h"
#include "command_buffer/service/cross/gl/render_surface_gl.h"


namespace o3d {
namespace command_buffer {
namespace o3d {

RenderSurfaceGL::RenderSurfaceGL(int width,
                                 int height,
                                 int mip_level,
                                 int side,
                                 TextureGL *texture)
    : width_(width), height_(height), mip_level_(mip_level), texture_(texture) {
}

RenderSurfaceGL* RenderSurfaceGL::Create(int width,
                                         int height,
                                         int mip_level,
                                         int side,
                                         TextureGL *texture) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
  DCHECK_GE(mip_level, 0);
  DCHECK(texture);

  RenderSurfaceGL* render_surface =
      new RenderSurfaceGL(width, height, mip_level, side, texture);
  return render_surface;
}

RenderDepthStencilSurfaceGL::RenderDepthStencilSurfaceGL(
    int width,
    int height)
    : width_(width), height_(height) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);

  // If packed depth stencil is supported, create only one buffer for both
  // depth and stencil.
  if (GLEW_EXT_packed_depth_stencil) {
    glGenRenderbuffersEXT(1, render_buffers_);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, render_buffers_[0]);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,
                             GL_DEPTH24_STENCIL8_EXT,
                             width,
                             height);
    CHECK_GL_ERROR();
    render_buffers_[1] = render_buffers_[0];
  } else {
    glGenRenderbuffersEXT(2, render_buffers_);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, render_buffers_[0]);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,
                             GL_DEPTH_COMPONENT24,
                             width,
                             height);
    CHECK_GL_ERROR();

    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, render_buffers_[1]);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,
                             GL_STENCIL_INDEX8_EXT,
                             width,
                             height);
    CHECK_GL_ERROR();
  }
}

RenderDepthStencilSurfaceGL* RenderDepthStencilSurfaceGL::Create(
    int width,
    int height) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);

  return new RenderDepthStencilSurfaceGL(height, width);
}

// Copies the data from a texture resource.
parse_error::ParseError GAPIGL::CreateRenderSurface(
    ResourceId id,
    unsigned int width,
    unsigned int height,
    unsigned int mip_level,
    unsigned int side,
    ResourceId texture_id) {
  if (id == current_surface_id_) {
    // This will delete the current surface which would be bad.
    return parse_error::kParseInvalidArguments;
  }
  TextureGL *texture = textures_.Get(texture_id);
  if (!texture->render_surfaces_enabled()) {
    return parse_error::kParseInvalidArguments;
  } else {
    RenderSurfaceGL* render_surface = RenderSurfaceGL::Create(width,
                                                              height,
                                                              mip_level,
                                                              side,
                                                              texture);
    if (render_surface == NULL) {
      return parse_error::kParseInvalidArguments;
    }
    render_surfaces_.Assign(id, render_surface);
  }
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIGL::DestroyRenderSurface(ResourceId id) {
  if (id == current_surface_id_) {
    return parse_error::kParseInvalidArguments;
  }
  return render_surfaces_.Destroy(id) ?
      parse_error::kParseNoError :
      parse_error::kParseInvalidArguments;
}

parse_error::ParseError GAPIGL::CreateDepthSurface(
    ResourceId id,
    unsigned int width,
    unsigned int height) {
  if (id == current_depth_surface_id_) {
    // This will delete the current surface which would be bad.
    return parse_error::kParseInvalidArguments;
  }
  RenderDepthStencilSurfaceGL* depth_surface =
    RenderDepthStencilSurfaceGL::Create(width, height);
  if (depth_surface == NULL) {
    return parse_error::kParseInvalidArguments;
  }
  depth_surfaces_.Assign(id, depth_surface);
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPIGL::DestroyDepthSurface(ResourceId id) {
  if (id == current_depth_surface_id_) {
    return parse_error::kParseInvalidArguments;
  }
  return depth_surfaces_.Destroy(id) ?
      parse_error::kParseNoError :
      parse_error::kParseInvalidArguments;
}

void ResetBoundAttachments() {
#ifdef _DEBUG
    GLint bound_framebuffer;
    ::glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &bound_framebuffer);
    DCHECK(bound_framebuffer != 0);
#endif

  // Reset the bound attachments to the current framebuffer object.
  ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                 GL_COLOR_ATTACHMENT0_EXT,
                                 GL_RENDERBUFFER_EXT,
                                 0);

  ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                 GL_DEPTH_ATTACHMENT_EXT,
                                 GL_RENDERBUFFER_EXT,
                                 0);

  ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                 GL_STENCIL_ATTACHMENT_EXT,
                                 GL_RENDERBUFFER_EXT,
                                 0);

  CHECK_GL_ERROR();
}

bool BindDepthStencilBuffer(const RenderDepthStencilSurfaceGL* gl_surface) {
  // Bind both the depth and stencil attachments.
  ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                 GL_DEPTH_ATTACHMENT_EXT,
                                 GL_RENDERBUFFER_EXT,
                                 gl_surface->depth_buffer());
  ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                 GL_STENCIL_ATTACHMENT_EXT,
                                 GL_RENDERBUFFER_EXT,
                                 gl_surface->stencil_buffer());

  // Check for errors.
  GLenum framebuffer_status = ::glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  if (GL_FRAMEBUFFER_COMPLETE_EXT != framebuffer_status) {
    return false;
  }

  CHECK_GL_ERROR();
  return true;
}

parse_error::ParseError GAPIGL::SetRenderSurface(
    ResourceId render_surface_id,
    ResourceId depth_stencil_id) {
  if (render_surfaces_.Get(render_surface_id) == NULL &&
      depth_surfaces_.Get(depth_stencil_id) == NULL) {
    return parse_error::kParseInvalidArguments;
  }

  ::glBindFramebufferEXT(GL_FRAMEBUFFER, render_surface_framebuffer_);
  ResetBoundAttachments();

  RenderSurfaceGL* render_surface = render_surfaces_.Get(render_surface_id);
  RenderDepthStencilSurfaceGL* depth_surface =
      depth_surfaces_.Get(render_surface_id);

  if (!render_surface->texture()->
          InstallFrameBufferObjects(render_surface) ||
      !BindDepthStencilBuffer(depth_surface)) {
    return parse_error::kParseInvalidArguments;
  }

  // RenderSurface rendering is performed with an inverted Y, so the front
  // face winding must be changed to clock-wise.  See comments for
  // UpdateHelperConstant.
  glFrontFace(GL_CW);

  current_surface_id_ = render_surface_id;
  current_depth_surface_id_ = depth_stencil_id;
  return parse_error::kParseNoError;
}

void GAPIGL::SetBackSurfaces() {
  // Bind the default context, and restore the default front-face winding.
  ::glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  glFrontFace(GL_CCW);
}

}  // namespace o3d
}  // namespace command_buffer
}  // namespace o3d

