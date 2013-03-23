// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

#include <GLES2/gl2.h>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <algorithm>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "webkit/gpu/gl_bindings_skia_cmd_buffer.h"

using gpu::Buffer;
using gpu::CommandBuffer;
using gpu::CommandBufferService;
using gpu::gles2::GLES2CmdHelper;
using gpu::gles2::GLES2Implementation;
using gpu::GpuScheduler;
using gpu::TransferBuffer;
using gpu::TransferBufferManager;
using gpu::TransferBufferManagerInterface;

namespace webkit {
namespace gpu {

class GLInProcessContext : public base::SupportsWeakPtr<GLInProcessContext> {
 public:
  // These are the same error codes as used by EGL.
  enum Error {
    SUCCESS             = 0x3000,
    NOT_INITIALIZED     = 0x3001,
    BAD_ATTRIBUTE       = 0x3004,
    BAD_GLContext       = 0x3006,
    CONTEXT_LOST        = 0x300E
  };

  // GLInProcessContext configuration attributes. These are the same as used by
  // EGL. Attributes are matched using a closest fit algorithm.
  enum Attribute {
    ALPHA_SIZE     = 0x3021,
    BLUE_SIZE      = 0x3022,
    GREEN_SIZE     = 0x3023,
    RED_SIZE       = 0x3024,
    DEPTH_SIZE     = 0x3025,
    STENCIL_SIZE   = 0x3026,
    SAMPLES        = 0x3031,
    SAMPLE_BUFFERS = 0x3032,
    NONE           = 0x3038  // Attrib list = terminator
  };

  // Initialize the library. This must have completed before any other
  // functions are invoked.
  static bool Initialize();

  // Terminate the library. This must be called after any other functions
  // have completed.
  static bool Terminate();

  ~GLInProcessContext();

  void PumpCommands();
  bool GetBufferChanged(int32 transfer_buffer_id);

  // Create a GLInProcessContext that renders to an offscreen frame buffer. If
  // parent is not NULL, that GLInProcessContext can access a copy of the
  // created GLInProcessContext's frame buffer that is updated every time
  // SwapBuffers is called. It is not as general as shared GLInProcessContexts
  // in other implementations of OpenGL. If parent is not NULL, it must be used
  // on the same thread as the parent. A child GLInProcessContext may not
  // outlive its parent.  attrib_list must be NULL or a NONE-terminated list of
  // attribute/value pairs.
  static GLInProcessContext* CreateOffscreenContext(
      GLInProcessContext* parent,
      const gfx::Size& size,
      GLInProcessContext* context_group,
      const char* allowed_extensions,
      const int32* attrib_list,
      gfx::GpuPreference gpu_preference);

  // For an offscreen frame buffer GLInProcessContext, return the texture ID
  // with respect to the parent GLInProcessContext. Returns zero if
  // GLInProcessContext does not have a parent.
  uint32 GetParentTextureId();

  // Create a new texture in the parent's GLInProcessContext.  Returns zero if
  // GLInProcessContext does not have a parent.
  uint32 CreateParentTexture(const gfx::Size& size);

  // Deletes a texture in the parent's GLInProcessContext.
  void DeleteParentTexture(uint32 texture);

  void SetContextLostCallback(const base::Closure& callback);

  // Set the current GLInProcessContext for the calling thread.
  static bool MakeCurrent(GLInProcessContext* context);

  // For a view GLInProcessContext, display everything that has been rendered
  // since the last call. For an offscreen GLInProcessContext, resolve
  // everything that has been rendered since the last call to a copy that can be
  // accessed by the parent GLInProcessContext.
  bool SwapBuffers();

  // TODO(gman): Remove this
  void DisableShaderTranslation();

  // Allows direct access to the GLES2 implementation so a GLInProcessContext
  // can be used without making it current.
  GLES2Implementation* GetImplementation();

  // Return the current error.
  Error GetError();

  // Return true if GPU process reported GLInProcessContext lost or there was a
  // problem communicating with the GPU process.
  bool IsCommandBufferContextLost();

  CommandBufferService* GetCommandBufferService();

 private:
  explicit GLInProcessContext(GLInProcessContext* parent);

  bool Initialize(const gfx::Size& size,
                  GLInProcessContext* context_group,
                  const char* allowed_extensions,
                  const int32* attrib_list,
                  gfx::GpuPreference gpu_preference);
  void Destroy();

  void OnContextLost();

