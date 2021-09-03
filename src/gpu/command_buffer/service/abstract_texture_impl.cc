// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/abstract_texture_impl.h"

#include <utility>

#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {
namespace gles2 {

AbstractTextureImpl::AbstractTextureImpl(GLenum target,
                                         GLenum internal_format,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLint border,
                                         GLenum format,
                                         GLenum type) {
  // Create a gles2 Texture.
  GLuint service_id = 0;
  api_ = gl::g_current_gl_context;
  api_->glGenTexturesFn(1, &service_id);
  gl::ScopedTextureBinder binder(target, service_id);
  api_->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api_->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api_->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api_->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  texture_ = new gpu::gles2::Texture(service_id);
  texture_->SetLightweightRef();
  texture_->SetTarget(target, 1);
  texture_->set_min_filter(GL_LINEAR);
  texture_->set_mag_filter(GL_LINEAR);
  texture_->set_wrap_t(GL_CLAMP_TO_EDGE);
  texture_->set_wrap_s(GL_CLAMP_TO_EDGE);
  gfx::Rect cleared_rect;
  texture_->SetLevelInfo(target, 0, internal_format, width, height, depth,
                         border, format, type, cleared_rect);
  texture_->SetImmutable(true, false);
}

AbstractTextureImpl::~AbstractTextureImpl() {
  // If context is not lost, then the texture should be destroyed on same
  // context it was create on.
  if (have_context_)
    DCHECK_EQ(api_, gl::g_current_gl_context);

  texture_->RemoveLightweightRef(have_context_);
}

TextureBase* AbstractTextureImpl::GetTextureBase() const {
  return texture_;
}

void AbstractTextureImpl::SetParameteri(GLenum pname, GLint param) {
  NOTIMPLEMENTED();
}

void AbstractTextureImpl::BindStreamTextureImage(gl::GLImage* image,
                                                 GLuint service_id) {
  const GLint level = 0;
  const GLuint target = texture_->target();
  texture_->SetLevelStreamTextureImage(
      target, level, image, Texture::ImageState::UNBOUND, service_id);
  texture_->SetLevelCleared(target, level, true);
}

void AbstractTextureImpl::BindImage(gl::GLImage* image, bool client_managed) {
  NOTIMPLEMENTED();
}

gl::GLImage* AbstractTextureImpl::GetImage() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void AbstractTextureImpl::SetCleared() {
  NOTIMPLEMENTED();
}

void AbstractTextureImpl::SetCleanupCallback(CleanupCallback cb) {
  NOTIMPLEMENTED();
}

void AbstractTextureImpl::NotifyOnContextLost() {
  have_context_ = false;
}

AbstractTextureImplPassthrough::AbstractTextureImplPassthrough(
    GLenum target,
    GLenum internal_format,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type) {
  // Create a gles2 Texture.
  GLuint service_id = 0;
  api_ = gl::g_current_gl_context;
  api_->glGenTexturesFn(1, &service_id);

  GLint prev_texture = 0;
  api_->glGetIntegervFn(GetTextureBindingQuery(target), &prev_texture);

  api_->glBindTextureFn(target, service_id);
  api_->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api_->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api_->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api_->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(target, prev_texture);

  texture_ = new TexturePassthrough(service_id, target, internal_format, width,
                                    height, depth, border, format, type);
}

AbstractTextureImplPassthrough::~AbstractTextureImplPassthrough() {
  // If context is not lost, then the texture should be destroyed on the same
  // context it was create on.
  if (have_context_)
    DCHECK_EQ(api_, gl::g_current_gl_context);
}

TextureBase* AbstractTextureImplPassthrough::GetTextureBase() const {
  return texture_.get();
}

void AbstractTextureImplPassthrough::SetParameteri(GLenum pname, GLint param) {
  NOTIMPLEMENTED();
}

void AbstractTextureImplPassthrough::BindStreamTextureImage(gl::GLImage* image,
                                                            GLuint service_id) {
  const GLint level = 0;
  const GLuint target = texture_->target();
  texture_->SetStreamLevelImage(target, level, image, service_id);
  texture_->set_is_bind_pending(true);
}

void AbstractTextureImplPassthrough::BindImage(gl::GLImage* image,
                                               bool client_managed) {
  NOTIMPLEMENTED();
}

gl::GLImage* AbstractTextureImplPassthrough::GetImage() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void AbstractTextureImplPassthrough::SetCleared() {
  NOTIMPLEMENTED();
}

void AbstractTextureImplPassthrough::SetCleanupCallback(CleanupCallback cb) {
  NOTIMPLEMENTED();
}

void AbstractTextureImplPassthrough::NotifyOnContextLost() {
  texture_->MarkContextLost();
  have_context_ = false;
}

}  // namespace gles2
}  // namespace gpu
