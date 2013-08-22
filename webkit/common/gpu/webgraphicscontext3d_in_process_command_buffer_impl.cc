// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

#include <GLES2/gl2.h>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_surface.h"
#include "webkit/common/gpu/gl_bindings_skia_cmd_buffer.h"

using gpu::gles2::GLES2Implementation;
using gpu::GLInProcessContext;

namespace webkit {
namespace gpu {

namespace {

const int32 kCommandBufferSize = 1024 * 1024;
// TODO(kbr): make the transfer buffer size configurable via context
// creation attributes.
const size_t kStartTransferBufferSize = 4 * 1024 * 1024;
const size_t kMinTransferBufferSize = 1 * 256 * 1024;
const size_t kMaxTransferBufferSize = 16 * 1024 * 1024;

void OnSignalSyncPoint(
    WebKit::WebGraphicsContext3D::WebGraphicsSyncPointCallback* callback) {
  callback->onSyncPointReached();
}

// Singleton used to initialize and terminate the gles2 library.
class GLES2Initializer {
 public:
  GLES2Initializer() {
    ::gles2::Initialize();
  }

  ~GLES2Initializer() {
    ::gles2::Terminate();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GLES2Initializer);
};

static base::LazyInstance<GLES2Initializer> g_gles2_initializer =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace anonymous

// static
scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>
WebGraphicsContext3DInProcessCommandBufferImpl::CreateViewContext(
    const WebKit::WebGraphicsContext3D::Attributes& attributes,
    gfx::AcceleratedWidget window) {
  scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context;
  if (gfx::GLSurface::InitializeOneOff()) {
    context.reset(new WebGraphicsContext3DInProcessCommandBufferImpl(
      scoped_ptr< ::gpu::GLInProcessContext>(), attributes, false, window));
  }
  return context.Pass();
}

// static
scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>
WebGraphicsContext3DInProcessCommandBufferImpl::CreateOffscreenContext(
    const WebKit::WebGraphicsContext3D::Attributes& attributes) {
  return make_scoped_ptr(new WebGraphicsContext3DInProcessCommandBufferImpl(
                             scoped_ptr< ::gpu::GLInProcessContext>(),
                             attributes,
                             true,
                             gfx::kNullAcceleratedWidget))
      .Pass();
}

scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>
WebGraphicsContext3DInProcessCommandBufferImpl::WrapContext(
    scoped_ptr< ::gpu::GLInProcessContext> context,
    const WebKit::WebGraphicsContext3D::Attributes& attributes) {
  return make_scoped_ptr(
      new WebGraphicsContext3DInProcessCommandBufferImpl(
          context.Pass(),
          attributes,
          true /* is_offscreen. Not used. */,
          gfx::kNullAcceleratedWidget /* window. Not used. */))
      .Pass();
}

WebGraphicsContext3DInProcessCommandBufferImpl::
    WebGraphicsContext3DInProcessCommandBufferImpl(
        scoped_ptr< ::gpu::GLInProcessContext> context,
        const WebKit::WebGraphicsContext3D::Attributes& attributes,
        bool is_offscreen,
        gfx::AcceleratedWidget window)
    : is_offscreen_(is_offscreen),
      window_(window),
      initialized_(false),
      initialize_failed_(false),
      context_(context.Pass()),
      gl_(NULL),
      context_lost_callback_(NULL),
      context_lost_reason_(GL_NO_ERROR),
      attributes_(attributes),
      cached_width_(0),
      cached_height_(0) {
}

WebGraphicsContext3DInProcessCommandBufferImpl::
    ~WebGraphicsContext3DInProcessCommandBufferImpl() {
}

// static
void WebGraphicsContext3DInProcessCommandBufferImpl::ConvertAttributes(
    const WebKit::WebGraphicsContext3D::Attributes& attributes,
    ::gpu::GLInProcessContextAttribs* output_attribs) {
  output_attribs->alpha_size = attributes.alpha ? 8 : 0;
  output_attribs->depth_size = attributes.depth ? 24 : 0;
  output_attribs->stencil_size = attributes.stencil ? 8 : 0;
  output_attribs->samples = attributes.antialias ? 4 : 0;
  output_attribs->sample_buffers = attributes.antialias ? 1 : 0;
}

bool WebGraphicsContext3DInProcessCommandBufferImpl::MaybeInitializeGL() {
  if (initialized_)
    return true;

  if (initialize_failed_)
    return false;

  // Ensure the gles2 library is initialized first in a thread safe way.
  g_gles2_initializer.Get();

  if (!context_) {
    const char* preferred_extensions = "*";

    // TODO(kbr): More work will be needed in this implementation to
    // properly support GPU switching. Like in the out-of-process
    // command buffer implementation, all previously created contexts
    // will need to be lost either when the first context requesting the
    // discrete GPU is created, or the last one is destroyed.
    gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;

    ::gpu::GLInProcessContextAttribs attrib_struct;
    ConvertAttributes(attributes_, &attrib_struct),

    context_.reset(GLInProcessContext::CreateContext(
        is_offscreen_,
        window_,
        gfx::Size(1, 1),
        attributes_.shareResources,
        preferred_extensions,
        attrib_struct,
        gpu_preference));
  }

  if (context_) {
    base::Closure context_lost_callback = base::Bind(
        &WebGraphicsContext3DInProcessCommandBufferImpl::OnContextLost,
        base::Unretained(this));
    context_->SetContextLostCallback(context_lost_callback);
  } else {
    initialize_failed_ = true;
    return false;
  }

  gl_ = context_->GetImplementation();

  if (gl_ && attributes_.noExtensions)
    gl_->EnableFeatureCHROMIUM("webgl_enable_glsl_webgl_validation");

  // Set attributes_ from created offscreen context.
  {
    GLint alpha_bits = 0;
    getIntegerv(GL_ALPHA_BITS, &alpha_bits);
    attributes_.alpha = alpha_bits > 0;
    GLint depth_bits = 0;
    getIntegerv(GL_DEPTH_BITS, &depth_bits);
    attributes_.depth = depth_bits > 0;
    GLint stencil_bits = 0;
    getIntegerv(GL_STENCIL_BITS, &stencil_bits);
    attributes_.stencil = stencil_bits > 0;
    GLint sample_buffers = 0;
    getIntegerv(GL_SAMPLE_BUFFERS, &sample_buffers);
    attributes_.antialias = sample_buffers > 0;
  }

  initialized_ = true;
  return true;
}

bool WebGraphicsContext3DInProcessCommandBufferImpl::makeContextCurrent() {
  if (!MaybeInitializeGL())
    return false;
  ::gles2::SetGLContext(gl_);
  return context_ && !isContextLost();
}

void WebGraphicsContext3DInProcessCommandBufferImpl::ClearContext() {
  // NOTE: Comment in the line below to check for code that is not calling
  // eglMakeCurrent where appropriate. The issue is code using
  // WebGraphicsContext3D does not need to call makeContextCurrent. Code using
  // direct OpenGL bindings needs to call the appropriate form of
  // eglMakeCurrent. If it doesn't it will be issuing commands on the wrong
  // context. Uncommenting the line below clears the current context so that
  // any code not calling eglMakeCurrent in the appropriate place should crash.
  // This is not a perfect test but generally code that used the direct OpenGL
  // bindings should not be mixed with code that uses WebGraphicsContext3D.
  //
  // GLInProcessContext::MakeCurrent(NULL);
}

int WebGraphicsContext3DInProcessCommandBufferImpl::width() {
  return cached_width_;
}

int WebGraphicsContext3DInProcessCommandBufferImpl::height() {
  return cached_height_;
}

void WebGraphicsContext3DInProcessCommandBufferImpl::prepareTexture() {
  if (!isContextLost()) {
    gl_->SwapBuffers();
    gl_->ShallowFlushCHROMIUM();
  }
}

void WebGraphicsContext3DInProcessCommandBufferImpl::postSubBufferCHROMIUM(
    int x, int y, int width, int height) {
  gl_->PostSubBufferCHROMIUM(x, y, width, height);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::reshape(
    int width, int height) {
  reshapeWithScaleFactor(width, height, 1.0f);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::reshapeWithScaleFactor(
    int width, int height, float scale_factor) {
  cached_width_ = width;
  cached_height_ = height;

  // TODO(gmam): See if we can comment this in.
  // ClearContext();

  gl_->ResizeCHROMIUM(width, height, scale_factor);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::synthesizeGLError(
    WGC3Denum error) {
  if (std::find(synthetic_errors_.begin(), synthetic_errors_.end(), error) ==
      synthetic_errors_.end()) {
    synthetic_errors_.push_back(error);
  }
}

void* WebGraphicsContext3DInProcessCommandBufferImpl::mapBufferSubDataCHROMIUM(
    WGC3Denum target,
    WGC3Dintptr offset,
    WGC3Dsizeiptr size,
    WGC3Denum access) {
  ClearContext();
  return gl_->MapBufferSubDataCHROMIUM(target, offset, size, access);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::unmapBufferSubDataCHROMIUM(
    const void* mem) {
  ClearContext();
  return gl_->UnmapBufferSubDataCHROMIUM(mem);
}

void* WebGraphicsContext3DInProcessCommandBufferImpl::mapTexSubImage2DCHROMIUM(
    WGC3Denum target,
    WGC3Dint level,
    WGC3Dint xoffset,
    WGC3Dint yoffset,
    WGC3Dsizei width,
    WGC3Dsizei height,
    WGC3Denum format,
    WGC3Denum type,
    WGC3Denum access) {
  ClearContext();
  return gl_->MapTexSubImage2DCHROMIUM(
      target, level, xoffset, yoffset, width, height, format, type, access);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::unmapTexSubImage2DCHROMIUM(
    const void* mem) {
  ClearContext();
  gl_->UnmapTexSubImage2DCHROMIUM(mem);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::setVisibilityCHROMIUM(
    bool visible) {
}

void WebGraphicsContext3DInProcessCommandBufferImpl::
    setMemoryAllocationChangedCallbackCHROMIUM(
        WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* callback) {
}

void WebGraphicsContext3DInProcessCommandBufferImpl::discardFramebufferEXT(
    WGC3Denum target, WGC3Dsizei numAttachments, const WGC3Denum* attachments) {
  gl_->DiscardFramebufferEXT(target, numAttachments, attachments);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::
    discardBackbufferCHROMIUM() {
}

void WebGraphicsContext3DInProcessCommandBufferImpl::
    ensureBackbufferCHROMIUM() {
}

void WebGraphicsContext3DInProcessCommandBufferImpl::
    copyTextureToParentTextureCHROMIUM(WebGLId texture, WebGLId parentTexture) {
  NOTIMPLEMENTED();
}

void WebGraphicsContext3DInProcessCommandBufferImpl::
    rateLimitOffscreenContextCHROMIUM() {
  // TODO(gmam): See if we can comment this in.
  // ClearContext();
  gl_->RateLimitOffscreenContextCHROMIUM();
}

WebKit::WebString WebGraphicsContext3DInProcessCommandBufferImpl::
    getRequestableExtensionsCHROMIUM() {
  // TODO(gmam): See if we can comment this in.
  // ClearContext();
  return WebKit::WebString::fromUTF8(
      gl_->GetRequestableExtensionsCHROMIUM());
}

void WebGraphicsContext3DInProcessCommandBufferImpl::requestExtensionCHROMIUM(
    const char* extension) {
  // TODO(gmam): See if we can comment this in.
  // ClearContext();
  gl_->RequestExtensionCHROMIUM(extension);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::blitFramebufferCHROMIUM(
    WGC3Dint srcX0, WGC3Dint srcY0, WGC3Dint srcX1, WGC3Dint srcY1,
    WGC3Dint dstX0, WGC3Dint dstY0, WGC3Dint dstX1, WGC3Dint dstY1,
    WGC3Dbitfield mask, WGC3Denum filter) {
  ClearContext();
  gl_->BlitFramebufferEXT(
      srcX0, srcY0, srcX1, srcY1,
      dstX0, dstY0, dstX1, dstY1,
      mask, filter);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::
    renderbufferStorageMultisampleCHROMIUM(
        WGC3Denum target, WGC3Dsizei samples, WGC3Denum internalformat,
        WGC3Dsizei width, WGC3Dsizei height) {
  ClearContext();
  gl_->RenderbufferStorageMultisampleEXT(
      target, samples, internalformat, width, height);
}

// Helper macros to reduce the amount of code.

#define DELEGATE_TO_GL(name, glname)                                    \
void WebGraphicsContext3DInProcessCommandBufferImpl::name() {           \
  ClearContext();                                                       \
  gl_->glname();                                                        \
}

#define DELEGATE_TO_GL_1(name, glname, t1)                              \
void WebGraphicsContext3DInProcessCommandBufferImpl::name(t1 a1) {      \
  ClearContext();                                                       \
  gl_->glname(a1);                                                      \
}

#define DELEGATE_TO_GL_1R(name, glname, t1, rt)                         \
rt WebGraphicsContext3DInProcessCommandBufferImpl::name(t1 a1) {        \
  ClearContext();                                                       \
  return gl_->glname(a1);                                               \
}

#define DELEGATE_TO_GL_1RB(name, glname, t1, rt)                        \
rt WebGraphicsContext3DInProcessCommandBufferImpl::name(t1 a1) {        \
  ClearContext();                                                       \
  return gl_->glname(a1) ? true : false;                                \
}

#define DELEGATE_TO_GL_2(name, glname, t1, t2)                          \
void WebGraphicsContext3DInProcessCommandBufferImpl::name(              \
    t1 a1, t2 a2) {                                                     \
  ClearContext();                                                       \
  gl_->glname(a1, a2);                                                  \
}

#define DELEGATE_TO_GL_2R(name, glname, t1, t2, rt)                     \
rt WebGraphicsContext3DInProcessCommandBufferImpl::name(t1 a1, t2 a2) { \
  ClearContext();                                                       \
  return gl_->glname(a1, a2);                                           \
}

#define DELEGATE_TO_GL_3(name, glname, t1, t2, t3)                      \
void WebGraphicsContext3DInProcessCommandBufferImpl::name(              \
    t1 a1, t2 a2, t3 a3) {                                              \
  ClearContext();                                                       \
  gl_->glname(a1, a2, a3);                                              \
}

#define DELEGATE_TO_GL_3R(name, glname, t1, t2, t3, rt)                 \
rt WebGraphicsContext3DInProcessCommandBufferImpl::name(                \
    t1 a1, t2 a2, t3 a3) {                                              \
  ClearContext();                                                       \
  return gl_->glname(a1, a2, a3);                                       \
}

#define DELEGATE_TO_GL_4(name, glname, t1, t2, t3, t4)                  \
void WebGraphicsContext3DInProcessCommandBufferImpl::name(              \
    t1 a1, t2 a2, t3 a3, t4 a4) {                                       \
  ClearContext();                                                       \
  gl_->glname(a1, a2, a3, a4);                                          \
}

#define DELEGATE_TO_GL_5(name, glname, t1, t2, t3, t4, t5)              \
void WebGraphicsContext3DInProcessCommandBufferImpl::name(              \
    t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) {                                \
  ClearContext();                                                       \
  gl_->glname(a1, a2, a3, a4, a5);                                      \
}

#define DELEGATE_TO_GL_6(name, glname, t1, t2, t3, t4, t5, t6)          \
void WebGraphicsContext3DInProcessCommandBufferImpl::name(              \
    t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) {                         \
  ClearContext();                                                       \
  gl_->glname(a1, a2, a3, a4, a5, a6);                                  \
}

#define DELEGATE_TO_GL_7(name, glname, t1, t2, t3, t4, t5, t6, t7)      \
void WebGraphicsContext3DInProcessCommandBufferImpl::name(              \
    t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) {                  \
  ClearContext();                                                       \
  gl_->glname(a1, a2, a3, a4, a5, a6, a7);                              \
}

#define DELEGATE_TO_GL_8(name, glname, t1, t2, t3, t4, t5, t6, t7, t8)  \
void WebGraphicsContext3DInProcessCommandBufferImpl::name(              \
    t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8) {           \
  ClearContext();                                                       \
  gl_->glname(a1, a2, a3, a4, a5, a6, a7, a8);                          \
}

#define DELEGATE_TO_GL_9(name, glname, t1, t2, t3, t4, t5, t6, t7, t8, t9) \
void WebGraphicsContext3DInProcessCommandBufferImpl::name(              \
    t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9) {    \
  ClearContext();                                                       \
  gl_->glname(a1, a2, a3, a4, a5, a6, a7, a8, a9);                      \
}

DELEGATE_TO_GL_1(activeTexture, ActiveTexture, WGC3Denum)

DELEGATE_TO_GL_2(attachShader, AttachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_3(bindAttribLocation, BindAttribLocation, WebGLId,
                 WGC3Duint, const WGC3Dchar*)

DELEGATE_TO_GL_2(bindBuffer, BindBuffer, WGC3Denum, WebGLId)

void WebGraphicsContext3DInProcessCommandBufferImpl::bindFramebuffer(
    WGC3Denum target,
    WebGLId framebuffer) {
  ClearContext();
  gl_->BindFramebuffer(target, framebuffer);
}

DELEGATE_TO_GL_2(bindRenderbuffer, BindRenderbuffer, WGC3Denum, WebGLId)

DELEGATE_TO_GL_2(bindTexture, BindTexture, WGC3Denum, WebGLId)

DELEGATE_TO_GL_4(blendColor, BlendColor,
                 WGC3Dclampf, WGC3Dclampf, WGC3Dclampf, WGC3Dclampf)

DELEGATE_TO_GL_1(blendEquation, BlendEquation, WGC3Denum)

DELEGATE_TO_GL_2(blendEquationSeparate, BlendEquationSeparate,
                 WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_2(blendFunc, BlendFunc, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_4(blendFuncSeparate, BlendFuncSeparate,
                 WGC3Denum, WGC3Denum, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_4(bufferData, BufferData,
                 WGC3Denum, WGC3Dsizeiptr, const void*, WGC3Denum)

DELEGATE_TO_GL_4(bufferSubData, BufferSubData,
                 WGC3Denum, WGC3Dintptr, WGC3Dsizeiptr, const void*)

DELEGATE_TO_GL_1R(checkFramebufferStatus, CheckFramebufferStatus,
                  WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_1(clear, Clear, WGC3Dbitfield)

DELEGATE_TO_GL_4(clearColor, ClearColor,
                 WGC3Dclampf, WGC3Dclampf, WGC3Dclampf, WGC3Dclampf)

DELEGATE_TO_GL_1(clearDepth, ClearDepthf, WGC3Dclampf)

DELEGATE_TO_GL_1(clearStencil, ClearStencil, WGC3Dint)

DELEGATE_TO_GL_4(colorMask, ColorMask,
                 WGC3Dboolean, WGC3Dboolean, WGC3Dboolean, WGC3Dboolean)

DELEGATE_TO_GL_1(compileShader, CompileShader, WebGLId)

DELEGATE_TO_GL_8(compressedTexImage2D, CompressedTexImage2D,
                 WGC3Denum, WGC3Dint, WGC3Denum, WGC3Dint, WGC3Dint,
                 WGC3Dsizei, WGC3Dsizei, const void*)

DELEGATE_TO_GL_9(compressedTexSubImage2D, CompressedTexSubImage2D,
                 WGC3Denum, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint,
                 WGC3Denum, WGC3Dsizei, const void*)

DELEGATE_TO_GL_8(copyTexImage2D, CopyTexImage2D,
                 WGC3Denum, WGC3Dint, WGC3Denum, WGC3Dint, WGC3Dint,
                 WGC3Dsizei, WGC3Dsizei, WGC3Dint)

DELEGATE_TO_GL_8(copyTexSubImage2D, CopyTexSubImage2D,
                 WGC3Denum, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint,
                 WGC3Dsizei, WGC3Dsizei)

DELEGATE_TO_GL_1(cullFace, CullFace, WGC3Denum)

DELEGATE_TO_GL_1(depthFunc, DepthFunc, WGC3Denum)

DELEGATE_TO_GL_1(depthMask, DepthMask, WGC3Dboolean)

DELEGATE_TO_GL_2(depthRange, DepthRangef, WGC3Dclampf, WGC3Dclampf)

DELEGATE_TO_GL_2(detachShader, DetachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_1(disable, Disable, WGC3Denum)

DELEGATE_TO_GL_1(disableVertexAttribArray, DisableVertexAttribArray,
                 WGC3Duint)

DELEGATE_TO_GL_3(drawArrays, DrawArrays, WGC3Denum, WGC3Dint, WGC3Dsizei)

void WebGraphicsContext3DInProcessCommandBufferImpl::drawElements(
    WGC3Denum mode,
    WGC3Dsizei count,
    WGC3Denum type,
    WGC3Dintptr offset) {
  ClearContext();
  gl_->DrawElements(
      mode, count, type,
      reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_1(enable, Enable, WGC3Denum)

DELEGATE_TO_GL_1(enableVertexAttribArray, EnableVertexAttribArray,
                 WGC3Duint)

DELEGATE_TO_GL(finish, Finish)

DELEGATE_TO_GL(flush, Flush)

DELEGATE_TO_GL_4(framebufferRenderbuffer, FramebufferRenderbuffer,
                 WGC3Denum, WGC3Denum, WGC3Denum, WebGLId)

DELEGATE_TO_GL_5(framebufferTexture2D, FramebufferTexture2D,
                 WGC3Denum, WGC3Denum, WGC3Denum, WebGLId, WGC3Dint)

DELEGATE_TO_GL_1(frontFace, FrontFace, WGC3Denum)

DELEGATE_TO_GL_1(generateMipmap, GenerateMipmap, WGC3Denum)

bool WebGraphicsContext3DInProcessCommandBufferImpl::getActiveAttrib(
    WebGLId program, WGC3Duint index, ActiveInfo& info) {
  ClearContext();
  if (!program) {
    synthesizeGLError(GL_INVALID_VALUE);
    return false;
  }
  GLint max_name_length = -1;
  gl_->GetProgramiv(
      program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_name_length);
  if (max_name_length < 0)
    return false;
  scoped_ptr<GLchar[]> name(new GLchar[max_name_length]);
  if (!name) {
    synthesizeGLError(GL_OUT_OF_MEMORY);
    return false;
  }
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  gl_->GetActiveAttrib(
      program, index, max_name_length, &length, &size, &type, name.get());
  if (size < 0) {
    return false;
  }
  info.name = WebKit::WebString::fromUTF8(name.get(), length);
  info.type = type;
  info.size = size;
  return true;
}

bool WebGraphicsContext3DInProcessCommandBufferImpl::getActiveUniform(
    WebGLId program, WGC3Duint index, ActiveInfo& info) {
  ClearContext();
  GLint max_name_length = -1;
  gl_->GetProgramiv(
      program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_length);
  if (max_name_length < 0)
    return false;
  scoped_ptr<GLchar[]> name(new GLchar[max_name_length]);
  if (!name) {
    synthesizeGLError(GL_OUT_OF_MEMORY);
    return false;
  }
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  gl_->GetActiveUniform(
      program, index, max_name_length, &length, &size, &type, name.get());
  if (size < 0) {
    return false;
  }
  info.name = WebKit::WebString::fromUTF8(name.get(), length);
  info.type = type;
  info.size = size;
  return true;
}

DELEGATE_TO_GL_4(getAttachedShaders, GetAttachedShaders,
                 WebGLId, WGC3Dsizei, WGC3Dsizei*, WebGLId*)

DELEGATE_TO_GL_2R(getAttribLocation, GetAttribLocation,
                  WebGLId, const WGC3Dchar*, WGC3Dint)

DELEGATE_TO_GL_2(getBooleanv, GetBooleanv, WGC3Denum, WGC3Dboolean*)

DELEGATE_TO_GL_3(getBufferParameteriv, GetBufferParameteriv,
                 WGC3Denum, WGC3Denum, WGC3Dint*)

WebKit::WebGraphicsContext3D::Attributes
WebGraphicsContext3DInProcessCommandBufferImpl::getContextAttributes() {
  return attributes_;
}

WGC3Denum WebGraphicsContext3DInProcessCommandBufferImpl::getError() {
  ClearContext();
  if (!synthetic_errors_.empty()) {
    std::vector<WGC3Denum>::iterator iter = synthetic_errors_.begin();
    WGC3Denum err = *iter;
    synthetic_errors_.erase(iter);
    return err;
  }

  return gl_->GetError();
}

bool WebGraphicsContext3DInProcessCommandBufferImpl::isContextLost() {
  return context_lost_reason_ != GL_NO_ERROR;
}

DELEGATE_TO_GL_2(getFloatv, GetFloatv, WGC3Denum, WGC3Dfloat*)

DELEGATE_TO_GL_4(getFramebufferAttachmentParameteriv,
                 GetFramebufferAttachmentParameteriv,
                 WGC3Denum, WGC3Denum, WGC3Denum, WGC3Dint*)

DELEGATE_TO_GL_2(getIntegerv, GetIntegerv, WGC3Denum, WGC3Dint*)

DELEGATE_TO_GL_3(getProgramiv, GetProgramiv, WebGLId, WGC3Denum, WGC3Dint*)

WebKit::WebString WebGraphicsContext3DInProcessCommandBufferImpl::
    getProgramInfoLog(WebGLId program) {
  ClearContext();
  GLint logLength = 0;
  gl_->GetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
  if (!logLength)
    return WebKit::WebString();
  scoped_ptr<GLchar[]> log(new GLchar[logLength]);
  if (!log)
    return WebKit::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetProgramInfoLog(
      program, logLength, &returnedLogLength, log.get());
  DCHECK_EQ(logLength, returnedLogLength + 1);
  WebKit::WebString res =
      WebKit::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

DELEGATE_TO_GL_3(getRenderbufferParameteriv, GetRenderbufferParameteriv,
                 WGC3Denum, WGC3Denum, WGC3Dint*)

DELEGATE_TO_GL_3(getShaderiv, GetShaderiv, WebGLId, WGC3Denum, WGC3Dint*)

WebKit::WebString WebGraphicsContext3DInProcessCommandBufferImpl::
    getShaderInfoLog(WebGLId shader) {
  ClearContext();
  GLint logLength = 0;
  gl_->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
  if (!logLength)
    return WebKit::WebString();
  scoped_ptr<GLchar[]> log(new GLchar[logLength]);
  if (!log)
    return WebKit::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetShaderInfoLog(
      shader, logLength, &returnedLogLength, log.get());
  DCHECK_EQ(logLength, returnedLogLength + 1);
  WebKit::WebString res =
      WebKit::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

DELEGATE_TO_GL_4(getShaderPrecisionFormat, GetShaderPrecisionFormat,
                 WGC3Denum, WGC3Denum, WGC3Dint*, WGC3Dint*)

WebKit::WebString WebGraphicsContext3DInProcessCommandBufferImpl::
    getShaderSource(WebGLId shader) {
  ClearContext();
  GLint logLength = 0;
  gl_->GetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &logLength);
  if (!logLength)
    return WebKit::WebString();
  scoped_ptr<GLchar[]> log(new GLchar[logLength]);
  if (!log)
    return WebKit::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetShaderSource(
      shader, logLength, &returnedLogLength, log.get());
  if (!returnedLogLength)
    return WebKit::WebString();
  DCHECK_EQ(logLength, returnedLogLength + 1);
  WebKit::WebString res =
      WebKit::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

WebKit::WebString WebGraphicsContext3DInProcessCommandBufferImpl::
    getTranslatedShaderSourceANGLE(WebGLId shader) {
  ClearContext();
  GLint logLength = 0;
  gl_->GetShaderiv(
      shader, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE, &logLength);
  if (!logLength)
    return WebKit::WebString();
  scoped_ptr<GLchar[]> log(new GLchar[logLength]);
  if (!log)
    return WebKit::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetTranslatedShaderSourceANGLE(
      shader, logLength, &returnedLogLength, log.get());
  if (!returnedLogLength)
    return WebKit::WebString();
  DCHECK_EQ(logLength, returnedLogLength + 1);
  WebKit::WebString res =
      WebKit::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

WebKit::WebString WebGraphicsContext3DInProcessCommandBufferImpl::getString(
    WGC3Denum name) {
  ClearContext();
  return WebKit::WebString::fromUTF8(
      reinterpret_cast<const char*>(gl_->GetString(name)));
}

DELEGATE_TO_GL_3(getTexParameterfv, GetTexParameterfv,
                 WGC3Denum, WGC3Denum, WGC3Dfloat*)

DELEGATE_TO_GL_3(getTexParameteriv, GetTexParameteriv,
                 WGC3Denum, WGC3Denum, WGC3Dint*)

DELEGATE_TO_GL_3(getUniformfv, GetUniformfv, WebGLId, WGC3Dint, WGC3Dfloat*)

DELEGATE_TO_GL_3(getUniformiv, GetUniformiv, WebGLId, WGC3Dint, WGC3Dint*)

DELEGATE_TO_GL_2R(getUniformLocation, GetUniformLocation,
                  WebGLId, const WGC3Dchar*, WGC3Dint)

DELEGATE_TO_GL_3(getVertexAttribfv, GetVertexAttribfv,
                 WGC3Duint, WGC3Denum, WGC3Dfloat*)

DELEGATE_TO_GL_3(getVertexAttribiv, GetVertexAttribiv,
                 WGC3Duint, WGC3Denum, WGC3Dint*)

WGC3Dsizeiptr WebGraphicsContext3DInProcessCommandBufferImpl::
    getVertexAttribOffset(WGC3Duint index, WGC3Denum pname) {
  ClearContext();
  GLvoid* value = NULL;
  // NOTE: If pname is ever a value that returns more then 1 element
  // this will corrupt memory.
  gl_->GetVertexAttribPointerv(index, pname, &value);
  return static_cast<WGC3Dsizeiptr>(reinterpret_cast<intptr_t>(value));
}

DELEGATE_TO_GL_2(hint, Hint, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_1RB(isBuffer, IsBuffer, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isEnabled, IsEnabled, WGC3Denum, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isFramebuffer, IsFramebuffer, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isProgram, IsProgram, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isRenderbuffer, IsRenderbuffer, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isShader, IsShader, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isTexture, IsTexture, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1(lineWidth, LineWidth, WGC3Dfloat)

DELEGATE_TO_GL_1(linkProgram, LinkProgram, WebGLId)

DELEGATE_TO_GL_2(pixelStorei, PixelStorei, WGC3Denum, WGC3Dint)

DELEGATE_TO_GL_2(polygonOffset, PolygonOffset, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_7(readPixels, ReadPixels,
                 WGC3Dint, WGC3Dint, WGC3Dsizei, WGC3Dsizei, WGC3Denum,
                 WGC3Denum, void*)

void WebGraphicsContext3DInProcessCommandBufferImpl::releaseShaderCompiler() {
  ClearContext();
}

DELEGATE_TO_GL_4(renderbufferStorage, RenderbufferStorage,
                 WGC3Denum, WGC3Denum, WGC3Dsizei, WGC3Dsizei)

DELEGATE_TO_GL_2(sampleCoverage, SampleCoverage, WGC3Dfloat, WGC3Dboolean)

DELEGATE_TO_GL_4(scissor, Scissor, WGC3Dint, WGC3Dint, WGC3Dsizei, WGC3Dsizei)

void WebGraphicsContext3DInProcessCommandBufferImpl::shaderSource(
    WebGLId shader, const WGC3Dchar* string) {
  ClearContext();
  GLint length = strlen(string);
  gl_->ShaderSource(shader, 1, &string, &length);
}

DELEGATE_TO_GL_3(stencilFunc, StencilFunc, WGC3Denum, WGC3Dint, WGC3Duint)

DELEGATE_TO_GL_4(stencilFuncSeparate, StencilFuncSeparate,
                 WGC3Denum, WGC3Denum, WGC3Dint, WGC3Duint)

DELEGATE_TO_GL_1(stencilMask, StencilMask, WGC3Duint)

DELEGATE_TO_GL_2(stencilMaskSeparate, StencilMaskSeparate,
                 WGC3Denum, WGC3Duint)

DELEGATE_TO_GL_3(stencilOp, StencilOp,
                 WGC3Denum, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_4(stencilOpSeparate, StencilOpSeparate,
                 WGC3Denum, WGC3Denum, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_9(texImage2D, TexImage2D,
                 WGC3Denum, WGC3Dint, WGC3Denum, WGC3Dsizei, WGC3Dsizei,
                 WGC3Dint, WGC3Denum, WGC3Denum, const void*)

DELEGATE_TO_GL_3(texParameterf, TexParameterf,
                 WGC3Denum, WGC3Denum, WGC3Dfloat);

static const unsigned int kTextureWrapR = 0x8072;

void WebGraphicsContext3DInProcessCommandBufferImpl::texParameteri(
    WGC3Denum target, WGC3Denum pname, WGC3Dint param) {
  ClearContext();
  // TODO(kbr): figure out whether the setting of TEXTURE_WRAP_R in
  // GraphicsContext3D.cpp is strictly necessary to avoid seams at the
  // edge of cube maps, and, if it is, push it into the GLES2 service
  // side code.
  if (pname == kTextureWrapR) {
    return;
  }
  gl_->TexParameteri(target, pname, param);
}

DELEGATE_TO_GL_9(texSubImage2D, TexSubImage2D,
                 WGC3Denum, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dsizei,
                 WGC3Dsizei, WGC3Denum, WGC3Denum, const void*)

DELEGATE_TO_GL_2(uniform1f, Uniform1f, WGC3Dint, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform1fv, Uniform1fv, WGC3Dint, WGC3Dsizei,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_2(uniform1i, Uniform1i, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform1iv, Uniform1iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_3(uniform2f, Uniform2f, WGC3Dint, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform2fv, Uniform2fv, WGC3Dint, WGC3Dsizei,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_3(uniform2i, Uniform2i, WGC3Dint, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform2iv, Uniform2iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_4(uniform3f, Uniform3f, WGC3Dint,
                 WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform3fv, Uniform3fv, WGC3Dint, WGC3Dsizei,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_4(uniform3i, Uniform3i, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform3iv, Uniform3iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_5(uniform4f, Uniform4f, WGC3Dint,
                 WGC3Dfloat, WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform4fv, Uniform4fv, WGC3Dint, WGC3Dsizei,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_5(uniform4i, Uniform4i, WGC3Dint,
                 WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform4iv, Uniform4iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_4(uniformMatrix2fv, UniformMatrix2fv,
                 WGC3Dint, WGC3Dsizei, WGC3Dboolean, const WGC3Dfloat*)

DELEGATE_TO_GL_4(uniformMatrix3fv, UniformMatrix3fv,
                 WGC3Dint, WGC3Dsizei, WGC3Dboolean, const WGC3Dfloat*)

DELEGATE_TO_GL_4(uniformMatrix4fv, UniformMatrix4fv,
                 WGC3Dint, WGC3Dsizei, WGC3Dboolean, const WGC3Dfloat*)

DELEGATE_TO_GL_1(useProgram, UseProgram, WebGLId)

DELEGATE_TO_GL_1(validateProgram, ValidateProgram, WebGLId)

DELEGATE_TO_GL_2(vertexAttrib1f, VertexAttrib1f, WGC3Duint, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib1fv, VertexAttrib1fv, WGC3Duint,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_3(vertexAttrib2f, VertexAttrib2f, WGC3Duint,
                 WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib2fv, VertexAttrib2fv, WGC3Duint,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_4(vertexAttrib3f, VertexAttrib3f, WGC3Duint,
                 WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib3fv, VertexAttrib3fv, WGC3Duint,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_5(vertexAttrib4f, VertexAttrib4f, WGC3Duint,
                 WGC3Dfloat, WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib4fv, VertexAttrib4fv, WGC3Duint,
                 const WGC3Dfloat*)

void WebGraphicsContext3DInProcessCommandBufferImpl::vertexAttribPointer(
    WGC3Duint index, WGC3Dint size, WGC3Denum type, WGC3Dboolean normalized,
    WGC3Dsizei stride, WGC3Dintptr offset) {
  ClearContext();
  gl_->VertexAttribPointer(
      index, size, type, normalized, stride,
      reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_4(viewport, Viewport,
                 WGC3Dint, WGC3Dint, WGC3Dsizei, WGC3Dsizei)

WebGLId WebGraphicsContext3DInProcessCommandBufferImpl::createBuffer() {
  ClearContext();
  GLuint o;
  gl_->GenBuffers(1, &o);
  return o;
}

WebGLId WebGraphicsContext3DInProcessCommandBufferImpl::createFramebuffer() {
  ClearContext();
  GLuint o = 0;
  gl_->GenFramebuffers(1, &o);
  return o;
}

WebGLId WebGraphicsContext3DInProcessCommandBufferImpl::createProgram() {
  ClearContext();
  return gl_->CreateProgram();
}

WebGLId WebGraphicsContext3DInProcessCommandBufferImpl::createRenderbuffer() {
  ClearContext();
  GLuint o;
  gl_->GenRenderbuffers(1, &o);
  return o;
}

DELEGATE_TO_GL_1R(createShader, CreateShader, WGC3Denum, WebGLId);

WebGLId WebGraphicsContext3DInProcessCommandBufferImpl::createTexture() {
  ClearContext();
  GLuint o;
  gl_->GenTextures(1, &o);
  return o;
}

void WebGraphicsContext3DInProcessCommandBufferImpl::deleteBuffer(
    WebGLId buffer) {
  ClearContext();
  gl_->DeleteBuffers(1, &buffer);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::deleteFramebuffer(
    WebGLId framebuffer) {
  ClearContext();
  gl_->DeleteFramebuffers(1, &framebuffer);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::deleteProgram(
    WebGLId program) {
  ClearContext();
  gl_->DeleteProgram(program);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::deleteRenderbuffer(
    WebGLId renderbuffer) {
  ClearContext();
  gl_->DeleteRenderbuffers(1, &renderbuffer);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::deleteShader(
    WebGLId shader) {
  ClearContext();
  gl_->DeleteShader(shader);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::deleteTexture(
    WebGLId texture) {
  ClearContext();
  gl_->DeleteTextures(1, &texture);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::OnSwapBuffersComplete() {
}

void WebGraphicsContext3DInProcessCommandBufferImpl::setContextLostCallback(
    WebGraphicsContext3D::WebGraphicsContextLostCallback* cb) {
  context_lost_callback_ = cb;
}

WGC3Denum WebGraphicsContext3DInProcessCommandBufferImpl::
    getGraphicsResetStatusARB() {
  return context_lost_reason_;
}

DELEGATE_TO_GL_5(texImageIOSurface2DCHROMIUM, TexImageIOSurface2DCHROMIUM,
                 WGC3Denum, WGC3Dint, WGC3Dint, WGC3Duint, WGC3Duint)

DELEGATE_TO_GL_5(texStorage2DEXT, TexStorage2DEXT,
                 WGC3Denum, WGC3Dint, WGC3Duint, WGC3Dint, WGC3Dint)

WebGLId WebGraphicsContext3DInProcessCommandBufferImpl::createQueryEXT() {
  GLuint o;
  gl_->GenQueriesEXT(1, &o);
  return o;
}

void WebGraphicsContext3DInProcessCommandBufferImpl::
    deleteQueryEXT(WebGLId query) {
  gl_->DeleteQueriesEXT(1, &query);
}

DELEGATE_TO_GL_1R(isQueryEXT, IsQueryEXT, WebGLId, WGC3Dboolean)
DELEGATE_TO_GL_2(beginQueryEXT, BeginQueryEXT, WGC3Denum, WebGLId)
DELEGATE_TO_GL_1(endQueryEXT, EndQueryEXT, WGC3Denum)
DELEGATE_TO_GL_3(getQueryivEXT, GetQueryivEXT, WGC3Denum, WGC3Denum, WGC3Dint*)
DELEGATE_TO_GL_3(getQueryObjectuivEXT, GetQueryObjectuivEXT,
                 WebGLId, WGC3Denum, WGC3Duint*)

DELEGATE_TO_GL_6(copyTextureCHROMIUM, CopyTextureCHROMIUM, WGC3Denum, WGC3Duint,
                 WGC3Duint, WGC3Dint, WGC3Denum, WGC3Denum)

void WebGraphicsContext3DInProcessCommandBufferImpl::insertEventMarkerEXT(
    const WGC3Dchar* marker) {
  gl_->InsertEventMarkerEXT(0, marker);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::pushGroupMarkerEXT(
    const WGC3Dchar* marker) {
  gl_->PushGroupMarkerEXT(0, marker);
}

DELEGATE_TO_GL(popGroupMarkerEXT, PopGroupMarkerEXT);

DELEGATE_TO_GL_2(bindTexImage2DCHROMIUM, BindTexImage2DCHROMIUM,
                 WGC3Denum, WGC3Dint)
DELEGATE_TO_GL_2(releaseTexImage2DCHROMIUM, ReleaseTexImage2DCHROMIUM,
                 WGC3Denum, WGC3Dint)

DELEGATE_TO_GL_1R(createStreamTextureCHROMIUM, CreateStreamTextureCHROMIUM,
                  WebGLId, WebGLId)
DELEGATE_TO_GL_1(destroyStreamTextureCHROMIUM, DestroyStreamTextureCHROMIUM,
                 WebGLId)

void* WebGraphicsContext3DInProcessCommandBufferImpl::mapBufferCHROMIUM(
    WGC3Denum target, WGC3Denum access) {
  ClearContext();
  return gl_->MapBufferCHROMIUM(target, access);
}

WGC3Dboolean WebGraphicsContext3DInProcessCommandBufferImpl::
    unmapBufferCHROMIUM(WGC3Denum target) {
  ClearContext();
  return gl_->UnmapBufferCHROMIUM(target);
}

GrGLInterface* WebGraphicsContext3DInProcessCommandBufferImpl::
    onCreateGrGLInterface() {
  return CreateCommandBufferSkiaGLBinding();
}

void WebGraphicsContext3DInProcessCommandBufferImpl::OnContextLost() {
  // TODO(kbr): improve the precision here.
  context_lost_reason_ = GL_UNKNOWN_CONTEXT_RESET_ARB;
  if (context_lost_callback_) {
    context_lost_callback_->onContextLost();
  }
}

DELEGATE_TO_GL_3R(createImageCHROMIUM, CreateImageCHROMIUM,
                  WGC3Dsizei, WGC3Dsizei, WGC3Denum, WGC3Duint);

DELEGATE_TO_GL_1(destroyImageCHROMIUM, DestroyImageCHROMIUM, WGC3Duint);

DELEGATE_TO_GL_3(getImageParameterivCHROMIUM, GetImageParameterivCHROMIUM,
                 WGC3Duint, WGC3Denum, GLint*);

DELEGATE_TO_GL_2R(mapImageCHROMIUM, MapImageCHROMIUM,
                  WGC3Duint, WGC3Denum, void*);

DELEGATE_TO_GL_1(unmapImageCHROMIUM, UnmapImageCHROMIUM, WGC3Duint);

DELEGATE_TO_GL_3(bindUniformLocationCHROMIUM, BindUniformLocationCHROMIUM,
                 WebGLId, WGC3Dint, const WGC3Dchar*)

DELEGATE_TO_GL(shallowFlushCHROMIUM, ShallowFlushCHROMIUM)
DELEGATE_TO_GL(shallowFinishCHROMIUM, ShallowFinishCHROMIUM)

DELEGATE_TO_GL_1(genMailboxCHROMIUM, GenMailboxCHROMIUM, WGC3Dbyte*)
DELEGATE_TO_GL_2(produceTextureCHROMIUM, ProduceTextureCHROMIUM,
                 WGC3Denum, const WGC3Dbyte*)
DELEGATE_TO_GL_2(consumeTextureCHROMIUM, ConsumeTextureCHROMIUM,
                 WGC3Denum, const WGC3Dbyte*)

DELEGATE_TO_GL_2(drawBuffersEXT, DrawBuffersEXT,
                 WGC3Dsizei, const WGC3Denum*)

unsigned WebGraphicsContext3DInProcessCommandBufferImpl::insertSyncPoint() {
  shallowFlushCHROMIUM();
  return 0;
}

void WebGraphicsContext3DInProcessCommandBufferImpl::signalSyncPoint(
    unsigned sync_point,
    WebGraphicsSyncPointCallback* callback) {
  // Take ownership of the callback.
  context_->SignalSyncPoint(
      sync_point, base::Bind(&OnSignalSyncPoint, base::Owned(callback)));
}

void WebGraphicsContext3DInProcessCommandBufferImpl::signalQuery(
    unsigned query,
    WebGraphicsSyncPointCallback* callback) {
  // Take ownership of the callback.
  context_->SignalQuery(query,
                        base::Bind(&OnSignalSyncPoint, base::Owned(callback)));
}

void WebGraphicsContext3DInProcessCommandBufferImpl::loseContextCHROMIUM(
    WGC3Denum current, WGC3Denum other) {
  gl_->LoseContextCHROMIUM(current, other);
  gl_->ShallowFlushCHROMIUM();
}

DELEGATE_TO_GL_9(asyncTexImage2DCHROMIUM, AsyncTexImage2DCHROMIUM,
    WGC3Denum, WGC3Dint, WGC3Denum, WGC3Dsizei, WGC3Dsizei, WGC3Dint,
    WGC3Denum, WGC3Denum, const void*)

DELEGATE_TO_GL_9(asyncTexSubImage2DCHROMIUM, AsyncTexSubImage2DCHROMIUM,
    WGC3Denum, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dsizei, WGC3Dsizei,
    WGC3Denum, WGC3Denum, const void*)

DELEGATE_TO_GL_1(waitAsyncTexImage2DCHROMIUM, WaitAsyncTexImage2DCHROMIUM,
    WGC3Denum)

}  // namespace gpu
}  // namespace webkit
