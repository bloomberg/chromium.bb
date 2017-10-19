// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLTransformFeedback_h
#define WebGLTransformFeedback_h

#include "modules/webgl/WebGLContextObject.h"
#include "modules/webgl/WebGLProgram.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class WebGL2RenderingContextBase;
class WebGLBuffer;

class WebGLTransformFeedback : public WebGLContextObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum TFType {
    TFTypeDefault,
    TFTypeUser,
  };

  ~WebGLTransformFeedback() override;

  GLuint Object() const { return object_; }

  static WebGLTransformFeedback* Create(WebGL2RenderingContextBase*, TFType);

  bool IsDefaultObject() const { return type_ == TFTypeDefault; }

  GLenum GetTarget() const { return target_; }
  void SetTarget(GLenum);

  bool HasEverBeenBound() const { return object_ && target_; }

  WebGLProgram* GetProgram() const { return program_; }
  void SetProgram(WebGLProgram*);

  // This is the generic bind point for the transform feedback buffer.
  void SetBoundTransformFeedbackBuffer(WebGLBuffer*);
  WebGLBuffer* GetBoundTransformFeedbackBuffer() const;

  // These are the indexed bind points for transform feedback buffers.
  // Returns false if index is out of range and the caller should
  // synthesize a GL error.
  bool SetBoundIndexedTransformFeedbackBuffer(GLuint index, WebGLBuffer*);
  bool GetBoundIndexedTransformFeedbackBuffer(GLuint index,
                                              WebGLBuffer** outBuffer) const;

  bool IsBufferBoundToTransformFeedback(WebGLBuffer*);

  void UnbindBuffer(WebGLBuffer*);

  virtual void Trace(blink::Visitor*);
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 protected:
  explicit WebGLTransformFeedback(WebGL2RenderingContextBase*, TFType);

 private:
  void DispatchDetached(gpu::gles2::GLES2Interface*);
  bool HasObject() const override { return object_ != 0; }
  void DeleteObjectImpl(gpu::gles2::GLES2Interface*) override;

  GLuint object_;

  TFType type_;
  GLenum target_;

  TraceWrapperMember<WebGLBuffer> bound_transform_feedback_buffer_;
  HeapVector<TraceWrapperMember<WebGLBuffer>>
      bound_indexed_transform_feedback_buffers_;

  Member<WebGLProgram> program_;
};

}  // namespace blink

#endif  // WebGLTransformFeedback_h