  base::WeakPtr<GLInProcessContext> parent_;
  base::Closure context_lost_callback_;
  uint32 parent_texture_id_;
  scoped_ptr<TransferBufferManagerInterface> transfer_buffer_manager_;
  scoped_ptr<CommandBufferService> command_buffer_;
  scoped_ptr< ::gpu::GpuScheduler> gpu_scheduler_;
  scoped_ptr< ::gpu::gles2::GLES2Decoder> decoder_;
  scoped_refptr<gfx::GLContext> context_;
  scoped_refptr<gfx::GLSurface> surface_;
  scoped_ptr<GLES2CmdHelper> gles2_helper_;
  scoped_ptr<TransferBuffer> transfer_buffer_;
  scoped_ptr<GLES2Implementation> gles2_implementation_;
  Error last_error_;
  bool context_lost_;

  DISALLOW_COPY_AND_ASSIGN(GLInProcessContext);
};

namespace {

const int32 kCommandBufferSize = 1024 * 1024;
// TODO(kbr): make the transfer buffer size configurable via context
// creation attributes.
const size_t kStartTransferBufferSize = 4 * 1024 * 1024;
const size_t kMinTransferBufferSize = 1 * 256 * 1024;
const size_t kMaxTransferBufferSize = 16 * 1024 * 1024;

static base::LazyInstance<
    std::set<WebGraphicsContext3DInProcessCommandBufferImpl*> >
        g_all_shared_contexts = LAZY_INSTANCE_INITIALIZER;
static base::LazyInstance<base::Lock>::Leaky
    g_all_shared_contexts_lock = LAZY_INSTANCE_INITIALIZER;

// Singleton used to initialize and terminate the gles2 library.
class GLES2Initializer {
 public:
  GLES2Initializer() {
    gles2::Initialize();
  }

