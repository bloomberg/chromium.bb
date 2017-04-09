// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLTransformFeedback.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGL2RenderingContextBase.h"

namespace blink {

WebGLTransformFeedback* WebGLTransformFeedback::Create(
    WebGL2RenderingContextBase* ctx) {
  return new WebGLTransformFeedback(ctx);
}

WebGLTransformFeedback::WebGLTransformFeedback(WebGL2RenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx), target_(0), program_(nullptr) {
  GLuint tf;
  ctx->ContextGL()->GenTransformFeedbacks(1, &tf);
  SetObject(tf);
}

WebGLTransformFeedback::~WebGLTransformFeedback() {
  RunDestructor();
}

void WebGLTransformFeedback::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  gl->DeleteTransformFeedbacks(1, &object_);
  object_ = 0;
}

void WebGLTransformFeedback::SetTarget(GLenum target) {
  if (target_)
    return;
  if (target == GL_TRANSFORM_FEEDBACK)
    target_ = target;
}

void WebGLTransformFeedback::SetProgram(WebGLProgram* program) {
  program_ = program;
}

DEFINE_TRACE(WebGLTransformFeedback) {
  visitor->Trace(program_);
  WebGLSharedPlatform3DObject::Trace(visitor);
}

}  // namespace blink
