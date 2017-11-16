// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLSharedPlatform3DObject_h
#define WebGLSharedPlatform3DObject_h

#include "base/memory/scoped_refptr.h"
#include "modules/webgl/WebGLSharedObject.h"

namespace blink {

class WebGLRenderingContextBase;

class WebGLSharedPlatform3DObject : public WebGLSharedObject {
 public:
  GLuint Object() const { return object_; }
  void SetObject(GLuint);

 protected:
  explicit WebGLSharedPlatform3DObject(WebGLRenderingContextBase*);

  bool HasObject() const override;

  GLuint object_;
};

}  // namespace blink

#endif  // WebGLSharedPlatform3DObject_h
