/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webgl/WebGLDrawBuffers.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGLFramebuffer.h"

namespace blink {

WebGLDrawBuffers::WebGLDrawBuffers(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->extensionsUtil()->ensureExtensionEnabled("GL_EXT_draw_buffers");
}

WebGLExtensionName WebGLDrawBuffers::name() const {
  return WebGLDrawBuffersName;
}

WebGLDrawBuffers* WebGLDrawBuffers::create(WebGLRenderingContextBase* context) {
  return new WebGLDrawBuffers(context);
}

// static
bool WebGLDrawBuffers::supported(WebGLRenderingContextBase* context) {
  return context->extensionsUtil()->supportsExtension("GL_EXT_draw_buffers");
}

// static
const char* WebGLDrawBuffers::extensionName() {
  return "WEBGL_draw_buffers";
}

void WebGLDrawBuffers::drawBuffersWEBGL(const Vector<GLenum>& buffers) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.isLost())
    return;
  GLsizei n = buffers.size();
  const GLenum* bufs = buffers.data();
  if (!scoped.context()->m_framebufferBinding) {
    if (n != 1) {
      scoped.context()->synthesizeGLError(GL_INVALID_OPERATION,
                                          "drawBuffersWEBGL",
                                          "must provide exactly one buffer");
      return;
    }
    if (bufs[0] != GL_BACK && bufs[0] != GL_NONE) {
      scoped.context()->synthesizeGLError(GL_INVALID_OPERATION,
                                          "drawBuffersWEBGL", "BACK or NONE");
      return;
    }
    // Because the backbuffer is simulated on all current WebKit ports, we need
    // to change BACK to COLOR_ATTACHMENT0.
    GLenum value = (bufs[0] == GL_BACK) ? GL_COLOR_ATTACHMENT0 : GL_NONE;
    scoped.context()->contextGL()->DrawBuffersEXT(1, &value);
    scoped.context()->setBackDrawBuffer(bufs[0]);
  } else {
    if (n > scoped.context()->maxDrawBuffers()) {
      scoped.context()->synthesizeGLError(GL_INVALID_VALUE, "drawBuffersWEBGL",
                                          "more than max draw buffers");
      return;
    }
    for (GLsizei i = 0; i < n; ++i) {
      if (bufs[i] != GL_NONE &&
          bufs[i] != static_cast<GLenum>(GL_COLOR_ATTACHMENT0_EXT + i)) {
        scoped.context()->synthesizeGLError(GL_INVALID_OPERATION,
                                            "drawBuffersWEBGL",
                                            "COLOR_ATTACHMENTi_EXT or NONE");
        return;
      }
    }
    scoped.context()->m_framebufferBinding->drawBuffers(buffers);
  }
}

}  // namespace blink