  ~GLES2Initializer() {
    gles2::Terminate();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GLES2Initializer);
};

////////////////////////////////////////////////////////////////////////////////

static base::LazyInstance<GLES2Initializer> g_gles2_initializer =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace anonymous

GLInProcessContext::~GLInProcessContext() {
  Destroy();
}

GLInProcessContext* GLInProcessContext::CreateOffscreenContext(
    GLInProcessContext* parent,
    const gfx::Size& size,
    GLInProcessContext* context_group,
    const char* allowed_extensions,
    const int32* attrib_list,
    gfx::GpuPreference gpu_preference) {
  scoped_ptr<GLInProcessContext> context(new GLInProcessContext(parent));
  if (!context->Initialize(
      size,
      context_group,
      allowed_extensions,
      attrib_list,
      gpu_preference))
    return NULL;

  return context.release();
}

// In the normal command buffer implementation, all commands are passed over IPC
// to the gpu process where they are fed to the GLES2Decoder from a single
// thread. In layout tests, any thread could call this function. GLES2Decoder,
// and in particular the GL implementations behind it, are not generally
// threadsafe, so we guard entry points with a mutex.
static base::LazyInstance<base::Lock> g_decoder_lock =
    LAZY_INSTANCE_INITIALIZER;

void GLInProcessContext::PumpCommands() {
  if (!context_lost_) {
    base::AutoLock lock(g_decoder_lock.Get());
    decoder_->MakeCurrent();
    gpu_scheduler_->PutChanged();
    ::gpu::CommandBuffer::State state = command_buffer_->GetState();
    if (::gpu::error::IsError(state.error)) {
      context_lost_ = true;
    }
  }
}

bool GLInProcessContext::GetBufferChanged(int32 transfer_buffer_id) {
  return gpu_scheduler_->SetGetBuffer(transfer_buffer_id);
}

uint32 GLInProcessContext::GetParentTextureId() {
  return parent_texture_id_;
}

uint32 GLInProcessContext::CreateParentTexture(const gfx::Size& size) {
  uint32 texture = 0;
  gles2_implementation_->GenTextures(1, &texture);
  gles2_implementation_->Flush();
  return texture;
}

void GLInProcessContext::DeleteParentTexture(uint32 texture) {
  gles2_implementation_->DeleteTextures(1, &texture);
}

void GLInProcessContext::SetContextLostCallback(const base::Closure& callback) {
  context_lost_callback_ = callback;
}

bool GLInProcessContext::MakeCurrent(GLInProcessContext* context) {
  if (context) {
    gles2::SetGLContext(context->gles2_implementation_.get());

    // Don't request latest error status from service. Just use the locally
    // cached information from the last flush.
    // TODO(apatrick): I'm not sure if this should actually change the
    // current context if it fails. For now it gets changed even if it fails
    // because making GL calls with a NULL context crashes.
    if (context->command_buffer_->GetState().error != ::gpu::error::kNoError)
      return false;
  } else {
    gles2::SetGLContext(NULL);
  }

  return true;
}

bool GLInProcessContext::SwapBuffers() {
  // Don't request latest error status from service. Just use the locally cached
  // information from the last flush.
  if (command_buffer_->GetState().error != ::gpu::error::kNoError)
    return false;

  gles2_implementation_->SwapBuffers();
  gles2_implementation_->Finish();
  return true;
}

GLInProcessContext::Error GLInProcessContext::GetError() {
  CommandBuffer::State state = command_buffer_->GetState();
  if (state.error == ::gpu::error::kNoError) {
    // TODO(gman): Figure out and document what this logic is for.
    Error old_error = last_error_;
    last_error_ = SUCCESS;
    return old_error;
  } else {
    // All command buffer errors are unrecoverable. The error is treated as a
    // lost context: destroy the context and create another one.
    return CONTEXT_LOST;
  }
}

bool GLInProcessContext::IsCommandBufferContextLost() {
  if (context_lost_ || !command_buffer_.get()) {
    return true;
  }
  CommandBuffer::State state = command_buffer_->GetState();
  return ::gpu::error::IsError(state.error);
}

CommandBufferService* GLInProcessContext::GetCommandBufferService() {
  return command_buffer_.get();
}

// TODO(gman): Remove This
void GLInProcessContext::DisableShaderTranslation() {
  NOTREACHED();
}

GLES2Implementation* GLInProcessContext::GetImplementation() {
  return gles2_implementation_.get();
}

GLInProcessContext::GLInProcessContext(GLInProcessContext* parent)
    : parent_(parent ?
          parent->AsWeakPtr() : base::WeakPtr<GLInProcessContext>()),
      parent_texture_id_(0),
      last_error_(SUCCESS),
      context_lost_(false) {
}

bool GLInProcessContext::Initialize(const gfx::Size& size,
                                    GLInProcessContext* context_group,
                                    const char* allowed_extensions,
                                    const int32* attrib_list,
                                    gfx::GpuPreference gpu_preference) {
  // Use one share group for all contexts.
  CR_DEFINE_STATIC_LOCAL(scoped_refptr<gfx::GLShareGroup>, share_group,
                         (new gfx::GLShareGroup));

  DCHECK(size.width() >= 0 && size.height() >= 0);

  // Ensure the gles2 library is initialized first in a thread safe way.
  g_gles2_initializer.Get();

  // Allocate a frame buffer ID with respect to the parent.
  if (parent_.get()) {
    // Flush any remaining commands in the parent context to make sure the
    // texture id accounting stays consistent.
    int32 token = parent_->gles2_helper_->InsertToken();
    parent_->gles2_helper_->WaitForToken(token);
    parent_texture_id_ = parent_->gles2_implementation_->MakeTextureId();
  }

  std::vector<int32> attribs;
  while (attrib_list) {
    int32 attrib = *attrib_list++;
    switch (attrib) {
      // Known attributes
      case ALPHA_SIZE:
      case BLUE_SIZE:
      case GREEN_SIZE:
      case RED_SIZE:
      case DEPTH_SIZE:
      case STENCIL_SIZE:
      case SAMPLES:
      case SAMPLE_BUFFERS:
        attribs.push_back(attrib);
        attribs.push_back(*attrib_list++);
        break;
      case NONE:
        attribs.push_back(attrib);
        attrib_list = NULL;
        break;
      default:
        last_error_ = BAD_ATTRIBUTE;
        attribs.push_back(NONE);
        attrib_list = NULL;
        break;
    }
  }

  {
    TransferBufferManager* manager = new TransferBufferManager();
    transfer_buffer_manager_.reset(manager);
    manager->Initialize();
  }

  command_buffer_.reset(
      new CommandBufferService(transfer_buffer_manager_.get()));
  if (!command_buffer_->Initialize()) {
    LOG(ERROR) << "Could not initialize command buffer.";
    Destroy();
    return false;
  }

  // TODO(gman): This needs to be true if this is Pepper.
  bool bind_generates_resource = false;
  decoder_.reset(::gpu::gles2::GLES2Decoder::Create(context_group ?
      context_group->decoder_->GetContextGroup() :
          new ::gpu::gles2::ContextGroup(
              NULL, NULL, NULL, bind_generates_resource)));

  gpu_scheduler_.reset(new GpuScheduler(command_buffer_.get(),
                                        decoder_.get(),
                                        decoder_.get()));

  decoder_->set_engine(gpu_scheduler_.get());

  surface_ = gfx::GLSurface::CreateOffscreenGLSurface(false, gfx::Size(1, 1));

  if (!surface_.get()) {
    LOG(ERROR) << "Could not create GLSurface.";
    Destroy();
    return false;
  }

  context_ = gfx::GLContext::CreateGLContext(share_group.get(),
                                             surface_.get(),
                                             gpu_preference);
  if (!context_.get()) {
    LOG(ERROR) << "Could not create GLContext.";
    Destroy();
    return false;
  }

  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "Could not make context current.";
    Destroy();
    return false;
  }

