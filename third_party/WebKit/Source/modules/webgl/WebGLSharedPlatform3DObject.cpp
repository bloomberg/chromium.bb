// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLSharedPlatform3DObject.h"

#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

WebGLSharedPlatform3DObject::WebGLSharedPlatform3DObject(
    WebGLRenderingContextBase* ctx)
    : WebGLSharedObject(ctx), object_(0) {}

void WebGLSharedPlatform3DObject::SetObject(GLuint object) {
  // object==0 && deleted==false indicating an uninitialized state;
  DCHECK(!object_);
  DCHECK(!IsDeleted());
  object_ = object;
}

bool WebGLSharedPlatform3DObject::HasObject() const {
  return object_ != 0;
}

}  // namespace blink
