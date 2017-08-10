// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLVertexArrayObjectBase.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

WebGLVertexArrayObjectBase::WebGLVertexArrayObjectBase(
    WebGLRenderingContextBase* ctx,
    VaoType type)
    : WebGLContextObject(ctx),
      object_(0),
      type_(type),
      has_ever_been_bound_(false),
      is_all_enabled_attrib_buffer_bound_(true) {
  array_buffer_list_.resize(ctx->MaxVertexAttribs());
  attrib_enabled_.resize(ctx->MaxVertexAttribs());
  for (size_t i = 0; i < attrib_enabled_.size(); ++i) {
    attrib_enabled_[i] = false;
  }

  switch (type_) {
    case kVaoTypeDefault:
      break;
    default:
      Context()->ContextGL()->GenVertexArraysOES(1, &object_);
      break;
  }
}

WebGLVertexArrayObjectBase::~WebGLVertexArrayObjectBase() {
  RunDestructor();
}

void WebGLVertexArrayObjectBase::DispatchDetached(
    gpu::gles2::GLES2Interface* gl) {
  if (bound_element_array_buffer_)
    bound_element_array_buffer_->OnDetached(gl);

  for (size_t i = 0; i < array_buffer_list_.size(); ++i) {
    if (array_buffer_list_[i])
      array_buffer_list_[i]->OnDetached(gl);
  }
}

void WebGLVertexArrayObjectBase::DeleteObjectImpl(
    gpu::gles2::GLES2Interface* gl) {
  switch (type_) {
    case kVaoTypeDefault:
      break;
    default:
      gl->DeleteVertexArraysOES(1, &object_);
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

void WebGLVertexArrayObjectBase::SetElementArrayBuffer(WebGLBuffer* buffer) {
  if (buffer)
    buffer->OnAttached();
  if (bound_element_array_buffer_)
    bound_element_array_buffer_->OnDetached(Context()->ContextGL());
  bound_element_array_buffer_ = buffer;
}

WebGLBuffer* WebGLVertexArrayObjectBase::GetArrayBufferForAttrib(size_t index) {
  DCHECK(index < Context()->MaxVertexAttribs());
  return array_buffer_list_[index].Get();
}

void WebGLVertexArrayObjectBase::SetArrayBufferForAttrib(GLuint index,
                                                         WebGLBuffer* buffer) {
  if (buffer)
    buffer->OnAttached();
  if (array_buffer_list_[index])
    array_buffer_list_[index]->OnDetached(Context()->ContextGL());

  array_buffer_list_[index] = buffer;
  UpdateAttribBufferBoundStatus();
}

void WebGLVertexArrayObjectBase::SetAttribEnabled(GLuint index, bool enabled) {
  DCHECK(index < Context()->MaxVertexAttribs());
  attrib_enabled_[index] = enabled;
  UpdateAttribBufferBoundStatus();
}

bool WebGLVertexArrayObjectBase::GetAttribEnabled(GLuint index) const {
  DCHECK(index < Context()->MaxVertexAttribs());
  return attrib_enabled_[index];
}

void WebGLVertexArrayObjectBase::UpdateAttribBufferBoundStatus() {
  is_all_enabled_attrib_buffer_bound_ = true;
  for (size_t i = 0; i < attrib_enabled_.size(); ++i) {
    if (attrib_enabled_[i] && !array_buffer_list_[i]) {
      is_all_enabled_attrib_buffer_bound_ = false;
      return;
    }
  }
}

void WebGLVertexArrayObjectBase::UnbindBuffer(WebGLBuffer* buffer) {
  if (bound_element_array_buffer_ == buffer) {
    bound_element_array_buffer_->OnDetached(Context()->ContextGL());
    bound_element_array_buffer_ = nullptr;
  }

  for (size_t i = 0; i < array_buffer_list_.size(); ++i) {
    if (array_buffer_list_[i] == buffer) {
      array_buffer_list_[i]->OnDetached(Context()->ContextGL());
      array_buffer_list_[i] = nullptr;
    }
  }
  UpdateAttribBufferBoundStatus();
}

DEFINE_TRACE(WebGLVertexArrayObjectBase) {
  visitor->Trace(bound_element_array_buffer_);
  visitor->Trace(array_buffer_list_);
  WebGLContextObject::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(WebGLVertexArrayObjectBase) {
  visitor->TraceWrappers(bound_element_array_buffer_);
  for (size_t i = 0; i < array_buffer_list_.size(); ++i) {
    visitor->TraceWrappers(array_buffer_list_[i]);
  }
  WebGLContextObject::TraceWrappers(visitor);
}

}  // namespace blink
