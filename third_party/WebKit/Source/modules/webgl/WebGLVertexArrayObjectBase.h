// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLVertexArrayObjectBase_h
#define WebGLVertexArrayObjectBase_h

#include "modules/webgl/WebGLBuffer.h"
#include "modules/webgl/WebGLContextObject.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"

namespace blink {

class WebGLVertexArrayObjectBase : public WebGLContextObject {
 public:
  enum VaoType {
    kVaoTypeDefault,
    kVaoTypeUser,
  };

  ~WebGLVertexArrayObjectBase() override;

  GLuint Object() const { return object_; }

  bool IsDefaultObject() const { return type_ == kVaoTypeDefault; }

  bool HasEverBeenBound() const { return Object() && has_ever_been_bound_; }
  void SetHasEverBeenBound() { has_ever_been_bound_ = true; }

  WebGLBuffer* BoundElementArrayBuffer() const {
    return bound_element_array_buffer_;
  }
  void SetElementArrayBuffer(WebGLBuffer*);

  WebGLBuffer* GetArrayBufferForAttrib(size_t);
  void SetArrayBufferForAttrib(GLuint, WebGLBuffer*);
  void SetAttribEnabled(GLuint, bool);
  bool GetAttribEnabled(GLuint) const;
  bool IsAllEnabledAttribBufferBound() const {
    return is_all_enabled_attrib_buffer_bound_;
  }
  void UnbindBuffer(WebGLBuffer*);

  virtual void Trace(blink::Visitor*);
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 protected:
  WebGLVertexArrayObjectBase(WebGLRenderingContextBase*, VaoType);

 private:
  void DispatchDetached(gpu::gles2::GLES2Interface*);
  bool HasObject() const override { return object_ != 0; }
  void DeleteObjectImpl(gpu::gles2::GLES2Interface*) override;

  void UpdateAttribBufferBoundStatus();

  GLuint object_;

  VaoType type_;
  bool has_ever_been_bound_;
  TraceWrapperMember<WebGLBuffer> bound_element_array_buffer_;
  HeapVector<TraceWrapperMember<WebGLBuffer>> array_buffer_list_;
  Vector<bool> attrib_enabled_;
  bool is_all_enabled_attrib_buffer_bound_;
};

}  // namespace blink

#endif  // WebGLVertexArrayObjectBase_h
