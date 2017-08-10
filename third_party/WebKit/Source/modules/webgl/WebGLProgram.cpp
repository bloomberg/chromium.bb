/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webgl/WebGLProgram.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGLContextGroup.h"
#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

WebGLProgram* WebGLProgram::Create(WebGLRenderingContextBase* ctx) {
  return new WebGLProgram(ctx);
}

WebGLProgram::WebGLProgram(WebGLRenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx),
      link_status_(false),
      link_count_(0),
      active_transform_feedback_count_(0),
      info_valid_(true) {
  SetObject(ctx->ContextGL()->CreateProgram());
}

WebGLProgram::~WebGLProgram() {
  RunDestructor();
}

void WebGLProgram::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  gl->DeleteProgram(object_);
  object_ = 0;
  if (!DestructionInProgress()) {
    if (vertex_shader_) {
      vertex_shader_->OnDetached(gl);
      vertex_shader_ = nullptr;
    }
    if (fragment_shader_) {
      fragment_shader_->OnDetached(gl);
      fragment_shader_ = nullptr;
    }
  }
}

bool WebGLProgram::LinkStatus(WebGLRenderingContextBase* context) {
  CacheInfoIfNeeded(context);
  return link_status_;
}

void WebGLProgram::IncreaseLinkCount() {
  ++link_count_;
  info_valid_ = false;
}

void WebGLProgram::IncreaseActiveTransformFeedbackCount() {
  ++active_transform_feedback_count_;
}

void WebGLProgram::DecreaseActiveTransformFeedbackCount() {
  --active_transform_feedback_count_;
}

WebGLShader* WebGLProgram::GetAttachedShader(GLenum type) {
  switch (type) {
    case GL_VERTEX_SHADER:
      return vertex_shader_;
    case GL_FRAGMENT_SHADER:
      return fragment_shader_;
    default:
      return 0;
  }
}

bool WebGLProgram::AttachShader(WebGLShader* shader) {
  if (!shader || !shader->Object())
    return false;
  switch (shader->GetType()) {
    case GL_VERTEX_SHADER:
      if (vertex_shader_)
        return false;
      vertex_shader_ = shader;
      return true;
    case GL_FRAGMENT_SHADER:
      if (fragment_shader_)
        return false;
      fragment_shader_ = shader;
      return true;
    default:
      return false;
  }
}

bool WebGLProgram::DetachShader(WebGLShader* shader) {
  if (!shader || !shader->Object())
    return false;
  switch (shader->GetType()) {
    case GL_VERTEX_SHADER:
      if (vertex_shader_ != shader)
        return false;
      vertex_shader_ = nullptr;
      return true;
    case GL_FRAGMENT_SHADER:
      if (fragment_shader_ != shader)
        return false;
      fragment_shader_ = nullptr;
      return true;
    default:
      return false;
  }
}

void WebGLProgram::CacheInfoIfNeeded(WebGLRenderingContextBase* context) {
  if (info_valid_)
    return;
  if (!object_)
    return;
  gpu::gles2::GLES2Interface* gl = context->ContextGL();
  link_status_ = 0;
  gl->GetProgramiv(object_, GL_LINK_STATUS, &link_status_);
  info_valid_ = true;
}

DEFINE_TRACE(WebGLProgram) {
  visitor->Trace(vertex_shader_);
  visitor->Trace(fragment_shader_);
  WebGLSharedPlatform3DObject::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(WebGLProgram) {
  visitor->TraceWrappers(vertex_shader_);
  visitor->TraceWrappers(fragment_shader_);
  WebGLSharedPlatform3DObject::TraceWrappers(visitor);
}

}  // namespace blink