  ::gpu::gles2::DisallowedFeatures disallowed_features;
  disallowed_features.swap_buffer_complete_callback = true;
  disallowed_features.gpu_memory_manager = true;
  if (!decoder_->Initialize(surface_,
                            context_,
                            true,
                            size,
                            disallowed_features,
                            allowed_extensions,
                            attribs)) {
    LOG(ERROR) << "Could not initialize decoder.";
    Destroy();
    return false;
  }

  if (!decoder_->SetParent(
      parent_.get() ? parent_->decoder_.get() : NULL,
      parent_texture_id_)) {
    LOG(ERROR) << "Could not set parent.";
    Destroy();
    return false;
  }

  command_buffer_->SetPutOffsetChangeCallback(
      base::Bind(&GLInProcessContext::PumpCommands, base::Unretained(this)));
  command_buffer_->SetGetBufferChangeCallback(
      base::Bind(
          &GLInProcessContext::GetBufferChanged, base::Unretained(this)));
  command_buffer_->SetParseErrorCallback(
      base::Bind(&GLInProcessContext::OnContextLost, base::Unretained(this)));

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_.reset(new GLES2CmdHelper(command_buffer_.get()));
  if (!gles2_helper_->Initialize(kCommandBufferSize)) {
    Destroy();
    return false;
  }

  // Create a transfer buffer.
  transfer_buffer_.reset(new TransferBuffer(gles2_helper_.get()));

  // Create the object exposing the OpenGL API.
  gles2_implementation_.reset(new GLES2Implementation(
      gles2_helper_.get(),
      context_group ? context_group->GetImplementation()->share_group() : NULL,
      transfer_buffer_.get(),
      true,
      false));

  if (!gles2_implementation_->Initialize(
      kStartTransferBufferSize,
      kMinTransferBufferSize,
      kMaxTransferBufferSize)) {
    return false;
  }

