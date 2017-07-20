/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/gpu/DrawingBuffer.h"

#include <algorithm>
#include <memory>

#include "build/build_config.h"
#include "components/viz/common/quads/shared_bitmap.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/AcceleratedStaticBitmapImage.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/WebGraphicsContext3DProviderWrapper.h"
#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/CheckedNumeric.h"
#include "platform/wtf/typed_arrays/ArrayBufferContents.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebExternalTextureLayer.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

namespace blink {

namespace {

const float kResourceAdjustedRatio = 0.5;

static bool g_should_fail_drawing_buffer_creation_for_testing = false;

}  // namespace

PassRefPtr<DrawingBuffer> DrawingBuffer::Create(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    Client* client,
    const IntSize& size,
    bool premultiplied_alpha,
    bool want_alpha_channel,
    bool want_depth_buffer,
    bool want_stencil_buffer,
    bool want_antialiasing,
    PreserveDrawingBuffer preserve,
    WebGLVersion webgl_version,
    ChromiumImageUsage chromium_image_usage,
    const CanvasColorParams& color_params) {
  DCHECK(context_provider);

  if (g_should_fail_drawing_buffer_creation_for_testing) {
    g_should_fail_drawing_buffer_creation_for_testing = false;
    return nullptr;
  }

  std::unique_ptr<Extensions3DUtil> extensions_util =
      Extensions3DUtil::Create(context_provider->ContextGL());
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
            "GL_CHROMIUM_framebuffer_multisample"))
      extensions_util->EnsureExtensionEnabled(
          "GL_CHROMIUM_framebuffer_multisample");
    else
      extensions_util->EnsureExtensionEnabled(
          "GL_EXT_multisampled_render_to_texture");
  }
  bool discard_framebuffer_supported =
      extensions_util->SupportsExtension("GL_EXT_discard_framebuffer");
  if (discard_framebuffer_supported)
    extensions_util->EnsureExtensionEnabled("GL_EXT_discard_framebuffer");

  RefPtr<DrawingBuffer> drawing_buffer = AdoptRef(new DrawingBuffer(
      std::move(context_provider), std::move(extensions_util), client,
      discard_framebuffer_supported, want_alpha_channel, premultiplied_alpha,
      preserve, webgl_version, want_depth_buffer, want_stencil_buffer,
      chromium_image_usage, color_params));
  if (!drawing_buffer->Initialize(size, multisample_supported)) {
    drawing_buffer->BeginDestruction();
    return PassRefPtr<DrawingBuffer>();
  }
  return drawing_buffer;
}

void DrawingBuffer::ForceNextDrawingBufferCreationToFail() {
  g_should_fail_drawing_buffer_creation_for_testing = true;
}

DrawingBuffer::DrawingBuffer(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    std::unique_ptr<Extensions3DUtil> extensions_util,
    Client* client,
    bool discard_framebuffer_supported,
    bool want_alpha_channel,
    bool premultiplied_alpha,
    PreserveDrawingBuffer preserve,
    WebGLVersion webgl_version,
    bool want_depth,
    bool want_stencil,
    ChromiumImageUsage chromium_image_usage,
    const CanvasColorParams& color_params)
    : client_(client),
      preserve_drawing_buffer_(preserve),
      webgl_version_(webgl_version),
      context_provider_(WTF::WrapUnique(new WebGraphicsContext3DProviderWrapper(
          std::move(context_provider)))),
      gl_(this->ContextProvider()->ContextGL()),
      extensions_util_(std::move(extensions_util)),
      discard_framebuffer_supported_(discard_framebuffer_supported),
      want_alpha_channel_(want_alpha_channel),
      premultiplied_alpha_(premultiplied_alpha),
      software_rendering_(this->ContextProvider()->IsSoftwareRendering()),
      want_depth_(want_depth),
      want_stencil_(want_stencil),
      color_space_(color_params.GetGfxColorSpace()),
      chromium_image_usage_(chromium_image_usage) {
  // Used by browser tests to detect the use of a DrawingBuffer.
  TRACE_EVENT_INSTANT0("test_gpu", "DrawingBufferCreation",
                       TRACE_EVENT_SCOPE_GLOBAL);
}

DrawingBuffer::~DrawingBuffer() {
  DCHECK(destruction_in_progress_);
  layer_.reset();
  context_provider_.reset();
}

bool DrawingBuffer::MarkContentsChanged() {
  if (contents_change_resolved_ || !contents_changed_) {
    contents_change_resolved_ = false;
    contents_changed_ = true;
    return true;
  }
  return false;
}

bool DrawingBuffer::BufferClearNeeded() const {
  return buffer_clear_needed_;
}

void DrawingBuffer::SetBufferClearNeeded(bool flag) {
  if (preserve_drawing_buffer_ == kDiscard) {
    buffer_clear_needed_ = flag;
  } else {
    DCHECK(!buffer_clear_needed_);
  }
}

gpu::gles2::GLES2Interface* DrawingBuffer::ContextGL() {
  return gl_;
}

WebGraphicsContext3DProvider* DrawingBuffer::ContextProvider() {
  return context_provider_->ContextProvider();
}

void DrawingBuffer::SetIsHidden(bool hidden) {
  if (is_hidden_ == hidden)
    return;
  is_hidden_ = hidden;
  if (is_hidden_)
    recycled_color_buffer_queue_.clear();
}

void DrawingBuffer::SetFilterQuality(SkFilterQuality filter_quality) {
  if (filter_quality_ != filter_quality) {
    filter_quality_ = filter_quality;
    if (layer_)
      layer_->SetNearestNeighbor(filter_quality == kNone_SkFilterQuality);
  }
}

bool DrawingBuffer::RequiresAlphaChannelToBePreserved() {
  return client_->DrawingBufferClientIsBoundForDraw() &&
         DefaultBufferRequiresAlphaChannelToBePreserved();
}

bool DrawingBuffer::DefaultBufferRequiresAlphaChannelToBePreserved() {
  if (WantExplicitResolve()) {
    return !want_alpha_channel_ &&
           GetMultisampledRenderbufferFormat() == GL_RGBA8_OES;
  }

  bool rgb_emulation =
      ContextProvider()->GetCapabilities().emulate_rgb_buffer_with_rgba ||
      (ShouldUseChromiumImage() &&
       ContextProvider()->GetCapabilities().chromium_image_rgb_emulation);
  return !want_alpha_channel_ && rgb_emulation;
}

