// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/xr/XRWebGLDrawingBuffer.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGLFramebuffer.h"
#include "modules/xr/XRSession.h"
#include "modules/xr/XRView.h"
#include "modules/xr/XRViewport.h"
#include "platform/graphics/AcceleratedStaticBitmapImage.h"
#include "platform/graphics/gpu/DrawingBuffer.h"
#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

// Large parts of this file have been shamelessly borrowed from
// platform/graphics/gpu/DrawingBuffer.cpp and simplified where applicable due
// to the more narrow use case. It may make sense in the future to abstract out
// some of the common bits into a base class?

XRWebGLDrawingBuffer* XRWebGLDrawingBuffer::Create(
    WebGLRenderingContextBase* webgl_context,
    const IntSize& size,
    bool want_alpha_channel,
    bool want_depth_buffer,
    bool want_stencil_buffer,
    bool want_antialiasing,
    bool want_multiview) {
  DCHECK(webgl_context);

  std::unique_ptr<Extensions3DUtil> extensions_util =
      Extensions3DUtil::Create(webgl_context->ContextGL());
  if (!extensions_util->IsValid()) {
    // This might be the first time we notice that the GL context is lost.
    return nullptr;
  }

  DCHECK(extensions_util->SupportsExtension("GL_OES_packed_depth_stencil"));
  extensions_util->EnsureExtensionEnabled("GL_OES_packed_depth_stencil");
  bool multisample_supported =
      want_antialiasing &&
      (extensions_util->SupportsExtension(
           "GL_CHROMIUM_framebuffer_multisample") ||
       extensions_util->SupportsExtension(
           "GL_EXT_multisampled_render_to_texture")) &&
      extensions_util->SupportsExtension("GL_OES_rgb8_rgba8");
  if (multisample_supported) {
    extensions_util->EnsureExtensionEnabled("GL_OES_rgb8_rgba8");
    if (extensions_util->SupportsExtension(
            "GL_CHROMIUM_framebuffer_multisample")) {
      extensions_util->EnsureExtensionEnabled(
          "GL_CHROMIUM_framebuffer_multisample");
    } else {
      extensions_util->EnsureExtensionEnabled(
          "GL_EXT_multisampled_render_to_texture");
    }
  }
  bool discard_framebuffer_supported =
      extensions_util->SupportsExtension("GL_EXT_discard_framebuffer");
  if (discard_framebuffer_supported)
    extensions_util->EnsureExtensionEnabled("GL_EXT_discard_framebuffer");

  // TODO(bajones): Support multiview.
  bool multiview_supported = false;

  XRWebGLDrawingBuffer* drawing_buffer = new XRWebGLDrawingBuffer(
      webgl_context, discard_framebuffer_supported, want_alpha_channel,
      want_depth_buffer, want_stencil_buffer, multiview_supported);
  if (!drawing_buffer->Initialize(size, multisample_supported,
                                  multiview_supported)) {
    DLOG(ERROR) << "XRWebGLDrawingBuffer Initialization Failed";
    return nullptr;
  }

  return drawing_buffer;
}

XRWebGLDrawingBuffer::XRWebGLDrawingBuffer(
    WebGLRenderingContextBase* webgl_context,
    bool discard_framebuffer_supported,
    bool want_alpha_channel,
    bool want_depth_buffer,
    bool want_stencil_buffer,
    bool multiview_supported)
    : webgl_context_(webgl_context),
      antialias_(false),
      depth_(want_depth_buffer),
      stencil_(want_stencil_buffer),
      alpha_(want_alpha_channel),
      multiview_(false) {
  contents_changed_ = true;  // TODO: Obviously something better than this.
}

