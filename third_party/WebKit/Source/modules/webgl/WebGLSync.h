// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLSync_h
#define WebGLSync_h

#include "modules/webgl/WebGLSharedObject.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace blink {

class WebGL2RenderingContextBase;

class WebGLSync : public WebGLSharedObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~WebGLSync() override;

  GLsync Object() const { return object_; }

 protected:
  WebGLSync(WebGL2RenderingContextBase*, GLsync, GLenum object_type);

  bool HasObject() const override { return object_ != nullptr; }
  void DeleteObjectImpl(gpu::gles2::GLES2Interface*) override;

  GLenum ObjectType() const { return object_type_; }

 private:
  bool IsSync() const override { return true; }

  GLsync object_;
  GLenum object_type_;
};

}  // namespace blink

#endif  // WebGLSync_h
