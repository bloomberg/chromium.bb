// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/scoped_binders.h"
#include "ui/gl/gl_bindings.h"

namespace ui {

ScopedFrameBufferBinder::ScopedFrameBufferBinder(unsigned int fbo) {
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo_);
  glBindFramebufferEXT(GL_FRAMEBUFFER, fbo);
}

ScopedFrameBufferBinder::~ScopedFrameBufferBinder() {
  glBindFramebufferEXT(GL_FRAMEBUFFER, old_fbo_);
}

ScopedTextureBinder::ScopedTextureBinder(unsigned int target, unsigned int id)
    : target_(target) {
  GLenum target_getter = 0;
  switch (target) {
  case GL_TEXTURE_2D:
    target_getter = GL_TEXTURE_BINDING_2D;
    break;
  case GL_TEXTURE_CUBE_MAP:
    target_getter = GL_TEXTURE_BINDING_CUBE_MAP;
    break;
  default:
    NOTIMPLEMENTED() << "Target not part of OpenGL ES 2.0 spec.";
  }
  glGetIntegerv(target_getter, &old_id_);
  glBindTexture(target_, id);
}

ScopedTextureBinder::~ScopedTextureBinder() {
  glBindTexture(target_, old_id_);
}

}  // namespace ui