std::unique_ptr<viz::SharedBitmap> DrawingBuffer::CreateOrRecycleBitmap() {
  auto it = std::remove_if(
      recycled_bitmaps_.begin(), recycled_bitmaps_.end(),
      [this](const RecycledBitmap& bitmap) { return bitmap.size != size_; });
  recycled_bitmaps_.Shrink(it - recycled_bitmaps_.begin());

  if (!recycled_bitmaps_.IsEmpty()) {
    RecycledBitmap recycled = std::move(recycled_bitmaps_.back());
    recycled_bitmaps_.pop_back();
    DCHECK(recycled.size == size_);
    return std::move(recycled.bitmap);
  }

  return Platform::Current()->AllocateSharedBitmap(size_);
}

bool DrawingBuffer::PrepareTextureMailbox(
    viz::TextureMailbox* out_mailbox,
    std::unique_ptr<cc::SingleReleaseCallback>* out_release_callback) {
  ScopedStateRestorer scoped_state_restorer(this);
  bool force_gpu_result = false;
  return PrepareTextureMailboxInternal(out_mailbox, out_release_callback,
                                       force_gpu_result);
}

bool DrawingBuffer::PrepareTextureMailboxInternal(
    viz::TextureMailbox* out_mailbox,
    std::unique_ptr<cc::SingleReleaseCallback>* out_release_callback,
    bool force_gpu_result) {
  DCHECK(state_restorer_);
  if (destruction_in_progress_) {
    // It can be hit in the following sequence.
    // 1. WebGL draws something.
    // 2. The compositor begins the frame.
    // 3. Javascript makes a context lost using WEBGL_lose_context extension.
    // 4. Here.
    return false;
  }
  DCHECK(!is_hidden_);
  if (!contents_changed_)
    return false;

  // If the context is lost, we don't know if we should be producing GPU or
  // software frames, until we get a new context, since the compositor will
  // be trying to get a new context and may change modes.
  if (gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    return false;

  TRACE_EVENT0("blink,rail", "DrawingBuffer::prepareMailbox");

  // Resolve the multisampled buffer into m_backColorBuffer texture.
  ResolveIfNeeded();

  if (software_rendering_ && !force_gpu_result) {
    return FinishPrepareTextureMailboxSoftware(out_mailbox,
                                               out_release_callback);
  } else {
    return FinishPrepareTextureMailboxGpu(out_mailbox, out_release_callback);
  }
}

bool DrawingBuffer::FinishPrepareTextureMailboxSoftware(
    viz::TextureMailbox* out_mailbox,
    std::unique_ptr<cc::SingleReleaseCallback>* out_release_callback) {
  DCHECK(state_restorer_);
  std::unique_ptr<viz::SharedBitmap> bitmap = CreateOrRecycleBitmap();
  if (!bitmap)
    return false;

  // Read the framebuffer into |bitmap|.
  {
    unsigned char* pixels = bitmap->pixels();
    DCHECK(pixels);
    bool need_premultiply = want_alpha_channel_ && !premultiplied_alpha_;
    WebGLImageConversion::AlphaOp op =
        need_premultiply ? WebGLImageConversion::kAlphaDoPremultiply
                         : WebGLImageConversion::kAlphaDoNothing;
    state_restorer_->SetFramebufferBindingDirty();
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    ReadBackFramebuffer(pixels, Size().Width(), Size().Height(), kReadbackSkia,
                        op);
  }

  *out_mailbox = viz::TextureMailbox(bitmap.get(), size_);
  out_mailbox->set_color_space(color_space_);

  // This holds a ref on the DrawingBuffer that will keep it alive until the
  // mailbox is released (and while the release callback is running). It also
  // owns the SharedBitmap.
  auto func = WTF::Bind(&DrawingBuffer::MailboxReleasedSoftware,
                        RefPtr<DrawingBuffer>(this),
                        WTF::Passed(std::move(bitmap)), size_);
  *out_release_callback =
      cc::SingleReleaseCallback::Create(ConvertToBaseCallback(std::move(func)));

  if (preserve_drawing_buffer_ == kDiscard) {
    SetBufferClearNeeded(true);
  }

  return true;
}

bool DrawingBuffer::FinishPrepareTextureMailboxGpu(
    viz::TextureMailbox* out_mailbox,
    std::unique_ptr<cc::SingleReleaseCallback>* out_release_callback) {
  DCHECK(state_restorer_);
  if (webgl_version_ > kWebGL1) {
    state_restorer_->SetPixelUnpackBufferBindingDirty();
    gl_->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  }

  // Specify the buffer that we will put in the mailbox.
  RefPtr<ColorBuffer> color_buffer_for_mailbox;
  if (preserve_drawing_buffer_ == kDiscard) {
    // If we can discard the backbuffer, send the old backbuffer directly
    // into the mailbox, and allocate (or recycle) a new backbuffer.
    color_buffer_for_mailbox = back_color_buffer_;
    back_color_buffer_ = CreateOrRecycleColorBuffer();
    AttachColorBufferToReadFramebuffer();

    // Explicitly specify that m_fbo (which is now bound to the just-allocated
    // m_backColorBuffer) is not initialized, to save GPU memory bandwidth for
    // tile-based GPU architectures.
    if (discard_framebuffer_supported_) {
      const GLenum kAttachments[3] = {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT,
                                      GL_STENCIL_ATTACHMENT};
      state_restorer_->SetFramebufferBindingDirty();
      gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
      gl_->DiscardFramebufferEXT(GL_FRAMEBUFFER, 3, kAttachments);
    }
  } else {
    // If we can't discard the backbuffer, create (or recycle) a buffer to put
    // in the mailbox, and copy backbuffer's contents there.
    color_buffer_for_mailbox = CreateOrRecycleColorBuffer();
    gl_->CopySubTextureCHROMIUM(back_color_buffer_->texture_id, 0,
                                color_buffer_for_mailbox->parameters.target,
                                color_buffer_for_mailbox->texture_id, 0, 0, 0,
                                0, 0, size_.Width(), size_.Height(), GL_FALSE,
                                GL_FALSE, GL_FALSE);
  }

  // Put colorBufferForMailbox into its mailbox, and populate its
  // produceSyncToken with that point.
  {
    gl_->ProduceTextureDirectCHROMIUM(
        color_buffer_for_mailbox->texture_id,
        color_buffer_for_mailbox->parameters.target,
        color_buffer_for_mailbox->mailbox.name);
    const GLuint64 fence_sync = gl_->InsertFenceSyncCHROMIUM();
#if defined(OS_MACOSX)
    gl_->DescheduleUntilFinishedCHROMIUM();
#endif
    gl_->Flush();
    gl_->GenSyncTokenCHROMIUM(
        fence_sync, color_buffer_for_mailbox->produce_sync_token.GetData());
  }

  // Populate the output mailbox and callback.
  {
    bool is_overlay_candidate = color_buffer_for_mailbox->image_id != 0;
    bool secure_output_only = false;
    *out_mailbox = viz::TextureMailbox(
        color_buffer_for_mailbox->mailbox,
        color_buffer_for_mailbox->produce_sync_token,
        color_buffer_for_mailbox->parameters.target, gfx::Size(size_),
        is_overlay_candidate, secure_output_only);
    out_mailbox->set_color_space(color_space_);

    // This holds a ref on the DrawingBuffer that will keep it alive until the
    // mailbox is released (and while the release callback is running).
    auto func =
        WTF::Bind(&DrawingBuffer::MailboxReleasedGpu,
                  RefPtr<DrawingBuffer>(this), color_buffer_for_mailbox);
    *out_release_callback = cc::SingleReleaseCallback::Create(
        ConvertToBaseCallback(std::move(func)));
  }

  // Point |m_frontColorBuffer| to the buffer that we are now presenting.
  front_color_buffer_ = color_buffer_for_mailbox;

  contents_changed_ = false;
  SetBufferClearNeeded(true);
  return true;
}

void DrawingBuffer::MailboxReleasedGpu(RefPtr<ColorBuffer> color_buffer,
                                       const gpu::SyncToken& sync_token,
                                       bool lost_resource) {
  // If the mailbox has been returned by the compositor then it is no
  // longer being presented, and so is no longer the front buffer.
  if (color_buffer == front_color_buffer_)
    front_color_buffer_ = nullptr;

  // Update the SyncToken to ensure that we will wait for it even if we
  // immediately destroy this buffer.
  color_buffer->receive_sync_token = sync_token;

  if (destruction_in_progress_ || color_buffer->size != size_ ||
      gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR || lost_resource ||
      is_hidden_) {
    return;
  }

  // Creation of image backed mailboxes is very expensive, so be less
  // aggressive about pruning them. Pruning is done in FIFO order.
  size_t cache_limit = 1;
  if (ShouldUseChromiumImage())
    cache_limit = 4;
  while (recycled_color_buffer_queue_.size() >= cache_limit)
    recycled_color_buffer_queue_.TakeLast();

  recycled_color_buffer_queue_.push_front(color_buffer);
}

void DrawingBuffer::MailboxReleasedSoftware(
    std::unique_ptr<viz::SharedBitmap> bitmap,
    const IntSize& size,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  DCHECK(!sync_token.HasData());  // No sync tokens for software resources.
  if (destruction_in_progress_ || lost_resource || is_hidden_ || size != size_)
    return;  // Just delete the bitmap.

  RecycledBitmap recycled = {std::move(bitmap), size_};
  recycled_bitmaps_.push_back(std::move(recycled));
}

PassRefPtr<StaticBitmapImage> DrawingBuffer::TransferToStaticBitmapImage() {
  ScopedStateRestorer scoped_state_restorer(this);

  // This can be null if the context is lost before the first call to
  // grContext().
  GrContext* gr_context = ContextProvider()->GetGrContext();

  viz::TextureMailbox texture_mailbox;
  std::unique_ptr<cc::SingleReleaseCallback> release_callback;
  bool success = false;
  if (gr_context) {
    bool force_gpu_result = true;
    success = PrepareTextureMailboxInternal(&texture_mailbox, &release_callback,
                                            force_gpu_result);
  }
  if (!success) {
    // If we can't get a mailbox, return an transparent black ImageBitmap.
    // The only situation in which this could happen is when two or more calls
    // to transferToImageBitmap are made back-to-back, or when the context gets
    // lost.
    sk_sp<SkSurface> surface =
        SkSurface::MakeRasterN32Premul(size_.Width(), size_.Height());
    return StaticBitmapImage::Create(surface->makeImageSnapshot());
  }

  DCHECK_EQ(size_.Width(), texture_mailbox.size_in_pixels().width());
  DCHECK_EQ(size_.Height(), texture_mailbox.size_in_pixels().height());

  // Make our own textureId that is a reference on the same texture backing
  // being used as the front buffer (which was returned from
  // PrepareTextureMailbox()). We do not need to wait on the sync token in
  // |textureMailbox| since the mailbox was produced on the same |m_gl| context
  // that we are using here. Similarly, the |releaseCallback| will run on the
  // same context so we don't need to send a sync token for this consume action
  // back to it.
  // TODO(danakj): Instead of using PrepareTextureMailbox(), we could just use
  // the actual texture id and avoid needing to produce/consume a mailbox.
  GLuint texture_id = gl_->CreateAndConsumeTextureCHROMIUM(
      GL_TEXTURE_2D, texture_mailbox.name());
  // Return the mailbox but report that the resource is lost to prevent trying
  // to use the backing for future frames. We keep it alive with our own
  // reference to the backing via our |textureId|.
  release_callback->Run(gpu::SyncToken(), true /* lostResource */);

  // We reuse the same mailbox name from above since our texture id was consumed
  // from it.
  const auto& sk_image_mailbox = texture_mailbox.mailbox();
  // Use the sync token generated after producing the mailbox. Waiting for this
  // before trying to use the mailbox with some other context will ensure it is
  // valid. We wouldn't need to wait for the consume done in this function
  // because the texture id it generated would only be valid for the
  // DrawingBuffer's context anyways.
  const auto& sk_image_sync_token = texture_mailbox.sync_token();

  // TODO(xidachen): Create a small pool of recycled textures from
  // ImageBitmapRenderingContext's transferFromImageBitmap, and try to use them
  // in DrawingBuffer.
  return AcceleratedStaticBitmapImage::CreateFromWebGLContextImage(
      sk_image_mailbox, sk_image_sync_token, texture_id,
      context_provider_->CreateWeakPtr(), size_);
}

DrawingBuffer::ColorBufferParameters
DrawingBuffer::GpuMemoryBufferColorBufferParameters() {
#if defined(OS_MACOSX)
  // A CHROMIUM_image backed texture requires a specialized set of parameters
  // on OSX.
  ColorBufferParameters parameters;
  parameters.target = GC3D_TEXTURE_RECTANGLE_ARB;

  if (want_alpha_channel_) {
    parameters.allocate_alpha_channel = true;
  } else if (ContextProvider()
                 ->GetCapabilities()
                 .chromium_image_rgb_emulation) {
    parameters.allocate_alpha_channel = false;
  } else {
    parameters.allocate_alpha_channel =
        DefaultBufferRequiresAlphaChannelToBePreserved();
  }
  return parameters;
#else
  return TextureColorBufferParameters();
#endif
}

DrawingBuffer::ColorBufferParameters
DrawingBuffer::TextureColorBufferParameters() {
  ColorBufferParameters parameters;
  parameters.target = GL_TEXTURE_2D;
  if (want_alpha_channel_) {
    parameters.allocate_alpha_channel = true;
  } else if (ContextProvider()
                 ->GetCapabilities()
                 .emulate_rgb_buffer_with_rgba) {
    parameters.allocate_alpha_channel = true;
  } else {
    parameters.allocate_alpha_channel =
        DefaultBufferRequiresAlphaChannelToBePreserved();
  }
  return parameters;
}

PassRefPtr<DrawingBuffer::ColorBuffer>
DrawingBuffer::CreateOrRecycleColorBuffer() {
  DCHECK(state_restorer_);
  if (!recycled_color_buffer_queue_.IsEmpty()) {
    RefPtr<ColorBuffer> recycled = recycled_color_buffer_queue_.TakeLast();
    if (recycled->receive_sync_token.HasData())
      gl_->WaitSyncTokenCHROMIUM(recycled->receive_sync_token.GetData());
    DCHECK(recycled->size == size_);
    return recycled;
  }
  return CreateColorBuffer(size_);
}

DrawingBuffer::ScopedRGBEmulationForBlitFramebuffer::
    ScopedRGBEmulationForBlitFramebuffer(DrawingBuffer* drawing_buffer)
    : drawing_buffer_(drawing_buffer) {
  doing_work_ = drawing_buffer->SetupRGBEmulationForBlitFramebuffer();
}

DrawingBuffer::ScopedRGBEmulationForBlitFramebuffer::
    ~ScopedRGBEmulationForBlitFramebuffer() {
  if (doing_work_) {
    drawing_buffer_->CleanupRGBEmulationForBlitFramebuffer();
  }
}

DrawingBuffer::ColorBuffer::ColorBuffer(
    DrawingBuffer* drawing_buffer,
    const ColorBufferParameters& parameters,
    const IntSize& size,
    GLuint texture_id,
    GLuint image_id,
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer)
    : drawing_buffer(drawing_buffer),
      parameters(parameters),
      size(size),
      texture_id(texture_id),
      image_id(image_id),
      gpu_memory_buffer(std::move(gpu_memory_buffer)) {
  drawing_buffer->ContextGL()->GenMailboxCHROMIUM(mailbox.name);
}

DrawingBuffer::ColorBuffer::~ColorBuffer() {
  gpu::gles2::GLES2Interface* gl = drawing_buffer->gl_;
  if (receive_sync_token.HasData())
    gl->WaitSyncTokenCHROMIUM(receive_sync_token.GetConstData());
  if (image_id) {
    gl->BindTexture(parameters.target, texture_id);
    gl->ReleaseTexImage2DCHROMIUM(parameters.target, image_id);
    if (rgb_workaround_texture_id) {
      gl->BindTexture(parameters.target, rgb_workaround_texture_id);
      gl->ReleaseTexImage2DCHROMIUM(parameters.target, image_id);
    }
    gl->DestroyImageCHROMIUM(image_id);
    switch (parameters.target) {
      case GL_TEXTURE_2D:
        // Restore the texture binding for GL_TEXTURE_2D, since the client will
        // expect the previous state.
        if (drawing_buffer->client_)
          drawing_buffer->client_->DrawingBufferClientRestoreTexture2DBinding();
        break;
      case GC3D_TEXTURE_RECTANGLE_ARB:
        // Rectangle textures aren't exposed to WebGL, so don't bother
        // restoring this state (there is no meaningful way to restore it).
        break;
      default:
        NOTREACHED();
        break;
    }
    gpu_memory_buffer.reset();
  }
  gl->DeleteTextures(1, &texture_id);
  if (rgb_workaround_texture_id) {
    // Avoid deleting this texture if it was unused.
    gl->DeleteTextures(1, &rgb_workaround_texture_id);
  }
}

bool DrawingBuffer::Initialize(const IntSize& size, bool use_multisampling) {
  ScopedStateRestorer scoped_state_restorer(this);

  if (gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    // Need to try to restore the context again later.
    return false;
  }

  gl_->GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size_);

  int max_sample_count = 0;
  anti_aliasing_mode_ = kNone;
  if (use_multisampling) {
    gl_->GetIntegerv(GL_MAX_SAMPLES_ANGLE, &max_sample_count);
    anti_aliasing_mode_ = kMSAAExplicitResolve;
    if (extensions_util_->SupportsExtension(
            "GL_EXT_multisampled_render_to_texture")) {
      anti_aliasing_mode_ = kMSAAImplicitResolve;
    } else if (extensions_util_->SupportsExtension(
                   "GL_CHROMIUM_screen_space_antialiasing")) {
      anti_aliasing_mode_ = kScreenSpaceAntialiasing;
    }
  }
  // TODO(dshwang): Enable storage textures on all platforms. crbug.com/557848
  // The Linux ATI bot fails
  // WebglConformance.conformance_textures_misc_tex_image_webgl, so use storage
  // textures only if ScreenSpaceAntialiasing is enabled, because
  // ScreenSpaceAntialiasing is much faster with storage textures.
  storage_texture_supported_ =
      (webgl_version_ > kWebGL1 ||
       extensions_util_->SupportsExtension("GL_EXT_texture_storage")) &&
      anti_aliasing_mode_ == kScreenSpaceAntialiasing;
  sample_count_ = std::min(4, max_sample_count);

  state_restorer_->SetFramebufferBindingDirty();
  gl_->GenFramebuffers(1, &fbo_);
  gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  if (WantExplicitResolve()) {
    gl_->GenFramebuffers(1, &multisample_fbo_);
    gl_->BindFramebuffer(GL_FRAMEBUFFER, multisample_fbo_);
    gl_->GenRenderbuffers(1, &multisample_renderbuffer_);
  }
  if (!ResizeFramebufferInternal(size))
    return false;

  if (depth_stencil_buffer_) {
    DCHECK(WantDepthOrStencil());
    has_implicit_stencil_buffer_ = !want_stencil_;
  }

  if (gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    // It's possible that the drawing buffer allocation provokes a context loss,
    // so check again just in case. http://crbug.com/512302
    return false;
  }

  return true;
}

