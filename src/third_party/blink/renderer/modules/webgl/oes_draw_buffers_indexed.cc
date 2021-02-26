// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/oes_draw_buffers_indexed.h"

namespace blink {

OESDrawBuffersIndexed::OESDrawBuffersIndexed(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_OES_draw_buffers_indexed");
}

WebGLExtensionName OESDrawBuffersIndexed::GetName() const {
  return kOESDrawBuffersIndexed;
}

bool OESDrawBuffersIndexed::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_OES_draw_buffers_indexed");
}

const char* OESDrawBuffersIndexed::ExtensionName() {
  return "OES_draw_buffers_indexed";
}

void OESDrawBuffersIndexed::enableiOES(GLenum target, GLuint index) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;
  scoped.Context()->ContextGL()->EnableiOES(target, index);
}

void OESDrawBuffersIndexed::disableiOES(GLenum target, GLuint index) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;
  scoped.Context()->ContextGL()->DisableiOES(target, index);
}

void OESDrawBuffersIndexed::blendEquationiOES(GLuint buf, GLenum mode) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;
  scoped.Context()->ContextGL()->BlendEquationiOES(buf, mode);
}

void OESDrawBuffersIndexed::blendEquationSeparateiOES(GLuint buf,
                                                      GLenum modeRGB,
                                                      GLenum modeAlpha) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;
  scoped.Context()->ContextGL()->BlendEquationSeparateiOES(buf, modeRGB,
                                                           modeAlpha);
}

void OESDrawBuffersIndexed::blendFunciOES(GLuint buf, GLenum src, GLenum dst) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;
  scoped.Context()->ContextGL()->BlendFunciOES(buf, src, dst);
}

void OESDrawBuffersIndexed::blendFuncSeparateiOES(GLuint buf,
                                                  GLenum srcRGB,
                                                  GLenum dstRGB,
                                                  GLenum srcAlpha,
                                                  GLenum dstAlpha) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;
  scoped.Context()->ContextGL()->BlendFuncSeparateiOES(buf, srcRGB, dstRGB,
                                                       srcAlpha, dstAlpha);
}

void OESDrawBuffersIndexed::colorMaskiOES(GLuint buf,
                                          GLboolean r,
                                          GLboolean g,
                                          GLboolean b,
                                          GLboolean a) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;
  scoped.Context()->ContextGL()->ColorMaskiOES(buf, r, g, b, a);
}

GLboolean OESDrawBuffersIndexed::isEnablediOES(GLenum target, GLuint index) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return 0;
  return scoped.Context()->ContextGL()->IsEnablediOES(target, index);
}

}  // namespace blink
