// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLTransformFeedback_h
#define WebGLTransformFeedback_h

#include "modules/webgl/WebGLProgram.h"
#include "modules/webgl/WebGLSharedPlatform3DObject.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class WebGL2RenderingContextBase;

class WebGLTransformFeedback : public WebGLSharedPlatform3DObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~WebGLTransformFeedback() override;

  static WebGLTransformFeedback* Create(WebGL2RenderingContextBase*);

  GLenum GetTarget() const { return target_; }
  void SetTarget(GLenum);

  bool HasEverBeenBound() const { return Object() && target_; }

  WebGLProgram* GetProgram() const { return program_; }
  void SetProgram(WebGLProgram*);

  DECLARE_TRACE();

 protected:
  explicit WebGLTransformFeedback(WebGL2RenderingContextBase*);

  void DeleteObjectImpl(gpu::gles2::GLES2Interface*) override;

 private:
  bool IsTransformFeedback() const override { return true; }

  GLenum target_;

  Member<WebGLProgram> program_;
};

}  // namespace blink

#endif  // WebGLTransformFeedback_h