bool DrawingBuffer::CopyToPlatformTexture(gpu::gles2::GLES2Interface* gl,
                                          GLenum texture_target,
                                          GLuint texture,
                                          bool premultiply_alpha,
                                          bool flip_y,
                                          const IntPoint& dest_texture_offset,
                                          const IntRect& source_sub_rectangle,
                                          SourceDrawingBuffer source_buffer) {
  ScopedStateRestorer scoped_state_restorer(this);

  if (contents_changed_) {
    ResolveIfNeeded();
    gl_->Flush();
  }

  if (!Extensions3DUtil::CanUseCopyTextureCHROMIUM(texture_target))
    return false;

  // Contexts may be in a different share group. We must transfer the texture
  // through a mailbox first.
  GLenum target = 0;
  gpu::Mailbox mailbox;
  gpu::SyncToken produce_sync_token;
  if (source_buffer == kFrontBuffer && front_color_buffer_) {
    target = front_color_buffer_->parameters.target;
    mailbox = front_color_buffer_->mailbox;
    produce_sync_token = front_color_buffer_->produce_sync_token;
  } else {
    target = back_color_buffer_->parameters.target;
    gl_->GenMailboxCHROMIUM(mailbox.name);
    gl_->ProduceTextureDirectCHROMIUM(back_color_buffer_->texture_id, target,
                                      mailbox.name);
    const GLuint64 fence_sync = gl_->InsertFenceSyncCHROMIUM();
    gl_->Flush();
    gl_->GenSyncTokenCHROMIUM(fence_sync, produce_sync_token.GetData());
  }

  if (!produce_sync_token.HasData()) {
    // This should only happen if the context has been lost.
    return false;
  }

  gl->WaitSyncTokenCHROMIUM(produce_sync_token.GetConstData());
  GLuint source_texture =
      gl->CreateAndConsumeTextureCHROMIUM(target, mailbox.name);

  GLboolean unpack_premultiply_alpha_needed = GL_FALSE;
  GLboolean unpack_unpremultiply_alpha_needed = GL_FALSE;
  if (want_alpha_channel_ && premultiplied_alpha_ && !premultiply_alpha)
    unpack_unpremultiply_alpha_needed = GL_TRUE;
  else if (want_alpha_channel_ && !premultiplied_alpha_ && premultiply_alpha)
    unpack_premultiply_alpha_needed = GL_TRUE;

  gl->CopySubTextureCHROMIUM(
      source_texture, 0, texture_target, texture, 0, dest_texture_offset.X(),
      dest_texture_offset.Y(), source_sub_rectangle.X(),
      source_sub_rectangle.Y(), source_sub_rectangle.Width(),
      source_sub_rectangle.Height(), flip_y, unpack_premultiply_alpha_needed,
      unpack_unpremultiply_alpha_needed);

  gl->DeleteTextures(1, &source_texture);

  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();

  gl->Flush();
  gpu::SyncToken sync_token;
  gl->GenSyncTokenCHROMIUM(fence_sync, sync_token.GetData());
  gl_->WaitSyncTokenCHROMIUM(sync_token.GetData());

  return true;
}