// TODO(bajones): The GL resources allocated in this function are leaking. Add
// a way to clean up the buffers when the layer is GCed or the session ends.
bool XRWebGLDrawingBuffer::Initialize(const IntSize& size,
                                      bool use_multisampling,
                                      bool use_multiview) {
  gpu::gles2::GLES2Interface* gl = gl_context();

  // Determine the WebGL version of the context
  switch (webgl_context_->Version()) {
    case 1:
      webgl_version_ = kWebGL1;
      break;
    case 2:
      webgl_version_ = kWebGL2;
      break;
    default:
      NOTREACHED();
      break;
  }

  std::unique_ptr<Extensions3DUtil> extensions_util =
      Extensions3DUtil::Create(gl);

  // Check context capabilities
  int max_sample_count = 0;
  anti_aliasing_mode_ = kNone;
  if (use_multisampling) {
    gl->GetIntegerv(GL_MAX_SAMPLES_ANGLE, &max_sample_count);
    anti_aliasing_mode_ = kMSAAExplicitResolve;
    if (extensions_util->SupportsExtension(
            "GL_EXT_multisampled_render_to_texture")) {
      anti_aliasing_mode_ = kMSAAImplicitResolve;
    } else if (extensions_util->SupportsExtension(
                   "GL_CHROMIUM_screen_space_antialiasing")) {
      anti_aliasing_mode_ = kScreenSpaceAntialiasing;
    }
  }

  storage_texture_supported_ =
      (webgl_version_ > kWebGL1 ||
       extensions_util->SupportsExtension("GL_EXT_texture_storage")) &&
      anti_aliasing_mode_ == kScreenSpaceAntialiasing;
  sample_count_ = std::min(4, max_sample_count);

  // Create an opaque WebGL Framebuffer
  framebuffer_ = WebGLFramebuffer::CreateOpaque(webgl_context_);

  Resize(size);

  if (gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    // It's possible that the drawing buffer allocation provokes a context loss,
    // so check again just in case.
    return false;
  }

  return true;
}

void XRWebGLDrawingBuffer::Resize(const IntSize& new_size) {
  if (webgl_context_->isContextLost())
    return;

  // Ensure we always have at least a 1x1 buffer
  IntSize adjusted_size(std::max(1, new_size.Width()),
                        std::max(1, new_size.Height()));

  if (adjusted_size == size_)
    return;

  size_ = adjusted_size;

  gpu::gles2::GLES2Interface* gl = gl_context();

  gl->BindFramebuffer(GL_FRAMEBUFFER, framebuffer_->Object());

  // Provide a depth and/or stencil buffer if requested.
  if (depth_ || stencil_) {
    if (depth_stencil_buffer_) {
      gl->DeleteRenderbuffers(1, &depth_stencil_buffer_);
      depth_stencil_buffer_ = 0;
    }
    gl->GenRenderbuffers(1, &depth_stencil_buffer_);
    gl->BindRenderbuffer(GL_RENDERBUFFER, depth_stencil_buffer_);

    if (anti_aliasing_mode_ == kMSAAImplicitResolve) {
      gl->RenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, sample_count_,
                                            GL_DEPTH24_STENCIL8_OES,
                                            size_.Width(), size_.Height());
    } else if (anti_aliasing_mode_ == kMSAAExplicitResolve) {
      gl->RenderbufferStorageMultisampleCHROMIUM(GL_RENDERBUFFER, sample_count_,
                                                 GL_DEPTH24_STENCIL8_OES,
                                                 size_.Width(), size_.Height());
    } else {
      gl->RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES,
                              size_.Width(), size_.Height());
    }

    gl->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, depth_stencil_buffer_);
  }

  if (WantExplicitResolve()) {
    // If we're doing an explicit multisample resolve use the main framebuffer
    // as the multisample target and resolve into resolved_fbo_ when needed.
    GLenum multisample_format = alpha_ ? GL_RGBA8_OES : GL_RGB8_OES;

    if (multisample_renderbuffer_) {
      gl->DeleteRenderbuffers(1, &multisample_renderbuffer_);
      multisample_renderbuffer_ = 0;
    }

    gl->GenRenderbuffers(1, &multisample_renderbuffer_);
    gl->BindRenderbuffer(GL_RENDERBUFFER, multisample_renderbuffer_);
    gl->RenderbufferStorageMultisampleCHROMIUM(GL_RENDERBUFFER, sample_count_,
                                               multisample_format,
                                               size_.Width(), size_.Height());

    gl->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_RENDERBUFFER, multisample_renderbuffer_);

    // Now bind the resolve target framebuffer to attach the color textures to.
    if (!resolved_framebuffer_) {
      gl->GenFramebuffers(1, &resolved_framebuffer_);
    }
    gl->BindFramebuffer(GL_FRAMEBUFFER, resolved_framebuffer_);
  }

  if (back_color_buffer_) {
    gl->DeleteTextures(1, &back_color_buffer_);
    back_color_buffer_ = 0;
  }
  if (front_color_buffer_) {
    gl->DeleteTextures(1, &front_color_buffer_);
    front_color_buffer_ = 0;
  }

  back_color_buffer_ = CreateColorBuffer();
  front_color_buffer_ = CreateColorBuffer();

  if (anti_aliasing_mode_ == kMSAAImplicitResolve) {
    gl->FramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                           GL_TEXTURE_2D, back_color_buffer_, 0,
                                           sample_count_);
  } else {
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, back_color_buffer_, 0);
  }

  if (gl->CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    DLOG(ERROR) << "Framebuffer incomplete";
  }

  DrawingBuffer::Client* client =
      static_cast<DrawingBuffer::Client*>(webgl_context_.Get());
  client->DrawingBufferClientRestoreRenderbufferBinding();
  client->DrawingBufferClientRestoreFramebufferBinding();
}

