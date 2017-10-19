// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLTransformFeedback.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGL2RenderingContextBase.h"

namespace blink {

WebGLTransformFeedback* WebGLTransformFeedback::Create(
    WebGL2RenderingContextBase* ctx,
    TFType type) {
  return new WebGLTransformFeedback(ctx, type);
}

WebGLTransformFeedback::WebGLTransformFeedback(WebGL2RenderingContextBase* ctx,
                                               TFType type)
    : WebGLContextObject(ctx),
      object_(0),
      type_(type),
      target_(0),
      program_(nullptr) {
  GLint max_attribs = ctx->GetMaxTransformFeedbackSeparateAttribs();
  DCHECK_GE(max_attribs, 0);
  bound_indexed_transform_feedback_buffers_.resize(max_attribs);

  switch (type_) {
    case TFTypeDefault:
      break;
    case TFTypeUser: {
      GLuint tf;
      ctx->ContextGL()->GenTransformFeedbacks(1, &tf);
      object_ = tf;
      break;
    }
  }
}

WebGLTransformFeedback::~WebGLTransformFeedback() {
  RunDestructor();
}

void WebGLTransformFeedback::DispatchDetached(gpu::gles2::GLES2Interface* gl) {
  if (bound_transform_feedback_buffer_)
    bound_transform_feedback_buffer_->OnDetached(gl);

  for (size_t i = 0; i < bound_indexed_transform_feedback_buffers_.size();
       ++i) {
    if (bound_indexed_transform_feedback_buffers_[i])
      bound_indexed_transform_feedback_buffers_[i]->OnDetached(gl);
  }
}

void WebGLTransformFeedback::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  switch (type_) {
    case TFTypeDefault:
      break;
    case TFTypeUser:
      gl->DeleteTransformFeedbacks(1, &object_);
      object_ = 0;
      break;
  }

  // Member<> objects must not be accessed during the destruction,
  // since they could have been already finalized.
  // The finalizers of these objects will handle their detachment
  // by themselves.
  if (!DestructionInProgress())
    DispatchDetached(gl);
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

void WebGLTransformFeedback::SetBoundTransformFeedbackBuffer(
    WebGLBuffer* buffer) {
  if (buffer)
    buffer->OnAttached();
  if (bound_transform_feedback_buffer_)
    bound_transform_feedback_buffer_->OnDetached(Context()->ContextGL());
  bound_transform_feedback_buffer_ = buffer;
}

WebGLBuffer* WebGLTransformFeedback::GetBoundTransformFeedbackBuffer() const {
  return bound_transform_feedback_buffer_;
}

bool WebGLTransformFeedback::SetBoundIndexedTransformFeedbackBuffer(
    GLuint index,
    WebGLBuffer* buffer) {
  if (index >= bound_indexed_transform_feedback_buffers_.size())
    return false;
  if (buffer)
    buffer->OnAttached();
  if (bound_indexed_transform_feedback_buffers_[index]) {
    bound_indexed_transform_feedback_buffers_[index]->OnDetached(
        Context()->ContextGL());
  }
  bound_indexed_transform_feedback_buffers_[index] = buffer;
  // This also sets the generic binding point in the OpenGL state.
  SetBoundTransformFeedbackBuffer(buffer);
  return true;
}

bool WebGLTransformFeedback::GetBoundIndexedTransformFeedbackBuffer(
    GLuint index,
    WebGLBuffer** outBuffer) const {
  if (index >= bound_indexed_transform_feedback_buffers_.size())
    return false;
  *outBuffer = bound_indexed_transform_feedback_buffers_[index].Get();
  return true;
}

bool WebGLTransformFeedback::IsBufferBoundToTransformFeedback(
    WebGLBuffer* buffer) {
  if (bound_transform_feedback_buffer_ == buffer)
    return true;

  for (size_t i = 0; i < bound_indexed_transform_feedback_buffers_.size();
       ++i) {
    if (bound_indexed_transform_feedback_buffers_[i] == buffer)
      return true;
  }
  return false;
}

void WebGLTransformFeedback::UnbindBuffer(WebGLBuffer* buffer) {
  if (bound_transform_feedback_buffer_ == buffer) {
    bound_transform_feedback_buffer_->OnDetached(Context()->ContextGL());
    bound_transform_feedback_buffer_ = nullptr;
  }
  for (size_t i = 0; i < bound_indexed_transform_feedback_buffers_.size();
       ++i) {
    if (bound_indexed_transform_feedback_buffers_[i] == buffer) {
      bound_indexed_transform_feedback_buffers_[i]->OnDetached(
          Context()->ContextGL());
      bound_indexed_transform_feedback_buffers_[i] = nullptr;
    }
  }
}

void WebGLTransformFeedback::Trace(blink::Visitor* visitor) {
  visitor->Trace(bound_transform_feedback_buffer_);
  visitor->Trace(bound_indexed_transform_feedback_buffers_);
  visitor->Trace(program_);
  WebGLContextObject::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(WebGLTransformFeedback) {
  visitor->TraceWrappers(bound_transform_feedback_buffer_);
  for (auto& buf : bound_indexed_transform_feedback_buffers_) {
    visitor->TraceWrappers(buf);
  }
  WebGLContextObject::TraceWrappers(visitor);
}

}  // namespace blink