WebLayer* DrawingBuffer::PlatformLayer() {
  if (!layer_) {
    layer_ =
        Platform::Current()->CompositorSupport()->CreateExternalTextureLayer(
            this);

    layer_->SetOpaque(!want_alpha_channel_);
    layer_->SetBlendBackgroundColor(want_alpha_channel_);
    layer_->SetPremultipliedAlpha(premultiplied_alpha_);
    layer_->SetNearestNeighbor(filter_quality_ == kNone_SkFilterQuality);
    GraphicsLayer::RegisterContentsLayer(layer_->Layer());
  }

  return layer_->Layer();
}

void DrawingBuffer::ClearPlatformLayer() {
  if (layer_)
    layer_->ClearTexture();

  gl_->Flush();
}

void DrawingBuffer::BeginDestruction() {
  DCHECK(!destruction_in_progress_);
  destruction_in_progress_ = true;

  ClearPlatformLayer();
  recycled_color_buffer_queue_.clear();

  if (multisample_fbo_)
    gl_->DeleteFramebuffers(1, &multisample_fbo_);

  if (fbo_)
    gl_->DeleteFramebuffers(1, &fbo_);

  if (multisample_renderbuffer_)
    gl_->DeleteRenderbuffers(1, &multisample_renderbuffer_);

  if (depth_stencil_buffer_)
    gl_->DeleteRenderbuffers(1, &depth_stencil_buffer_);

  size_ = IntSize();

  back_color_buffer_ = nullptr;
  front_color_buffer_ = nullptr;
  multisample_renderbuffer_ = 0;
  depth_stencil_buffer_ = 0;
  multisample_fbo_ = 0;
  fbo_ = 0;

  if (layer_)
    GraphicsLayer::UnregisterContentsLayer(layer_->Layer());

  client_ = nullptr;
}