void XRWebGLDrawingBuffer::MarkFramebufferComplete(bool complete) {
  framebuffer_->MarkOpaqueBufferComplete(complete);
}

GLuint XRWebGLDrawingBuffer::CreateColorBuffer() {
  gpu::gles2::GLES2Interface* gl = gl_context();

  GLuint texture_id = 0;
  gl->GenTextures(1, &texture_id);
  gl->BindTexture(GL_TEXTURE_2D, texture_id);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (storage_texture_supported_) {
    GLenum internal_storage_format = alpha_ ? GL_RGBA8 : GL_RGB8;
    gl->TexStorage2DEXT(GL_TEXTURE_2D, 1, internal_storage_format,
                        size_.Width(), size_.Height());
  } else {
    GLenum gl_format = alpha_ ? GL_RGBA : GL_RGB;
    gl->TexImage2D(GL_TEXTURE_2D, 0, gl_format, size_.Width(), size_.Height(),
                   0, gl_format, GL_UNSIGNED_BYTE, nullptr);
  }

  DrawingBuffer::Client* client =
      static_cast<DrawingBuffer::Client*>(webgl_context_.Get());
  client->DrawingBufferClientRestoreTexture2DBinding();

  return texture_id;
}

bool XRWebGLDrawingBuffer::WantExplicitResolve() const {
  return anti_aliasing_mode_ == kMSAAExplicitResolve;
}

// Swap the front and back buffers. After this call the front buffer should
// contain the previously rendered content, resolved from the multisample
// renderbuffer if needed.
void XRWebGLDrawingBuffer::SwapColorBuffers() {
  if (webgl_context_->isContextLost())
    return;

  gpu::gles2::GLES2Interface* gl = gl_context();

  DrawingBuffer::Client* client =
      static_cast<DrawingBuffer::Client*>(webgl_context_.Get());

  // Resolve multisample buffers if needed
  if (WantExplicitResolve()) {
    gl->BindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, framebuffer_->Object());
    gl->BindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, resolved_framebuffer_);
    gl->Disable(GL_SCISSOR_TEST);

    int width = size_.Width();
    int height = size_.Height();
    // Use NEAREST, because there is no scale performed during the blit.
    gl->BlitFramebufferCHROMIUM(0, 0, width, height, 0, 0, width, height,
                                GL_COLOR_BUFFER_BIT, GL_NEAREST);

    gl->BindFramebuffer(GL_FRAMEBUFFER, resolved_framebuffer_);

    client->DrawingBufferClientRestoreScissorTest();
  } else {
    gl->BindFramebuffer(GL_FRAMEBUFFER, framebuffer_->Object());
    if (anti_aliasing_mode_ == kScreenSpaceAntialiasing)
      gl->ApplyScreenSpaceAntialiasingCHROMIUM();
  }

  // Swap buffers
  GLuint tmp = back_color_buffer_;
  back_color_buffer_ = front_color_buffer_;
  front_color_buffer_ = tmp;

  if (anti_aliasing_mode_ == kMSAAImplicitResolve) {
    gl->FramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                           GL_TEXTURE_2D, back_color_buffer_, 0,
                                           sample_count_);
  } else {
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, back_color_buffer_, 0);
  }

  client->DrawingBufferClientRestoreFramebufferBinding();
}

void XRWebGLDrawingBuffer::Trace(blink::Visitor* visitor) {
  visitor->Trace(webgl_context_);
  visitor->Trace(framebuffer_);
}

void XRWebGLDrawingBuffer::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(webgl_context_);
}

}  // namespace blink
