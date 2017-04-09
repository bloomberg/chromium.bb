// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLSampler.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGL2RenderingContextBase.h"

namespace blink {

WebGLSampler* WebGLSampler::Create(WebGL2RenderingContextBase* ctx) {
  return new WebGLSampler(ctx);
}

WebGLSampler::WebGLSampler(WebGL2RenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx) {
  GLuint sampler;
  ctx->ContextGL()->GenSamplers(1, &sampler);
  SetObject(sampler);
}

WebGLSampler::~WebGLSampler() {
  RunDestructor();
}

void WebGLSampler::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  gl->DeleteSamplers(1, &object_);
  object_ = 0;
}

}  // namespace blink