bool DrawingBuffer::ResizeDefaultFramebuffer(const IntSize& size) {
  DCHECK(state_restorer_);
  // Recreate m_backColorBuffer.
  back_color_buffer_ = CreateColorBuffer(size);

  AttachColorBufferToReadFramebuffer();

  if (WantExplicitResolve()) {
    state_restorer_->SetFramebufferBindingDirty();
    state_restorer_->SetRenderbufferBindingDirty();
    gl_->BindFramebuffer(GL_FRAMEBUFFER, multisample_fbo_);
    gl_->BindRenderbuffer(GL_RENDERBUFFER, multisample_renderbuffer_);
    gl_->RenderbufferStorageMultisampleCHROMIUM(
        GL_RENDERBUFFER, sample_count_, GetMultisampledRenderbufferFormat(),
        size.Width(), size.Height());

    if (gl_->GetError() == GL_OUT_OF_MEMORY)
      return false;

    gl_->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                 GL_RENDERBUFFER, multisample_renderbuffer_);
  }

  if (WantDepthOrStencil()) {
    state_restorer_->SetFramebufferBindingDirty();
    state_restorer_->SetRenderbufferBindingDirty();
    gl_->BindFramebuffer(GL_FRAMEBUFFER,
                         multisample_fbo_ ? multisample_fbo_ : fbo_);
    if (!depth_stencil_buffer_)
      gl_->GenRenderbuffers(1, &depth_stencil_buffer_);
    gl_->BindRenderbuffer(GL_RENDERBUFFER, depth_stencil_buffer_);
    if (anti_aliasing_mode_ == kMSAAImplicitResolve) {
      gl_->RenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, sample_count_,
                                             GL_DEPTH24_STENCIL8_OES,
                                             size.Width(), size.Height());
    } else if (anti_aliasing_mode_ == kMSAAExplicitResolve) {
      gl_->RenderbufferStorageMultisampleCHROMIUM(
          GL_RENDERBUFFER, sample_count_, GL_DEPTH24_STENCIL8_OES, size.Width(),
          size.Height());
    } else {
      gl_->RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES,
                               size.Width(), size.Height());
    }
    // For ES 2.0 contexts DEPTH_STENCIL is not available natively, so we
    // emulate
    // it at the command buffer level for WebGL contexts.
    gl_->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                 GL_RENDERBUFFER, depth_stencil_buffer_);
    gl_->BindRenderbuffer(GL_RENDERBUFFER, 0);
  }

  if (WantExplicitResolve()) {
    state_restorer_->SetFramebufferBindingDirty();
    gl_->BindFramebuffer(GL_FRAMEBUFFER, multisample_fbo_);
    if (gl_->CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      return false;
  }

  state_restorer_->SetFramebufferBindingDirty();
  gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  return gl_->CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void DrawingBuffer::ClearFramebuffers(GLbitfield clear_mask) {
  ScopedStateRestorer scoped_state_restorer(this);
  ClearFramebuffersInternal(clear_mask);
}

void DrawingBuffer::ClearFramebuffersInternal(GLbitfield clear_mask) {
  DCHECK(state_restorer_);
  state_restorer_->SetFramebufferBindingDirty();
  // We will clear the multisample FBO, but we also need to clear the
  // non-multisampled buffer.
  if (multisample_fbo_) {
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl_->Clear(GL_COLOR_BUFFER_BIT);
  }

  gl_->BindFramebuffer(GL_FRAMEBUFFER,
                       multisample_fbo_ ? multisample_fbo_ : fbo_);
  gl_->Clear(clear_mask);
}

IntSize DrawingBuffer::AdjustSize(const IntSize& desired_size,
                                  const IntSize& cur_size,
                                  int max_texture_size) {
  IntSize adjusted_size = desired_size;

  // Clamp if the desired size is greater than the maximum texture size for the
  // device.
  if (adjusted_size.Height() > max_texture_size)
    adjusted_size.SetHeight(max_texture_size);

  if (adjusted_size.Width() > max_texture_size)
    adjusted_size.SetWidth(max_texture_size);

  return adjusted_size;
}

bool DrawingBuffer::Resize(const IntSize& new_size) {
  ScopedStateRestorer scoped_state_restorer(this);
  return ResizeFramebufferInternal(new_size);
}

bool DrawingBuffer::ResizeFramebufferInternal(const IntSize& new_size) {
  DCHECK(state_restorer_);
  DCHECK(!new_size.IsEmpty());
  IntSize adjusted_size = AdjustSize(new_size, size_, max_texture_size_);
  if (adjusted_size.IsEmpty())
    return false;

  if (adjusted_size != size_) {
    do {
      if (!ResizeDefaultFramebuffer(adjusted_size)) {
        adjusted_size.Scale(kResourceAdjustedRatio);
        continue;
      }
      break;
    } while (!adjusted_size.IsEmpty());

    size_ = adjusted_size;
    // Free all mailboxes, because they are now of the wrong size. Only the
    // first call in this loop has any effect.
    recycled_color_buffer_queue_.clear();
    recycled_bitmaps_.clear();

    if (adjusted_size.IsEmpty())
      return false;
  }

  state_restorer_->SetClearStateDirty();
  gl_->Disable(GL_SCISSOR_TEST);
  gl_->ClearColor(0, 0, 0,
                  DefaultBufferRequiresAlphaChannelToBePreserved() ? 1 : 0);
  gl_->ColorMask(true, true, true, true);

  GLbitfield clear_mask = GL_COLOR_BUFFER_BIT;
  if (!!depth_stencil_buffer_) {
    gl_->ClearDepthf(1.0f);
    clear_mask |= GL_DEPTH_BUFFER_BIT;
    gl_->DepthMask(true);
  }
  if (!!depth_stencil_buffer_) {
    gl_->ClearStencil(0);
    clear_mask |= GL_STENCIL_BUFFER_BIT;
    gl_->StencilMaskSeparate(GL_FRONT, 0xFFFFFFFF);
  }

  ClearFramebuffersInternal(clear_mask);
  return true;
}

void DrawingBuffer::ResolveAndBindForReadAndDraw() {
  {
    ScopedStateRestorer scoped_state_restorer(this);
    ResolveIfNeeded();
  }
  gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
}

void DrawingBuffer::ResolveMultisampleFramebufferInternal() {
  DCHECK(state_restorer_);
  state_restorer_->SetFramebufferBindingDirty();
  if (WantExplicitResolve()) {
    state_restorer_->SetClearStateDirty();
    gl_->BindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, multisample_fbo_);
    gl_->BindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, fbo_);
    gl_->Disable(GL_SCISSOR_TEST);

    int width = size_.Width();
    int height = size_.Height();
    // Use NEAREST, because there is no scale performed during the blit.
    GLuint filter = GL_NEAREST;

    gl_->BlitFramebufferCHROMIUM(0, 0, width, height, 0, 0, width, height,
                                 GL_COLOR_BUFFER_BIT, filter);

    // On old AMD GPUs on OS X, glColorMask doesn't work correctly for
    // multisampled renderbuffers and the alpha channel can be overwritten.
    // Clear the alpha channel of |m_fbo|.
    if (DefaultBufferRequiresAlphaChannelToBePreserved() &&
        ContextProvider()
            ->GetCapabilities()
            .disable_multisampling_color_mask_usage) {
      gl_->ClearColor(0, 0, 0, 1);
      gl_->ColorMask(false, false, false, true);
      gl_->Clear(GL_COLOR_BUFFER_BIT);
    }
  }

  gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  if (anti_aliasing_mode_ == kScreenSpaceAntialiasing)
    gl_->ApplyScreenSpaceAntialiasingCHROMIUM();
}

