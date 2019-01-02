// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_multiview.h"

#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLMultiview::WebGLMultiview(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_ANGLE_multiview");
  context->ContextGL()->GetIntegerv(GL_MAX_VIEWS_OVR, &max_views_ovr_);
}

WebGLExtensionName WebGLMultiview::GetName() const {
  return kWebGLMultiviewName;
}

WebGLMultiview* WebGLMultiview::Create(WebGLRenderingContextBase* context) {
  return new WebGLMultiview(context);
}

void WebGLMultiview::framebufferTextureMultiviewWEBGL(GLenum target,
                                                      GLenum attachment,
                                                      WebGLTexture* texture,
                                                      GLint level,
                                                      GLint base_view_index,
                                                      GLsizei num_views) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;
  if (texture &&
      !texture->Validate(scoped.Context()->ContextGroup(), scoped.Context())) {
    scoped.Context()->SynthesizeGLError(
        GL_INVALID_OPERATION, "framebufferTextureMultiviewWEBGL",
        "texture does not belong to this context");
    return;
  }
  GLenum textarget = texture ? texture->GetTarget() : 0;
  if (texture) {
    if (textarget != GL_TEXTURE_2D_ARRAY) {
      scoped.Context()->SynthesizeGLError(GL_INVALID_OPERATION,
                                          "framebufferTextureMultiviewWEBGL",
                                          "invalid texture type");
      return;
    }
    if (num_views < 1) {
      scoped.Context()->SynthesizeGLError(GL_INVALID_VALUE,
                                          "framebufferTextureMultiviewWEBGL",
                                          "numViews is less than one");
      return;
    }
    if (num_views > max_views_ovr_) {
      scoped.Context()->SynthesizeGLError(
          GL_INVALID_VALUE, "framebufferTextureMultiviewWEBGL",
          "numViews is more than the value of MAX_VIEWS_OVR");
      return;
    }
    if (!static_cast<WebGL2RenderingContextBase*>(scoped.Context())
             ->ValidateTexFuncLayer("framebufferTextureMultiviewWEBGL",
                                    textarget, base_view_index))
      return;
    if (!static_cast<WebGL2RenderingContextBase*>(scoped.Context())
             ->ValidateTexFuncLayer("framebufferTextureMultiviewWEBGL",
                                    textarget, base_view_index + num_views - 1))
      return;
    if (!scoped.Context()->ValidateTexFuncLevel(
            "framebufferTextureMultiviewWEBGL", textarget, level))
      return;
  }

  WebGLFramebuffer* framebuffer_binding =
      scoped.Context()->GetFramebufferBinding(target);
  if (!framebuffer_binding || !framebuffer_binding->Object()) {
    scoped.Context()->SynthesizeGLError(GL_INVALID_OPERATION,
                                        "framebufferTextureMultiviewWEBGL",
                                        "no framebuffer bound");
    return;
  }
  // Don't allow modifications to opaque framebuffer attachements.
  if (framebuffer_binding->Opaque()) {
    scoped.Context()->SynthesizeGLError(GL_INVALID_OPERATION,
                                        "framebufferTextureMultiviewWEBGL",
                                        "opaque framebuffer bound");
    return;
  }

  framebuffer_binding->SetAttachmentForBoundFramebuffer(
      target, attachment, textarget, texture, level, base_view_index,
      num_views);
  scoped.Context()->ApplyStencilTest();
}

bool WebGLMultiview::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension("GL_ANGLE_multiview");
}

const char* WebGLMultiview::ExtensionName() {
  return "WEBGL_multiview";
}

}  // namespace blink
