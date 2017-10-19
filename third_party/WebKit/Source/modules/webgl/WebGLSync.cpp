// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLSync.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGL2RenderingContextBase.h"

namespace blink {

WebGLSync::WebGLSync(WebGL2RenderingContextBase* ctx,
                     GLsync object,
                     GLenum object_type)
    : WebGLSharedObject(ctx), object_(object), object_type_(object_type) {}

WebGLSync::~WebGLSync() {
  RunDestructor();
}

void WebGLSync::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  gl->DeleteSync(object_);
  object_ = nullptr;
}

}  // namespace blink