  return true;
}

void GLInProcessContext::Destroy() {
  bool context_lost = IsCommandBufferContextLost();

  if (parent_.get() && parent_texture_id_ != 0) {
    parent_->gles2_implementation_->FreeTextureId(parent_texture_id_);
    parent_texture_id_ = 0;
  }

  if (gles2_implementation_.get()) {
    // First flush the context to ensure that any pending frees of resources
    // are completed. Otherwise, if this context is part of a share group,
    // those resources might leak. Also, any remaining side effects of commands
    // issued on this context might not be visible to other contexts in the
    // share group.
    gles2_implementation_->Flush();

    gles2_implementation_.reset();
  }

  transfer_buffer_.reset();
  gles2_helper_.reset();
  command_buffer_.reset();

  if (decoder_.get()) {
    decoder_->Destroy(!context_lost);
  }
}

void GLInProcessContext::OnContextLost() {
  if (!context_lost_callback_.is_null())
    context_lost_callback_.Run();
}

WebGraphicsContext3DInProcessCommandBufferImpl::
    WebGraphicsContext3DInProcessCommandBufferImpl()
    : context_(NULL),
      gl_(NULL),
      context_lost_callback_(NULL),
      context_lost_reason_(GL_NO_ERROR),
      cached_width_(0),
      cached_height_(0),
      bound_fbo_(0) {
}

WebGraphicsContext3DInProcessCommandBufferImpl::
    ~WebGraphicsContext3DInProcessCommandBufferImpl() {
  base::AutoLock a(g_all_shared_contexts_lock.Get());
  g_all_shared_contexts.Pointer()->erase(this);
}

bool WebGraphicsContext3DInProcessCommandBufferImpl::Initialize(
    WebGraphicsContext3D::Attributes attributes,
    WebKit::WebGraphicsContext3D* view_context) {
  // Convert WebGL context creation attributes into GLInProcessContext / EGL
  // size requests.
  const int alpha_size = attributes.alpha ? 8 : 0;
  const int depth_size = attributes.depth ? 24 : 0;
  const int stencil_size = attributes.stencil ? 8 : 0;
  const int samples = attributes.antialias ? 4 : 0;
  const int sample_buffers = attributes.antialias ? 1 : 0;
  const int32 attribs[] = {
    GLInProcessContext::ALPHA_SIZE, alpha_size,
    GLInProcessContext::DEPTH_SIZE, depth_size,
    GLInProcessContext::STENCIL_SIZE, stencil_size,
    GLInProcessContext::SAMPLES, samples,
    GLInProcessContext::SAMPLE_BUFFERS, sample_buffers,
    GLInProcessContext::NONE,
  };

  const char* preferred_extensions = "*";

  // TODO(kbr): More work will be needed in this implementation to
  // properly support GPU switching. Like in the out-of-process
  // command buffer implementation, all previously created contexts
  // will need to be lost either when the first context requesting the
  // discrete GPU is created, or the last one is destroyed.
  gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;

  GLInProcessContext* parent_context = NULL;
  if (view_context) {
    WebGraphicsContext3DInProcessCommandBufferImpl* context_impl =
        static_cast<WebGraphicsContext3DInProcessCommandBufferImpl*>(
            view_context);
    parent_context = context_impl->context_;
  }

  WebGraphicsContext3DInProcessCommandBufferImpl* context_group = NULL;
  base::AutoLock lock(g_all_shared_contexts_lock.Get());
  if (attributes.shareResources)
    context_group = g_all_shared_contexts.Pointer()->empty() ?
        NULL : *g_all_shared_contexts.Pointer()->begin();

  context_ = GLInProcessContext::CreateOffscreenContext(
      parent_context,
      gfx::Size(1, 1),
      context_group ? context_group->context_ : NULL,
      preferred_extensions,
      attribs,
      gpu_preference);

  if (!context_)
    return false;

  gl_ = context_->GetImplementation();

  if (gl_ && attributes.noExtensions)
    gl_->EnableFeatureCHROMIUM("webgl_enable_glsl_webgl_validation");

  context_->SetContextLostCallback(
      base::Bind(
          &WebGraphicsContext3DInProcessCommandBufferImpl::OnContextLost,
          base::Unretained(this)));

  // Set attributes_ from created offscreen context.
  {
    attributes_ = attributes;
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
  makeContextCurrent();

  if (attributes.shareResources)
    g_all_shared_contexts.Pointer()->insert(this);

  return true;
}

bool WebGraphicsContext3DInProcessCommandBufferImpl::makeContextCurrent() {
  return GLInProcessContext::MakeCurrent(context_);
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

bool WebGraphicsContext3DInProcessCommandBufferImpl::isGLES2Compliant() {
  return true;
}

bool WebGraphicsContext3DInProcessCommandBufferImpl::setParentContext(
    WebGraphicsContext3D* parent_context) {
  return false;
}

WebGLId WebGraphicsContext3DInProcessCommandBufferImpl::getPlatformTextureId() {
  DCHECK(context_);
  return context_->GetParentTextureId();
}

void WebGraphicsContext3DInProcessCommandBufferImpl::prepareTexture() {
  // Copies the contents of the off-screen render target into the texture
  // used by the compositor.
  context_->SwapBuffers();
}

void WebGraphicsContext3DInProcessCommandBufferImpl::postSubBufferCHROMIUM(
    int x, int y, int width, int height) {
  gl_->PostSubBufferCHROMIUM(x, y, width, height);
}

void WebGraphicsContext3DInProcessCommandBufferImpl::reshape(
    int width, int height) {
  cached_width_ = width;
  cached_height_ = height;

  // TODO(gmam): See if we can comment this in.
  // ClearContext();

  gl_->ResizeCHROMIUM(width, height);
}

WebGLId WebGraphicsContext3DInProcessCommandBufferImpl::createCompositorTexture(
    WGC3Dsizei width, WGC3Dsizei height) {
  // TODO(gmam): See if we can comment this in.
  // ClearContext();
  return context_->CreateParentTexture(gfx::Size(width, height));
}

void WebGraphicsContext3DInProcessCommandBufferImpl::deleteCompositorTexture(
    WebGLId parent_texture) {
  // TODO(gmam): See if we can comment this in.
  // ClearContext();
  context_->DeleteParentTexture(parent_texture);
}

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
void WebGraphicsContext3DInProcessCommandBufferImpl::FlipVertically(
    uint8* framebuffer,
    unsigned int width,
    unsigned int height) {
  if (width == 0)
    return;
  scanline_.resize(width * 4);
  uint8* scanline = &scanline_[0];
  unsigned int row_bytes = width * 4;
  unsigned int count = height / 2;
  for (unsigned int i = 0; i < count; i++) {
    uint8* row_a = framebuffer + i * row_bytes;
    uint8* row_b = framebuffer + (height - i - 1) * row_bytes;
    // TODO(kbr): this is where the multiplication of the alpha
    // channel into the color buffer will need to occur if the
    // user specifies the "premultiplyAlpha" flag in the context
    // creation attributes.
    memcpy(scanline, row_b, row_bytes);
    memcpy(row_b, row_a, row_bytes);
    memcpy(row_a, scanline, row_bytes);
  }
}
#endif

bool WebGraphicsContext3DInProcessCommandBufferImpl::readBackFramebuffer(
    unsigned char* pixels,
    size_t buffer_size,
    WebGLId framebuffer,
    int width,
    int height) {
  // TODO(gmam): See if we can comment this in.
  // ClearContext();
  if (buffer_size != static_cast<size_t>(4 * width * height)) {
    return false;
  }

  // Earlier versions of this code used the GPU to flip the
  // framebuffer vertically before reading it back for compositing
  // via software. This code was quite complicated, used a lot of
  // GPU memory, and didn't provide an obvious speedup. Since this
  // vertical flip is only a temporary solution anyway until Chrome
  // is fully GPU composited, it wasn't worth the complexity.

  bool mustRestoreFBO = (bound_fbo_ != framebuffer);
  if (mustRestoreFBO) {
    gl_->BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  }
  gl_->ReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  // Swizzle red and blue channels
  // TODO(kbr): expose GL_BGRA as extension
  for (size_t i = 0; i < buffer_size; i += 4) {
    std::swap(pixels[i], pixels[i + 2]);
  }

  if (mustRestoreFBO) {
    gl_->BindFramebuffer(GL_FRAMEBUFFER, bound_fbo_);
  }

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
  if (pixels) {
    FlipVertically(pixels, width, height);
  }
#endif

  return true;
}

bool WebGraphicsContext3DInProcessCommandBufferImpl::readBackFramebuffer(
    unsigned char* pixels,
    size_t buffer_size) {
  return readBackFramebuffer(pixels, buffer_size, 0, width(), height());
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
  bound_fbo_ = framebuffer;
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
  scoped_array<GLchar> name(new GLchar[max_name_length]);
  if (!name.get()) {
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
  scoped_array<GLchar> name(new GLchar[max_name_length]);
  if (!name.get()) {
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
  return context_->IsCommandBufferContextLost();
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
  scoped_array<GLchar> log(new GLchar[logLength]);
  if (!log.get())
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
  scoped_array<GLchar> log(new GLchar[logLength]);
  if (!log.get())
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
  scoped_array<GLchar> log(new GLchar[logLength]);
  if (!log.get())
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
  scoped_array<GLchar> log(new GLchar[logLength]);
  if (!log.get())
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

void WebGraphicsContext3DInProcessCommandBufferImpl::copyTextureToCompositor(
    WebGLId texture, WebGLId parentTexture) {
  NOTIMPLEMENTED();
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

DELEGATE_TO_GL_5(copyTextureCHROMIUM, CopyTextureCHROMIUM, WGC3Denum, WGC3Duint,
                 WGC3Duint, WGC3Dint, WGC3Denum)

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

DELEGATE_TO_GL_3(bindUniformLocationCHROMIUM, BindUniformLocationCHROMIUM,
                 WebGLId, WGC3Dint, const WGC3Dchar*)

DELEGATE_TO_GL(shallowFlushCHROMIUM, ShallowFlushCHROMIUM)

DELEGATE_TO_GL_1(genMailboxCHROMIUM, GenMailboxCHROMIUM, WGC3Dbyte*)
DELEGATE_TO_GL_2(produceTextureCHROMIUM, ProduceTextureCHROMIUM,
                 WGC3Denum, const WGC3Dbyte*)
DELEGATE_TO_GL_2(consumeTextureCHROMIUM, ConsumeTextureCHROMIUM,
                 WGC3Denum, const WGC3Dbyte*)

DELEGATE_TO_GL_2(drawBuffersEXT, DrawBuffersEXT,
                 WGC3Dsizei, const WGC3Denum*)
}  // namespace gpu
}  // namespace webkit