void DrawingBuffer::ResolveIfNeeded() {
  if (anti_aliasing_mode_ != kNone && !contents_change_resolved_)
    ResolveMultisampleFramebufferInternal();
  contents_change_resolved_ = true;
}

void DrawingBuffer::RestoreFramebufferBindings() {
  client_->DrawingBufferClientRestoreFramebufferBinding();
}

void DrawingBuffer::RestoreAllState() {
  client_->DrawingBufferClientRestoreScissorTest();
  client_->DrawingBufferClientRestoreMaskAndClearValues();
  client_->DrawingBufferClientRestorePixelPackParameters();
  client_->DrawingBufferClientRestoreTexture2DBinding();
  client_->DrawingBufferClientRestoreRenderbufferBinding();
  client_->DrawingBufferClientRestoreFramebufferBinding();
  client_->DrawingBufferClientRestorePixelUnpackBufferBinding();
  client_->DrawingBufferClientRestorePixelPackBufferBinding();
}

bool DrawingBuffer::Multisample() const {
  return anti_aliasing_mode_ != kNone;
}

void DrawingBuffer::Bind(GLenum target) {
  gl_->BindFramebuffer(target, WantExplicitResolve() ? multisample_fbo_ : fbo_);
}

bool DrawingBuffer::PaintRenderingResultsToImageData(
    int& width,
    int& height,
    SourceDrawingBuffer source_buffer,
    WTF::ArrayBufferContents& contents) {
  ScopedStateRestorer scoped_state_restorer(this);

  DCHECK(!premultiplied_alpha_);
  width = Size().Width();
  height = Size().Height();

  CheckedNumeric<int> data_size = 4;
  data_size *= width;
  data_size *= height;
  if (!data_size.IsValid())
    return false;

  WTF::ArrayBufferContents pixels(width * height, 4,
                                  WTF::ArrayBufferContents::kNotShared,
                                  WTF::ArrayBufferContents::kDontInitialize);

  GLuint fbo = 0;
  state_restorer_->SetFramebufferBindingDirty();
  if (source_buffer == kFrontBuffer && front_color_buffer_) {
    gl_->GenFramebuffers(1, &fbo);
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              front_color_buffer_->parameters.target,
                              front_color_buffer_->texture_id, 0);
  } else {
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  }

  ReadBackFramebuffer(static_cast<unsigned char*>(pixels.Data()), width, height,
                      kReadbackRGBA, WebGLImageConversion::kAlphaDoNothing);
  FlipVertically(static_cast<uint8_t*>(pixels.Data()), width, height);

  if (fbo) {
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              front_color_buffer_->parameters.target, 0, 0);
    gl_->DeleteFramebuffers(1, &fbo);
  }

  pixels.Transfer(contents);
  return true;
}

void DrawingBuffer::ReadBackFramebuffer(unsigned char* pixels,
                                        int width,
                                        int height,
                                        ReadbackOrder readback_order,
                                        WebGLImageConversion::AlphaOp op) {
  DCHECK(state_restorer_);
  state_restorer_->SetPixelPackParametersDirty();
  gl_->PixelStorei(GL_PACK_ALIGNMENT, 1);
  if (webgl_version_ > kWebGL1) {
    gl_->PixelStorei(GL_PACK_SKIP_ROWS, 0);
    gl_->PixelStorei(GL_PACK_SKIP_PIXELS, 0);
    gl_->PixelStorei(GL_PACK_ROW_LENGTH, 0);

    state_restorer_->SetPixelPackBufferBindingDirty();
    gl_->BindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  }
  gl_->ReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  size_t buffer_size = 4 * width * height;

  if (readback_order == kReadbackSkia) {
#if (SK_R32_SHIFT == 16) && !SK_B32_SHIFT
    // Swizzle red and blue channels to match SkBitmap's byte ordering.
    // TODO(kbr): expose GL_BGRA as extension.
    for (size_t i = 0; i < buffer_size; i += 4) {
      std::swap(pixels[i], pixels[i + 2]);
    }
#endif
  }

  if (op == WebGLImageConversion::kAlphaDoPremultiply) {
    for (size_t i = 0; i < buffer_size; i += 4) {
      pixels[i + 0] = std::min(255, pixels[i + 0] * pixels[i + 3] / 255);
      pixels[i + 1] = std::min(255, pixels[i + 1] * pixels[i + 3] / 255);
      pixels[i + 2] = std::min(255, pixels[i + 2] * pixels[i + 3] / 255);
    }
  } else if (op != WebGLImageConversion::kAlphaDoNothing) {
    NOTREACHED();
  }
}

void DrawingBuffer::FlipVertically(uint8_t* framebuffer,
                                   int width,
                                   int height) {
  std::vector<uint8_t> scanline(width * 4);
  unsigned row_bytes = width * 4;
  unsigned count = height / 2;
  for (unsigned i = 0; i < count; i++) {
    uint8_t* row_a = framebuffer + i * row_bytes;
    uint8_t* row_b = framebuffer + (height - i - 1) * row_bytes;
    memcpy(scanline.data(), row_b, row_bytes);
    memcpy(row_b, row_a, row_bytes);
    memcpy(row_a, scanline.data(), row_bytes);
  }
}

RefPtr<DrawingBuffer::ColorBuffer> DrawingBuffer::CreateColorBuffer(
    const IntSize& size) {
  DCHECK(state_restorer_);
  state_restorer_->SetFramebufferBindingDirty();
  state_restorer_->SetTextureBindingDirty();

  // Select the Parameters for the texture object. Allocate the backing
  // GpuMemoryBuffer and GLImage, if one is going to be used.
  ColorBufferParameters parameters;
  GLuint image_id = 0;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager =
      Platform::Current()->GetGpuMemoryBufferManager();
  if (ShouldUseChromiumImage() && gpu_memory_buffer_manager) {
    parameters = GpuMemoryBufferColorBufferParameters();
    gfx::BufferFormat buffer_format;
    GLenum gl_format = GL_NONE;
    if (parameters.allocate_alpha_channel) {
      buffer_format = gfx::BufferFormat::RGBA_8888;
      gl_format = GL_RGBA;
    } else {
      buffer_format = gfx::BufferFormat::RGBX_8888;
      if (gpu::IsImageFromGpuMemoryBufferFormatSupported(
              gfx::BufferFormat::BGRX_8888,
              ContextProvider()->GetCapabilities()))
        buffer_format = gfx::BufferFormat::BGRX_8888;
      gl_format = GL_RGB;
    }
    gpu_memory_buffer = gpu_memory_buffer_manager->CreateGpuMemoryBuffer(
        gfx::Size(size), buffer_format, gfx::BufferUsage::SCANOUT,
        gpu::kNullSurfaceHandle);
    if (gpu_memory_buffer) {
      if (RuntimeEnabledFeatures::ColorCorrectRenderingEnabled())
        gpu_memory_buffer->SetColorSpaceForScanout(color_space_);
      image_id =
          gl_->CreateImageCHROMIUM(gpu_memory_buffer->AsClientBuffer(),
                                   size.Width(), size.Height(), gl_format);
      if (!image_id)
        gpu_memory_buffer.reset();
    }
  } else {
    parameters = TextureColorBufferParameters();
  }

  // Allocate the texture for this object.
  GLuint texture_id = 0;
  {
    gl_->GenTextures(1, &texture_id);
    gl_->BindTexture(parameters.target, texture_id);
    gl_->TexParameteri(parameters.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_->TexParameteri(parameters.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_->TexParameteri(parameters.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(parameters.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }

  // If this is GpuMemoryBuffer-backed, then bind the texture to the
  // GpuMemoryBuffer's GLImage. Otherwise, allocate ordinary texture storage.
  if (image_id) {
    gl_->BindTexImage2DCHROMIUM(parameters.target, image_id);
  } else {
    if (storage_texture_supported_) {
      GLenum internal_storage_format =
          parameters.allocate_alpha_channel ? GL_RGBA8 : GL_RGB8;
      gl_->TexStorage2DEXT(GL_TEXTURE_2D, 1, internal_storage_format,
                           size.Width(), size.Height());
    } else {
      GLenum gl_format = parameters.allocate_alpha_channel ? GL_RGBA : GL_RGB;
      gl_->TexImage2D(parameters.target, 0, gl_format, size.Width(),
                      size.Height(), 0, gl_format, GL_UNSIGNED_BYTE, 0);
    }
  }

  // Clear the alpha channel if this is RGB emulated.
  if (image_id && !want_alpha_channel_ &&
      ContextProvider()->GetCapabilities().chromium_image_rgb_emulation) {
    GLuint fbo = 0;

    state_restorer_->SetClearStateDirty();
    gl_->GenFramebuffers(1, &fbo);
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              parameters.target, texture_id, 0);
    gl_->ClearColor(0, 0, 0, 1);
    gl_->ColorMask(false, false, false, true);
    gl_->Clear(GL_COLOR_BUFFER_BIT);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              parameters.target, 0, 0);
    gl_->DeleteFramebuffers(1, &fbo);
  }

  return AdoptRef(new ColorBuffer(this, parameters, size, texture_id, image_id,
                                  std::move(gpu_memory_buffer)));
}

void DrawingBuffer::AttachColorBufferToReadFramebuffer() {
  DCHECK(state_restorer_);
  state_restorer_->SetFramebufferBindingDirty();
  state_restorer_->SetTextureBindingDirty();

  gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);

  GLenum target = back_color_buffer_->parameters.target;
  GLenum id = back_color_buffer_->texture_id;

  gl_->BindTexture(target, id);

  if (anti_aliasing_mode_ == kMSAAImplicitResolve)
    gl_->FramebufferTexture2DMultisampleEXT(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, id, 0, sample_count_);
  else
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, id,
                              0);
}

bool DrawingBuffer::WantExplicitResolve() {
  return anti_aliasing_mode_ == kMSAAExplicitResolve;
}

bool DrawingBuffer::WantDepthOrStencil() {
  return want_depth_ || want_stencil_;
}

GLenum DrawingBuffer::GetMultisampledRenderbufferFormat() {
  DCHECK(WantExplicitResolve());
  if (want_alpha_channel_)
    return GL_RGBA8_OES;
  if (ShouldUseChromiumImage() &&
      ContextProvider()->GetCapabilities().chromium_image_rgb_emulation)
    return GL_RGBA8_OES;
  if (ContextProvider()
          ->GetCapabilities()
          .disable_webgl_rgb_multisampling_usage)
    return GL_RGBA8_OES;
  return GL_RGB8_OES;
}

bool DrawingBuffer::SetupRGBEmulationForBlitFramebuffer() {
  // We only need to do this work if:
  //  - The user has selected alpha:false and antialias:false
  //  - We are using CHROMIUM_image with RGB emulation
  // macOS is the only platform on which this is necessary.

  if (want_alpha_channel_ || anti_aliasing_mode_ != kNone)
    return false;

  if (!(ShouldUseChromiumImage() &&
        ContextProvider()->GetCapabilities().chromium_image_rgb_emulation))
    return false;

  if (!back_color_buffer_)
    return false;

  // If for some reason the back buffer doesn't have a CHROMIUM_image,
  // don't proceed with this workaround.
  if (!back_color_buffer_->image_id)
    return false;

  // Before allowing the BlitFramebuffer call to go through, it's necessary
  // to swap out the RGBA texture that's bound to the CHROMIUM_image
  // instance with an RGB texture. BlitFramebuffer requires the internal
  // formats of the source and destination to match when doing a
  // multisample resolve, and the best way to achieve this without adding
  // more full-screen blits is to hook up a true RGB texture to the
  // underlying IOSurface. Unfortunately, on macOS, this rendering path
  // destroys the alpha channel and requires a fixup afterward, which is
  // why it isn't used all the time.

  GLuint rgb_texture = back_color_buffer_->rgb_workaround_texture_id;
  GLenum target = GC3D_TEXTURE_RECTANGLE_ARB;
  if (!rgb_texture) {
    gl_->GenTextures(1, &rgb_texture);
    gl_->BindTexture(target, rgb_texture);
    gl_->TexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_->TexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_->TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Bind this texture to the CHROMIUM_image instance that the color
    // buffer owns. This is an expensive operation, so it's important that
    // the result be cached.
    gl_->BindTexImage2DWithInternalformatCHROMIUM(target, GL_RGB,
                                                  back_color_buffer_->image_id);
    back_color_buffer_->rgb_workaround_texture_id = rgb_texture;
  }

  gl_->FramebufferTexture2D(GL_DRAW_FRAMEBUFFER_ANGLE, GL_COLOR_ATTACHMENT0,
                            target, rgb_texture, 0);
  return true;
}

void DrawingBuffer::CleanupRGBEmulationForBlitFramebuffer() {
  // This will only be called if SetupRGBEmulationForBlitFramebuffer was.
  // Put the framebuffer back the way it was, and clear the alpha channel.
  DCHECK(back_color_buffer_);
  DCHECK(back_color_buffer_->image_id);
  GLenum target = GC3D_TEXTURE_RECTANGLE_ARB;
  gl_->FramebufferTexture2D(GL_DRAW_FRAMEBUFFER_ANGLE, GL_COLOR_ATTACHMENT0,
                            target, back_color_buffer_->texture_id, 0);
  // Clear the alpha channel.
  gl_->ColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
  gl_->Disable(GL_SCISSOR_TEST);
  gl_->ClearColor(0, 0, 0, 1);
  gl_->Clear(GL_COLOR_BUFFER_BIT);
  DCHECK(client_);
  client_->DrawingBufferClientRestoreScissorTest();
  client_->DrawingBufferClientRestoreMaskAndClearValues();
}

DrawingBuffer::ScopedStateRestorer::ScopedStateRestorer(
    DrawingBuffer* drawing_buffer)
    : drawing_buffer_(drawing_buffer) {
  // If this is a nested restorer, save the previous restorer.
  previous_state_restorer_ = drawing_buffer->state_restorer_;
  drawing_buffer_->state_restorer_ = this;
}

DrawingBuffer::ScopedStateRestorer::~ScopedStateRestorer() {
  DCHECK_EQ(drawing_buffer_->state_restorer_, this);
  drawing_buffer_->state_restorer_ = previous_state_restorer_;
  Client* client = drawing_buffer_->client_;
  if (!client)
    return;

  if (clear_state_dirty_) {
    client->DrawingBufferClientRestoreScissorTest();
    client->DrawingBufferClientRestoreMaskAndClearValues();
  }
  if (pixel_pack_parameters_dirty_)
    client->DrawingBufferClientRestorePixelPackParameters();
  if (texture_binding_dirty_)
    client->DrawingBufferClientRestoreTexture2DBinding();
  if (renderbuffer_binding_dirty_)
    client->DrawingBufferClientRestoreRenderbufferBinding();
  if (framebuffer_binding_dirty_)
    client->DrawingBufferClientRestoreFramebufferBinding();
  if (pixel_unpack_buffer_binding_dirty_)
    client->DrawingBufferClientRestorePixelUnpackBufferBinding();
  if (pixel_pack_buffer_binding_dirty_)
    client->DrawingBufferClientRestorePixelPackBufferBinding();
}

bool DrawingBuffer::ShouldUseChromiumImage() {
  return RuntimeEnabledFeatures::WebGLImageChromiumEnabled() &&
         chromium_image_usage_ == kAllowChromiumImage;
}

}  // namespace blink
