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

#include "cc/resources/shared_bitmap.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/AcceleratedStaticBitmapImage.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebExternalBitmap.h"
#include "public/platform/WebExternalTextureLayer.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "wtf/CheckedNumeric.h"
#include "wtf/PtrUtil.h"
#include "wtf/typed_arrays/ArrayBufferContents.h"
#include <algorithm>
#include <memory>

namespace blink {

namespace {

const float s_resourceAdjustedRatio = 0.5;

class ScopedTextureUnit0BindingRestorer {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ScopedTextureUnit0BindingRestorer);

 public:
  ScopedTextureUnit0BindingRestorer(gpu::gles2::GLES2Interface* gl,
                                    GLenum activeTextureUnit,
                                    GLuint textureUnitZeroId)
      : m_gl(gl),
        m_oldActiveTextureUnit(activeTextureUnit),
        m_oldTextureUnitZeroId(textureUnitZeroId) {
    m_gl->ActiveTexture(GL_TEXTURE0);
  }
  ~ScopedTextureUnit0BindingRestorer() {
    m_gl->BindTexture(GL_TEXTURE_2D, m_oldTextureUnitZeroId);
    m_gl->ActiveTexture(m_oldActiveTextureUnit);
  }

 private:
  gpu::gles2::GLES2Interface* m_gl;
  GLenum m_oldActiveTextureUnit;
  GLuint m_oldTextureUnitZeroId;
};

static bool shouldFailDrawingBufferCreationForTesting = false;

}  // namespace

PassRefPtr<DrawingBuffer> DrawingBuffer::create(
    std::unique_ptr<WebGraphicsContext3DProvider> contextProvider,
    const IntSize& size,
    bool premultipliedAlpha,
    bool wantAlphaChannel,
    bool wantDepthBuffer,
    bool wantStencilBuffer,
    bool wantAntialiasing,
    PreserveDrawingBuffer preserve,
    WebGLVersion webGLVersion,
    ChromiumImageUsage chromiumImageUsage) {
  ASSERT(contextProvider);

  if (shouldFailDrawingBufferCreationForTesting) {
    shouldFailDrawingBufferCreationForTesting = false;
    return nullptr;
  }

  std::unique_ptr<Extensions3DUtil> extensionsUtil =
      Extensions3DUtil::create(contextProvider->contextGL());
  if (!extensionsUtil->isValid()) {
    // This might be the first time we notice that the GL context is lost.
    return nullptr;
  }
  ASSERT(extensionsUtil->supportsExtension("GL_OES_packed_depth_stencil"));
  extensionsUtil->ensureExtensionEnabled("GL_OES_packed_depth_stencil");
  bool multisampleSupported =
      wantAntialiasing && (extensionsUtil->supportsExtension(
                               "GL_CHROMIUM_framebuffer_multisample") ||
                           extensionsUtil->supportsExtension(
                               "GL_EXT_multisampled_render_to_texture")) &&
      extensionsUtil->supportsExtension("GL_OES_rgb8_rgba8");
  if (multisampleSupported) {
    extensionsUtil->ensureExtensionEnabled("GL_OES_rgb8_rgba8");
    if (extensionsUtil->supportsExtension(
            "GL_CHROMIUM_framebuffer_multisample"))
      extensionsUtil->ensureExtensionEnabled(
          "GL_CHROMIUM_framebuffer_multisample");
    else
      extensionsUtil->ensureExtensionEnabled(
          "GL_EXT_multisampled_render_to_texture");
  }
  bool discardFramebufferSupported =
      extensionsUtil->supportsExtension("GL_EXT_discard_framebuffer");
  if (discardFramebufferSupported)
    extensionsUtil->ensureExtensionEnabled("GL_EXT_discard_framebuffer");

  RefPtr<DrawingBuffer> drawingBuffer = adoptRef(new DrawingBuffer(
      std::move(contextProvider), std::move(extensionsUtil),
      discardFramebufferSupported, wantAlphaChannel, premultipliedAlpha,
      preserve, webGLVersion, wantDepthBuffer, wantStencilBuffer,
      chromiumImageUsage));
  if (!drawingBuffer->initialize(size, multisampleSupported)) {
    drawingBuffer->beginDestruction();
    return PassRefPtr<DrawingBuffer>();
  }
  return drawingBuffer.release();
}

void DrawingBuffer::forceNextDrawingBufferCreationToFail() {
  shouldFailDrawingBufferCreationForTesting = true;
}

DrawingBuffer::DrawingBuffer(
    std::unique_ptr<WebGraphicsContext3DProvider> contextProvider,
    std::unique_ptr<Extensions3DUtil> extensionsUtil,
    bool discardFramebufferSupported,
    bool wantAlphaChannel,
    bool premultipliedAlpha,
    PreserveDrawingBuffer preserve,
    WebGLVersion webGLVersion,
    bool wantDepth,
    bool wantStencil,
    ChromiumImageUsage chromiumImageUsage)
    : m_preserveDrawingBuffer(preserve),
      m_webGLVersion(webGLVersion),
      m_contextProvider(std::move(contextProvider)),
      m_gl(m_contextProvider->contextGL()),
      m_extensionsUtil(std::move(extensionsUtil)),
      m_discardFramebufferSupported(discardFramebufferSupported),
      m_wantAlphaChannel(wantAlphaChannel),
      m_premultipliedAlpha(premultipliedAlpha),
      m_softwareRendering(m_contextProvider->isSoftwareRendering()),
      m_wantDepth(wantDepth),
      m_wantStencil(wantStencil),
      m_chromiumImageUsage(chromiumImageUsage) {
  memset(m_colorMask, 0, 4 * sizeof(GLboolean));
  memset(m_clearColor, 0, 4 * sizeof(GLfloat));
  // Used by browser tests to detect the use of a DrawingBuffer.
  TRACE_EVENT_INSTANT0("test_gpu", "DrawingBufferCreation",
                       TRACE_EVENT_SCOPE_GLOBAL);
}

DrawingBuffer::~DrawingBuffer() {
  ASSERT(m_destructionInProgress);
  ASSERT(m_textureMailboxes.isEmpty());
  m_layer.reset();
  m_contextProvider.reset();
}

void DrawingBuffer::markContentsChanged() {
  m_contentsChanged = true;
  m_contentsChangeCommitted = false;
}

bool DrawingBuffer::bufferClearNeeded() const {
  return m_bufferClearNeeded;
}

void DrawingBuffer::setBufferClearNeeded(bool flag) {
  if (m_preserveDrawingBuffer == Discard) {
    m_bufferClearNeeded = flag;
  } else {
    ASSERT(!m_bufferClearNeeded);
  }
}

gpu::gles2::GLES2Interface* DrawingBuffer::contextGL() {
  return m_gl;
}

WebGraphicsContext3DProvider* DrawingBuffer::contextProvider() {
  return m_contextProvider.get();
}

void DrawingBuffer::setIsHidden(bool hidden) {
  if (m_isHidden == hidden)
    return;
  m_isHidden = hidden;
  if (m_isHidden)
    freeRecycledMailboxes();
}

void DrawingBuffer::setFilterQuality(SkFilterQuality filterQuality) {
  if (m_filterQuality != filterQuality) {
    m_filterQuality = filterQuality;
    if (m_layer)
      m_layer->setNearestNeighbor(filterQuality == kNone_SkFilterQuality);
  }
}

bool DrawingBuffer::requiresAlphaChannelToBePreserved() {
  return !m_drawFramebufferBinding &&
         defaultBufferRequiresAlphaChannelToBePreserved();
}

bool DrawingBuffer::defaultBufferRequiresAlphaChannelToBePreserved() {
  if (wantExplicitResolve()) {
    return !m_wantAlphaChannel &&
           getMultisampledRenderbufferFormat() == GL_RGBA8_OES;
  }

  bool rgbEmulation =
      contextProvider()->getCapabilities().emulate_rgb_buffer_with_rgba ||
      (shouldUseChromiumImage() &&
       contextProvider()->getCapabilities().chromium_image_rgb_emulation);
  return !m_wantAlphaChannel && rgbEmulation;
}

void DrawingBuffer::freeRecycledMailboxes() {
  while (!m_recycledMailboxQueue.isEmpty()) {
    RefPtr<RecycledMailbox> recycled = m_recycledMailboxQueue.takeLast();
    deleteMailbox(recycled->mailbox, recycled->syncToken);
  }
}

std::unique_ptr<cc::SharedBitmap> DrawingBuffer::createOrRecycleBitmap() {
  auto it = std::remove_if(
      m_recycledBitmaps.begin(), m_recycledBitmaps.end(),
      [this](const RecycledBitmap& bitmap) { return bitmap.size != m_size; });
  m_recycledBitmaps.shrink(it - m_recycledBitmaps.begin());

  if (!m_recycledBitmaps.isEmpty()) {
    RecycledBitmap recycled = std::move(m_recycledBitmaps.last());
    m_recycledBitmaps.removeLast();
    DCHECK(recycled.size == m_size);
    return std::move(recycled.bitmap);
  }

  return Platform::current()->allocateSharedBitmap(m_size);
}

bool DrawingBuffer::PrepareTextureMailbox(
    cc::TextureMailbox* outMailbox,
    std::unique_ptr<cc::SingleReleaseCallback>* outReleaseCallback) {
  bool forceGpuResult = false;
  return prepareTextureMailboxInternal(outMailbox, outReleaseCallback,
                                       forceGpuResult);
}

bool DrawingBuffer::prepareTextureMailboxInternal(
    cc::TextureMailbox* outMailbox,
    std::unique_ptr<cc::SingleReleaseCallback>* outReleaseCallback,
    bool forceGpuResult) {
  if (m_destructionInProgress) {
    // It can be hit in the following sequence.
    // 1. WebGL draws something.
    // 2. The compositor begins the frame.
    // 3. Javascript makes a context lost using WEBGL_lose_context extension.
    // 4. Here.
    return false;
  }
  ASSERT(!m_isHidden);
  if (!m_contentsChanged)
    return false;

  // If the context is lost, we don't know if we should be producing GPU or
  // software frames, until we get a new context, since the compositor will
  // be trying to get a new context and may change modes.
  if (m_gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    return false;

  TRACE_EVENT0("blink,rail", "DrawingBuffer::prepareMailbox");

  if (m_newMailboxCallback)
    (*m_newMailboxCallback)();

  // Resolve the multisampled buffer into m_colorBuffer texture.
  if (m_antiAliasingMode != None)
    commit();

  if (m_softwareRendering && !forceGpuResult) {
    std::unique_ptr<cc::SharedBitmap> bitmap = createOrRecycleBitmap();
    if (!bitmap)
      return false;
    unsigned char* pixels = bitmap->pixels();
    DCHECK(pixels);

    bool needPremultiply = m_wantAlphaChannel && !m_premultipliedAlpha;
    WebGLImageConversion::AlphaOp op =
        needPremultiply ? WebGLImageConversion::AlphaDoPremultiply
                        : WebGLImageConversion::AlphaDoNothing;
    readBackFramebuffer(pixels, size().width(), size().height(), ReadbackSkia,
                        op);

    *outMailbox = cc::TextureMailbox(bitmap.get(), m_size);

    // This holds a ref on the DrawingBuffer that will keep it alive until the
    // mailbox is released (and while the release callback is running). It also
    // owns the SharedBitmap.
    auto func = WTF::bind(&DrawingBuffer::softwareMailboxReleased,
                          RefPtr<DrawingBuffer>(this),
                          WTF::passed(std::move(bitmap)), m_size);
    *outReleaseCallback = cc::SingleReleaseCallback::Create(
        convertToBaseCallback(std::move(func)));
    return true;
  }

  if (m_webGLVersion > WebGL1) {
    m_gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  }

  // We must restore the texture binding since creating new textures,
  // consuming and producing mailboxes changes it.
  ScopedTextureUnit0BindingRestorer restorer(m_gl, m_activeTextureUnit,
                                             m_texture2DBinding);

  // First try to recycle an old buffer.
  RefPtr<MailboxInfo> mailboxInfo = takeRecycledMailbox();

  // No buffer available to recycle, create a new one.
  if (!mailboxInfo)
    mailboxInfo = createNewMailbox(createTextureAndAllocateMemory(m_size));

  if (m_preserveDrawingBuffer == Discard) {
    std::swap(mailboxInfo->textureInfo, m_colorBuffer);
    attachColorBufferToReadFramebuffer();

    if (m_discardFramebufferSupported) {
      // Explicitly discard framebuffer to save GPU memory bandwidth for tile-based GPU arch.
      const GLenum attachments[3] = {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT,
                                     GL_STENCIL_ATTACHMENT};
      m_gl->BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
      m_gl->DiscardFramebufferEXT(GL_FRAMEBUFFER, 3, attachments);
    }
  } else {
    m_gl->CopySubTextureCHROMIUM(
        m_colorBuffer.textureId, mailboxInfo->textureInfo.textureId, 0, 0, 0, 0,
        m_size.width(), m_size.height(), GL_FALSE, GL_FALSE, GL_FALSE);
  }

  restoreFramebufferBindings();
  restorePixelUnpackBufferBindings();
  m_contentsChanged = false;

  m_gl->ProduceTextureDirectCHROMIUM(mailboxInfo->textureInfo.textureId,
                                     mailboxInfo->textureInfo.parameters.target,
                                     mailboxInfo->mailbox.name);
  const GLuint64 fenceSync = m_gl->InsertFenceSyncCHROMIUM();
#if OS(MACOSX)
  m_gl->DescheduleUntilFinishedCHROMIUM();
#endif
  m_gl->Flush();
  gpu::SyncToken syncToken;
  m_gl->GenSyncTokenCHROMIUM(fenceSync, syncToken.GetData());

  bool isOverlayCandidate = mailboxInfo->textureInfo.imageId != 0;
  bool secureOutputOnly = false;
  *outMailbox = cc::TextureMailbox(mailboxInfo->mailbox, syncToken,
                                   mailboxInfo->textureInfo.parameters.target,
                                   gfx::Size(m_size.width(), m_size.height()),
                                   isOverlayCandidate, secureOutputOnly);

  // This holds a ref on the DrawingBuffer that will keep it alive until the
  // mailbox is released (and while the release callback is running).
  auto func = WTF::bind(&DrawingBuffer::gpuMailboxReleased,
                        RefPtr<DrawingBuffer>(this), mailboxInfo->mailbox);
  *outReleaseCallback =
      cc::SingleReleaseCallback::Create(convertToBaseCallback(std::move(func)));

  m_frontColorBuffer = {mailboxInfo->mailbox, syncToken,
                        mailboxInfo->textureInfo};
  setBufferClearNeeded(true);
  return true;
}

void DrawingBuffer::gpuMailboxReleased(const gpu::Mailbox& mailbox,
                                       const gpu::SyncToken& syncToken,
                                       bool lostResource) {
  if (m_destructionInProgress ||
      m_gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR || lostResource ||
      m_isHidden) {
    deleteMailbox(mailbox, syncToken);
    return;
  }

  for (size_t i = 0; i < m_textureMailboxes.size(); i++) {
    RefPtr<MailboxInfo> mailboxInfo = m_textureMailboxes[i];
    if (mailboxInfo->mailbox == mailbox) {
      m_recycledMailboxQueue.prepend(
          adoptRef(new RecycledMailbox(mailbox, syncToken)));
      return;
    }
  }
  ASSERT_NOT_REACHED();
}

void DrawingBuffer::softwareMailboxReleased(
    std::unique_ptr<cc::SharedBitmap> bitmap,
    const IntSize& size,
    const gpu::SyncToken& syncToken,
    bool lostResource) {
  DCHECK(!syncToken.HasData());  // No sync tokens for software resources.
  if (m_destructionInProgress || lostResource || m_isHidden || size != m_size)
    return;  // Just delete the bitmap.

  RecycledBitmap recycled = {std::move(bitmap), m_size};
  m_recycledBitmaps.append(std::move(recycled));
}

PassRefPtr<StaticBitmapImage> DrawingBuffer::transferToStaticBitmapImage() {
  // This can be null if the context is lost before the first call to grContext().
  GrContext* grContext = m_contextProvider->grContext();

  cc::TextureMailbox textureMailbox;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback;
  bool success = false;
  if (grContext) {
    bool forceGpuResult = true;
    success = prepareTextureMailboxInternal(&textureMailbox, &releaseCallback,
                                            forceGpuResult);
  }
  if (!success) {
    // If we can't get a mailbox, return an transparent black ImageBitmap.
    // The only situation this could happen is when two or more calls to transferToImageBitmap are made back-to-back, or when the context gets lost.
    sk_sp<SkSurface> surface =
        SkSurface::MakeRasterN32Premul(m_size.width(), m_size.height());
    return StaticBitmapImage::create(surface->makeImageSnapshot());
  }

  DCHECK_EQ(m_size.width(), textureMailbox.size_in_pixels().width());
  DCHECK_EQ(m_size.height(), textureMailbox.size_in_pixels().height());

  // Make our own textureId that is a reference on the same texture backing being used as the front
  // buffer (which was returned from PrepareTextureMailbox()).
  // We do not need to wait on the sync token in |textureMailbox| since the mailbox was produced on
  // the same |m_gl| context that we are using here. Similarly, the |releaseCallback| will run on
  // the same context so we don't need to send a sync token for this consume action back to it.
  // TODO(danakj): Instead of using PrepareTextureMailbox(), we could just use the actual texture id and
  // avoid needing to produce/consume a mailbox.
  GLuint textureId = m_gl->CreateAndConsumeTextureCHROMIUM(
      GL_TEXTURE_2D, textureMailbox.name());
  // Return the mailbox but report that the resource is lost to prevent trying to use
  // the backing for future frames. We keep it alive with our own reference to the
  // backing via our |textureId|.
  releaseCallback->Run(gpu::SyncToken(), true /* lostResource */);

  // Store that texture id as the backing for an SkImage.
  GrGLTextureInfo textureInfo;
  textureInfo.fTarget = GL_TEXTURE_2D;
  textureInfo.fID = textureId;
  GrBackendTextureDesc backendTexture;
  backendTexture.fOrigin = kBottomLeft_GrSurfaceOrigin;
  backendTexture.fWidth = m_size.width();
  backendTexture.fHeight = m_size.height();
  backendTexture.fConfig = kSkia8888_GrPixelConfig;
  backendTexture.fTextureHandle =
      skia::GrGLTextureInfoToGrBackendObject(textureInfo);
  sk_sp<SkImage> skImage =
      SkImage::MakeFromAdoptedTexture(grContext, backendTexture);

  // We reuse the same mailbox name from above since our texture id was consumed from it.
  const auto& skImageMailbox = textureMailbox.mailbox();
  // Use the sync token generated after producing the mailbox. Waiting for this before trying to use
  // the mailbox with some other context will ensure it is valid. We wouldn't need to wait for the
  // consume done in this function because the texture id it generated would only be valid for the
  // DrawingBuffer's context anyways.
  const auto& skImageSyncToken = textureMailbox.sync_token();

  // TODO(xidachen): Create a small pool of recycled textures from ImageBitmapRenderingContext's
  // transferFromImageBitmap, and try to use them in DrawingBuffer.
  return AcceleratedStaticBitmapImage::createFromWebGLContextImage(
      std::move(skImage), skImageMailbox, skImageSyncToken);
}

DrawingBuffer::TextureParameters
DrawingBuffer::chromiumImageTextureParameters() {
#if OS(MACOSX)
  // A CHROMIUM_image backed texture requires a specialized set of parameters
  // on OSX.
  TextureParameters parameters;
  parameters.target = GC3D_TEXTURE_RECTANGLE_ARB;

  if (m_wantAlphaChannel) {
    parameters.creationInternalColorFormat = GL_RGBA;
    parameters.internalColorFormat = GL_RGBA;
  } else if (contextProvider()
                 ->getCapabilities()
                 .chromium_image_rgb_emulation) {
    parameters.creationInternalColorFormat = GL_RGB;
    parameters.internalColorFormat = GL_RGBA;
  } else {
    GLenum format =
        defaultBufferRequiresAlphaChannelToBePreserved() ? GL_RGBA : GL_RGB;
    parameters.creationInternalColorFormat = format;
    parameters.internalColorFormat = format;
  }

  // Unused when CHROMIUM_image is being used.
  parameters.colorFormat = 0;
  return parameters;
#else
  return defaultTextureParameters();
#endif
}

DrawingBuffer::TextureParameters DrawingBuffer::defaultTextureParameters() {
  TextureParameters parameters;
  parameters.target = GL_TEXTURE_2D;
  if (m_wantAlphaChannel) {
    parameters.internalColorFormat = GL_RGBA;
    parameters.creationInternalColorFormat = GL_RGBA;
    parameters.colorFormat = GL_RGBA;
  } else if (contextProvider()
                 ->getCapabilities()
                 .emulate_rgb_buffer_with_rgba) {
    parameters.internalColorFormat = GL_RGBA;
    parameters.creationInternalColorFormat = GL_RGBA;
    parameters.colorFormat = GL_RGBA;
  } else {
    GLenum format =
        defaultBufferRequiresAlphaChannelToBePreserved() ? GL_RGBA : GL_RGB;
    parameters.creationInternalColorFormat = format;
    parameters.internalColorFormat = format;
    parameters.colorFormat = format;
  }
  return parameters;
}

PassRefPtr<DrawingBuffer::MailboxInfo> DrawingBuffer::takeRecycledMailbox() {
  if (m_recycledMailboxQueue.isEmpty())
    return nullptr;

  // Creation of image backed mailboxes is very expensive, so be less
  // aggressive about pruning them.
  size_t cacheLimit = 1;
  if (shouldUseChromiumImage())
    cacheLimit = 4;

  RefPtr<RecycledMailbox> recycled;
  while (m_recycledMailboxQueue.size() > cacheLimit) {
    recycled = m_recycledMailboxQueue.takeLast();
    deleteMailbox(recycled->mailbox, recycled->syncToken);
  }
  recycled = m_recycledMailboxQueue.takeLast();

  if (recycled->syncToken.HasData())
    m_gl->WaitSyncTokenCHROMIUM(recycled->syncToken.GetData());

  RefPtr<MailboxInfo> mailboxInfo;
  for (size_t i = 0; i < m_textureMailboxes.size(); i++) {
    if (m_textureMailboxes[i]->mailbox == recycled->mailbox) {
      mailboxInfo = m_textureMailboxes[i];
      break;
    }
  }
  ASSERT(mailboxInfo);

  if (mailboxInfo->size != m_size) {
    resizeTextureMemory(&mailboxInfo->textureInfo, m_size);
    mailboxInfo->size = m_size;
  }

  return mailboxInfo.release();
}

PassRefPtr<DrawingBuffer::MailboxInfo> DrawingBuffer::createNewMailbox(
    const TextureInfo& info) {
  RefPtr<MailboxInfo> returnMailbox = adoptRef(new MailboxInfo);
  m_gl->GenMailboxCHROMIUM(returnMailbox->mailbox.name);
  returnMailbox->textureInfo = info;
  returnMailbox->size = m_size;
  m_textureMailboxes.append(returnMailbox);
  return returnMailbox.release();
}

void DrawingBuffer::deleteMailbox(const gpu::Mailbox& mailbox,
                                  const gpu::SyncToken& syncToken) {
  if (syncToken.HasData())
    m_gl->WaitSyncTokenCHROMIUM(syncToken.GetConstData());
  for (size_t i = 0; i < m_textureMailboxes.size(); i++) {
    if (m_textureMailboxes[i]->mailbox == mailbox) {
      deleteChromiumImageForTexture(&m_textureMailboxes[i]->textureInfo);
      m_gl->DeleteTextures(1, &m_textureMailboxes[i]->textureInfo.textureId);
      m_textureMailboxes.remove(i);
      return;
    }
  }
  ASSERT_NOT_REACHED();
}

bool DrawingBuffer::initialize(const IntSize& size, bool useMultisampling) {
  if (m_gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    // Need to try to restore the context again later.
    return false;
  }

  m_gl->GetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);

  int maxSampleCount = 0;
  m_antiAliasingMode = None;
  if (useMultisampling) {
    m_gl->GetIntegerv(GL_MAX_SAMPLES_ANGLE, &maxSampleCount);
    m_antiAliasingMode = MSAAExplicitResolve;
    if (m_extensionsUtil->supportsExtension(
            "GL_EXT_multisampled_render_to_texture")) {
      m_antiAliasingMode = MSAAImplicitResolve;
    } else if (m_extensionsUtil->supportsExtension(
                   "GL_CHROMIUM_screen_space_antialiasing")) {
      m_antiAliasingMode = ScreenSpaceAntialiasing;
    }
  }
  // TODO(dshwang): enable storage texture on all platform. crbug.com/557848
  // Linux ATI bot fails WebglConformance.conformance_textures_misc_tex_image_webgl
  // So use storage texture only if ScreenSpaceAntialiasing is enabled,
  // because ScreenSpaceAntialiasing is much faster with storage texture.
  m_storageTextureSupported =
      (m_webGLVersion > WebGL1 ||
       m_extensionsUtil->supportsExtension("GL_EXT_texture_storage")) &&
      m_antiAliasingMode == ScreenSpaceAntialiasing;
  m_sampleCount = std::min(4, maxSampleCount);

  m_gl->GenFramebuffers(1, &m_fbo);
  m_gl->BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
  if (wantExplicitResolve()) {
    m_gl->GenFramebuffers(1, &m_multisampleFBO);
    m_gl->BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
    m_gl->GenRenderbuffers(1, &m_multisampleRenderbuffer);
  }
  if (!reset(size))
    return false;

  if (m_depthStencilBuffer) {
    DCHECK(wantDepthOrStencil());
    m_hasImplicitStencilBuffer = !m_wantStencil;
  }

  if (m_gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    // It's possible that the drawing buffer allocation provokes a context loss, so check again just in case. http://crbug.com/512302
    return false;
  }

  return true;
}

bool DrawingBuffer::copyToPlatformTexture(gpu::gles2::GLES2Interface* gl,
                                          GLuint texture,
                                          GLenum internalFormat,
                                          GLenum destType,
                                          GLint level,
                                          bool premultiplyAlpha,
                                          bool flipY,
                                          SourceDrawingBuffer sourceBuffer) {
  if (m_contentsChanged) {
    if (m_antiAliasingMode != None) {
      commit();
      restoreFramebufferBindings();
    }
    m_gl->Flush();
  }

  // Assume that the destination target is GL_TEXTURE_2D.
  if (!Extensions3DUtil::canUseCopyTextureCHROMIUM(
          GL_TEXTURE_2D, internalFormat, destType, level))
    return false;

  // Contexts may be in a different share group. We must transfer the texture through a mailbox first
  GLint textureId = 0;
  GLenum target = 0;
  gpu::Mailbox mailbox;
  gpu::SyncToken produceSyncToken;
  if (sourceBuffer == FrontBuffer && m_frontColorBuffer.texInfo.textureId) {
    textureId = m_frontColorBuffer.texInfo.textureId;
    target = m_frontColorBuffer.texInfo.parameters.target;
    mailbox = m_frontColorBuffer.mailbox;
    produceSyncToken = m_frontColorBuffer.produceSyncToken;
  } else {
    textureId = m_colorBuffer.textureId;
    target = m_colorBuffer.parameters.target;
    m_gl->GenMailboxCHROMIUM(mailbox.name);
    m_gl->ProduceTextureDirectCHROMIUM(textureId, target, mailbox.name);
    const GLuint64 fenceSync = m_gl->InsertFenceSyncCHROMIUM();
    m_gl->Flush();
    m_gl->GenSyncTokenCHROMIUM(fenceSync, produceSyncToken.GetData());
  }

  DCHECK(produceSyncToken.HasData());
  gl->WaitSyncTokenCHROMIUM(produceSyncToken.GetConstData());
  GLuint sourceTexture =
      gl->CreateAndConsumeTextureCHROMIUM(target, mailbox.name);

  GLboolean unpackPremultiplyAlphaNeeded = GL_FALSE;
  GLboolean unpackUnpremultiplyAlphaNeeded = GL_FALSE;
  if (m_wantAlphaChannel && m_premultipliedAlpha && !premultiplyAlpha)
    unpackUnpremultiplyAlphaNeeded = GL_TRUE;
  else if (m_wantAlphaChannel && !m_premultipliedAlpha && premultiplyAlpha)
    unpackPremultiplyAlphaNeeded = GL_TRUE;

  gl->CopyTextureCHROMIUM(sourceTexture, texture, internalFormat, destType,
                          flipY, unpackPremultiplyAlphaNeeded,
                          unpackUnpremultiplyAlphaNeeded);

  gl->DeleteTextures(1, &sourceTexture);

  const GLuint64 fenceSync = gl->InsertFenceSyncCHROMIUM();

  gl->Flush();
  gpu::SyncToken syncToken;
  gl->GenSyncTokenCHROMIUM(fenceSync, syncToken.GetData());
  m_gl->WaitSyncTokenCHROMIUM(syncToken.GetData());

  return true;
}

GLuint DrawingBuffer::framebuffer() const {
  return m_fbo;
}

WebLayer* DrawingBuffer::platformLayer() {
  if (!m_layer) {
    m_layer = wrapUnique(
        Platform::current()->compositorSupport()->createExternalTextureLayer(
            this));

    m_layer->setOpaque(!m_wantAlphaChannel);
    m_layer->setBlendBackgroundColor(m_wantAlphaChannel);
    m_layer->setPremultipliedAlpha(m_premultipliedAlpha);
    m_layer->setNearestNeighbor(m_filterQuality == kNone_SkFilterQuality);
    GraphicsLayer::registerContentsLayer(m_layer->layer());
  }

  return m_layer->layer();
}

void DrawingBuffer::clearPlatformLayer() {
  if (m_layer)
    m_layer->clearTexture();

  m_gl->Flush();
}

void DrawingBuffer::beginDestruction() {
  ASSERT(!m_destructionInProgress);
  m_destructionInProgress = true;

  clearPlatformLayer();
  freeRecycledMailboxes();

  if (m_multisampleFBO)
    m_gl->DeleteFramebuffers(1, &m_multisampleFBO);

  if (m_fbo)
    m_gl->DeleteFramebuffers(1, &m_fbo);

  if (m_multisampleRenderbuffer)
    m_gl->DeleteRenderbuffers(1, &m_multisampleRenderbuffer);

  if (m_depthStencilBuffer)
    m_gl->DeleteRenderbuffers(1, &m_depthStencilBuffer);

  if (m_colorBuffer.textureId) {
    deleteChromiumImageForTexture(&m_colorBuffer);
    m_gl->DeleteTextures(1, &m_colorBuffer.textureId);
  }

  setSize(IntSize());

  m_colorBuffer = TextureInfo();
  m_frontColorBuffer = FrontBufferInfo();
  m_multisampleRenderbuffer = 0;
  m_depthStencilBuffer = 0;
  m_multisampleFBO = 0;
  m_fbo = 0;

  if (m_layer)
    GraphicsLayer::unregisterContentsLayer(m_layer->layer());
}

GLuint DrawingBuffer::createColorTexture(const TextureParameters& parameters) {
  GLuint offscreenColorTexture;
  m_gl->GenTextures(1, &offscreenColorTexture);
  m_gl->BindTexture(parameters.target, offscreenColorTexture);
  m_gl->TexParameteri(parameters.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  m_gl->TexParameteri(parameters.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  m_gl->TexParameteri(parameters.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  m_gl->TexParameteri(parameters.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return offscreenColorTexture;
}

bool DrawingBuffer::resizeMultisampleFramebuffer(const IntSize& size) {
  DCHECK(wantExplicitResolve());
  m_gl->BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
  m_gl->BindRenderbuffer(GL_RENDERBUFFER, m_multisampleRenderbuffer);
  m_gl->RenderbufferStorageMultisampleCHROMIUM(
      GL_RENDERBUFFER, m_sampleCount, getMultisampledRenderbufferFormat(),
      size.width(), size.height());

  if (m_gl->GetError() == GL_OUT_OF_MEMORY)
    return false;

  m_gl->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_RENDERBUFFER, m_multisampleRenderbuffer);

  return true;
}

void DrawingBuffer::resizeDepthStencil(const IntSize& size) {
  m_gl->BindFramebuffer(GL_FRAMEBUFFER,
                        m_multisampleFBO ? m_multisampleFBO : m_fbo);
  if (!m_depthStencilBuffer)
    m_gl->GenRenderbuffers(1, &m_depthStencilBuffer);
  m_gl->BindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
  if (m_antiAliasingMode == MSAAImplicitResolve)
    m_gl->RenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, m_sampleCount,
                                            GL_DEPTH24_STENCIL8_OES,
                                            size.width(), size.height());
  else if (m_antiAliasingMode == MSAAExplicitResolve)
    m_gl->RenderbufferStorageMultisampleCHROMIUM(GL_RENDERBUFFER, m_sampleCount,
                                                 GL_DEPTH24_STENCIL8_OES,
                                                 size.width(), size.height());
  else
    m_gl->RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES,
                              size.width(), size.height());
  // For ES 2.0 contexts DEPTH_STENCIL is not available natively, so we emulate it
  // at the command buffer level for WebGL contexts.
  m_gl->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, m_depthStencilBuffer);
  m_gl->BindRenderbuffer(GL_RENDERBUFFER, 0);
}

bool DrawingBuffer::resizeDefaultFramebuffer(const IntSize& size) {
  // Resize or create m_colorBuffer.
  if (m_colorBuffer.textureId) {
    resizeTextureMemory(&m_colorBuffer, size);
  } else {
    m_colorBuffer = createTextureAndAllocateMemory(size);
  }

  attachColorBufferToReadFramebuffer();

  if (wantExplicitResolve()) {
    if (!resizeMultisampleFramebuffer(size))
      return false;
  }

  if (wantDepthOrStencil())
    resizeDepthStencil(size);

  if (wantExplicitResolve()) {
    m_gl->BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
    if (m_gl->CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      return false;
  }

  m_gl->BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
  return m_gl->CheckFramebufferStatus(GL_FRAMEBUFFER) ==
         GL_FRAMEBUFFER_COMPLETE;
}

void DrawingBuffer::clearFramebuffers(GLbitfield clearMask) {
  // We will clear the multisample FBO, but we also need to clear the non-multisampled buffer.
  if (m_multisampleFBO) {
    m_gl->BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    m_gl->Clear(GL_COLOR_BUFFER_BIT);
  }

  m_gl->BindFramebuffer(GL_FRAMEBUFFER,
                        m_multisampleFBO ? m_multisampleFBO : m_fbo);
  m_gl->Clear(clearMask);
}

void DrawingBuffer::setSize(const IntSize& size) {
  if (m_size == size)
    return;

  m_size = size;
}

IntSize DrawingBuffer::adjustSize(const IntSize& desiredSize,
                                  const IntSize& curSize,
                                  int maxTextureSize) {
  IntSize adjustedSize = desiredSize;

  // Clamp if the desired size is greater than the maximum texture size for the device.
  if (adjustedSize.height() > maxTextureSize)
    adjustedSize.setHeight(maxTextureSize);

  if (adjustedSize.width() > maxTextureSize)
    adjustedSize.setWidth(maxTextureSize);

  return adjustedSize;
}

bool DrawingBuffer::reset(const IntSize& newSize) {
  CHECK(!newSize.isEmpty());
  IntSize adjustedSize = adjustSize(newSize, m_size, m_maxTextureSize);
  if (adjustedSize.isEmpty())
    return false;

  if (adjustedSize != m_size) {
    do {
      if (!resizeDefaultFramebuffer(adjustedSize)) {
        adjustedSize.scale(s_resourceAdjustedRatio);
        continue;
      }
      break;
    } while (!adjustedSize.isEmpty());

    setSize(adjustedSize);

    if (adjustedSize.isEmpty())
      return false;
  }

  m_gl->Disable(GL_SCISSOR_TEST);
  m_gl->ClearColor(0, 0, 0,
                   defaultBufferRequiresAlphaChannelToBePreserved() ? 1 : 0);
  m_gl->ColorMask(true, true, true, true);

  GLbitfield clearMask = GL_COLOR_BUFFER_BIT;
  if (!!m_depthStencilBuffer) {
    m_gl->ClearDepthf(1.0f);
    clearMask |= GL_DEPTH_BUFFER_BIT;
    m_gl->DepthMask(true);
  }
  if (!!m_depthStencilBuffer) {
    m_gl->ClearStencil(0);
    clearMask |= GL_STENCIL_BUFFER_BIT;
    m_gl->StencilMaskSeparate(GL_FRONT, 0xFFFFFFFF);
  }

  clearFramebuffers(clearMask);
  return true;
}

void DrawingBuffer::commit() {
  if (wantExplicitResolve() && !m_contentsChangeCommitted) {
    m_gl->BindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, m_multisampleFBO);
    m_gl->BindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, m_fbo);

    if (m_scissorEnabled)
      m_gl->Disable(GL_SCISSOR_TEST);

    int width = m_size.width();
    int height = m_size.height();
    // Use NEAREST, because there is no scale performed during the blit.
    GLuint filter = GL_NEAREST;

    m_gl->BlitFramebufferCHROMIUM(0, 0, width, height, 0, 0, width, height,
                                  GL_COLOR_BUFFER_BIT, filter);

    // On old AMD GPUs on OS X, glColorMask doesn't work correctly for
    // multisampled renderbuffers and the alpha channel can be overwritten.
    // Clear the alpha channel of |m_fbo|.
    if (defaultBufferRequiresAlphaChannelToBePreserved() &&
        contextProvider()
            ->getCapabilities()
            .disable_multisampling_color_mask_usage) {
      m_gl->ClearColor(0, 0, 0, 1);
      m_gl->ColorMask(false, false, false, true);
      m_gl->Clear(GL_COLOR_BUFFER_BIT);

      m_gl->ClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2],
                       m_clearColor[3]);
      m_gl->ColorMask(m_colorMask[0], m_colorMask[1], m_colorMask[2],
                      m_colorMask[3]);
    }

    if (m_scissorEnabled)
      m_gl->Enable(GL_SCISSOR_TEST);
  }

  m_gl->BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
  if (m_antiAliasingMode == ScreenSpaceAntialiasing) {
    m_gl->ApplyScreenSpaceAntialiasingCHROMIUM();
  }
  m_contentsChangeCommitted = true;
}

void DrawingBuffer::restorePixelUnpackBufferBindings() {
  if (m_webGLVersion > WebGL1) {
    m_gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pixelUnpackBufferBinding);
  }
}

void DrawingBuffer::restoreFramebufferBindings() {
  if (m_drawFramebufferBinding && m_readFramebufferBinding) {
    if (m_drawFramebufferBinding == m_readFramebufferBinding) {
      m_gl->BindFramebuffer(GL_FRAMEBUFFER, m_readFramebufferBinding);
    } else {
      m_gl->BindFramebuffer(GL_READ_FRAMEBUFFER, m_readFramebufferBinding);
      m_gl->BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_drawFramebufferBinding);
    }
    return;
  }
  if (!m_drawFramebufferBinding && !m_readFramebufferBinding) {
    bind(GL_FRAMEBUFFER);
    return;
  }
  if (!m_drawFramebufferBinding) {
    bind(GL_DRAW_FRAMEBUFFER);
    m_gl->BindFramebuffer(GL_READ_FRAMEBUFFER, m_readFramebufferBinding);
  } else {
    bind(GL_READ_FRAMEBUFFER);
    m_gl->BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_drawFramebufferBinding);
  }
}

bool DrawingBuffer::multisample() const {
  return m_antiAliasingMode != None;
}

void DrawingBuffer::bind(GLenum target) {
  m_gl->BindFramebuffer(target,
                        wantExplicitResolve() ? m_multisampleFBO : m_fbo);
}

void DrawingBuffer::setPackAlignment(GLint param) {
  m_packAlignment = param;
}

bool DrawingBuffer::paintRenderingResultsToImageData(
    int& width,
    int& height,
    SourceDrawingBuffer sourceBuffer,
    WTF::ArrayBufferContents& contents) {
  ASSERT(!m_premultipliedAlpha);
  width = size().width();
  height = size().height();

  CheckedNumeric<int> dataSize = 4;
  dataSize *= width;
  dataSize *= height;
  if (!dataSize.IsValid())
    return false;

  WTF::ArrayBufferContents pixels(width * height, 4,
                                  WTF::ArrayBufferContents::NotShared,
                                  WTF::ArrayBufferContents::DontInitialize);

  GLuint fbo = 0;
  if (sourceBuffer == FrontBuffer && m_frontColorBuffer.texInfo.textureId) {
    m_gl->GenFramebuffers(1, &fbo);
    m_gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    m_gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               m_frontColorBuffer.texInfo.parameters.target,
                               m_frontColorBuffer.texInfo.textureId, 0);
  } else {
    m_gl->BindFramebuffer(GL_FRAMEBUFFER, framebuffer());
  }

  readBackFramebuffer(static_cast<unsigned char*>(pixels.data()), width, height,
                      ReadbackRGBA, WebGLImageConversion::AlphaDoNothing);
  flipVertically(static_cast<uint8_t*>(pixels.data()), width, height);

  if (fbo) {
    m_gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               m_frontColorBuffer.texInfo.parameters.target, 0,
                               0);
    m_gl->DeleteFramebuffers(1, &fbo);
  }

  restoreFramebufferBindings();

  pixels.transfer(contents);
  return true;
}

void DrawingBuffer::readBackFramebuffer(unsigned char* pixels,
                                        int width,
                                        int height,
                                        ReadbackOrder readbackOrder,
                                        WebGLImageConversion::AlphaOp op) {
  if (m_packAlignment > 4)
    m_gl->PixelStorei(GL_PACK_ALIGNMENT, 1);
  m_gl->ReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  if (m_packAlignment > 4)
    m_gl->PixelStorei(GL_PACK_ALIGNMENT, m_packAlignment);

  size_t bufferSize = 4 * width * height;

  if (readbackOrder == ReadbackSkia) {
#if (SK_R32_SHIFT == 16) && !SK_B32_SHIFT
    // Swizzle red and blue channels to match SkBitmap's byte ordering.
    // TODO(kbr): expose GL_BGRA as extension.
    for (size_t i = 0; i < bufferSize; i += 4) {
      std::swap(pixels[i], pixels[i + 2]);
    }
#endif
  }

  if (op == WebGLImageConversion::AlphaDoPremultiply) {
    for (size_t i = 0; i < bufferSize; i += 4) {
      pixels[i + 0] = std::min(255, pixels[i + 0] * pixels[i + 3] / 255);
      pixels[i + 1] = std::min(255, pixels[i + 1] * pixels[i + 3] / 255);
      pixels[i + 2] = std::min(255, pixels[i + 2] * pixels[i + 3] / 255);
    }
  } else if (op != WebGLImageConversion::AlphaDoNothing) {
    ASSERT_NOT_REACHED();
  }
}

void DrawingBuffer::flipVertically(uint8_t* framebuffer,
                                   int width,
                                   int height) {
  m_scanline.resize(width * 4);
  uint8_t* scanline = &m_scanline[0];
  unsigned rowBytes = width * 4;
  unsigned count = height / 2;
  for (unsigned i = 0; i < count; i++) {
    uint8_t* rowA = framebuffer + i * rowBytes;
    uint8_t* rowB = framebuffer + (height - i - 1) * rowBytes;
    memcpy(scanline, rowB, rowBytes);
    memcpy(rowB, rowA, rowBytes);
    memcpy(rowA, scanline, rowBytes);
  }
}

void DrawingBuffer::allocateConditionallyImmutableTexture(GLenum target,
                                                          GLenum internalformat,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLint border,
                                                          GLenum format,
                                                          GLenum type) {
  if (m_storageTextureSupported) {
    GLenum internalStorageFormat = GL_NONE;
    if (internalformat == GL_RGB) {
      internalStorageFormat = GL_RGB8;
    } else if (internalformat == GL_RGBA) {
      internalStorageFormat = GL_RGBA8;
    } else {
      NOTREACHED();
    }
    m_gl->TexStorage2DEXT(GL_TEXTURE_2D, 1, internalStorageFormat, width,
                          height);
    return;
  }
  m_gl->TexImage2D(target, 0, internalformat, width, height, border, format,
                   type, 0);
}

void DrawingBuffer::deleteChromiumImageForTexture(TextureInfo* info) {
  if (info->imageId) {
    m_gl->BindTexture(info->parameters.target, info->textureId);
    m_gl->ReleaseTexImage2DCHROMIUM(info->parameters.target, info->imageId);
    m_gl->DestroyImageCHROMIUM(info->imageId);
    info->imageId = 0;
  }
}

void DrawingBuffer::clearChromiumImageAlpha(const TextureInfo& info) {
  if (m_wantAlphaChannel)
    return;
  if (!contextProvider()->getCapabilities().chromium_image_rgb_emulation)
    return;

  GLuint fbo = 0;
  m_gl->GenFramebuffers(1, &fbo);
  m_gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);
  m_gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             info.parameters.target, info.textureId, 0);
  m_gl->ClearColor(0, 0, 0, 1);
  m_gl->ColorMask(false, false, false, true);
  m_gl->Clear(GL_COLOR_BUFFER_BIT);
  m_gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             info.parameters.target, 0, 0);
  m_gl->DeleteFramebuffers(1, &fbo);
  restoreFramebufferBindings();
  m_gl->ClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2],
                   m_clearColor[3]);
  m_gl->ColorMask(m_colorMask[0], m_colorMask[1], m_colorMask[2],
                  m_colorMask[3]);
}

DrawingBuffer::TextureInfo DrawingBuffer::createTextureAndAllocateMemory(
    const IntSize& size) {
  if (!shouldUseChromiumImage())
    return createDefaultTextureAndAllocateMemory(size);

  TextureParameters parameters = chromiumImageTextureParameters();
  GLuint imageId = m_gl->CreateGpuMemoryBufferImageCHROMIUM(
      size.width(), size.height(), parameters.creationInternalColorFormat,
      GC3D_SCANOUT_CHROMIUM);
  GLuint textureId = createColorTexture(parameters);
  if (imageId) {
    m_gl->BindTexImage2DCHROMIUM(parameters.target, imageId);
  }

  TextureInfo info;
  info.textureId = textureId;
  info.imageId = imageId;
  info.parameters = parameters;
  clearChromiumImageAlpha(info);
  return info;
}

DrawingBuffer::TextureInfo DrawingBuffer::createDefaultTextureAndAllocateMemory(
    const IntSize& size) {
  DrawingBuffer::TextureInfo info;
  TextureParameters parameters = defaultTextureParameters();
  info.parameters = parameters;
  info.textureId = createColorTexture(parameters);
  allocateConditionallyImmutableTexture(
      parameters.target, parameters.creationInternalColorFormat, size.width(),
      size.height(), 0, parameters.colorFormat, GL_UNSIGNED_BYTE);
  info.immutable = m_storageTextureSupported;
  return info;
}

void DrawingBuffer::resizeTextureMemory(TextureInfo* info,
                                        const IntSize& size) {
  ASSERT(info->textureId);
  if (!shouldUseChromiumImage()) {
    if (info->immutable) {
      DCHECK(m_storageTextureSupported);
      m_gl->DeleteTextures(1, &info->textureId);
      info->textureId = createColorTexture(info->parameters);
    }
    m_gl->BindTexture(info->parameters.target, info->textureId);
    allocateConditionallyImmutableTexture(
        info->parameters.target, info->parameters.creationInternalColorFormat,
        size.width(), size.height(), 0, info->parameters.colorFormat,
        GL_UNSIGNED_BYTE);
    info->immutable = m_storageTextureSupported;
    return;
  }

  DCHECK(!info->immutable);
  deleteChromiumImageForTexture(info);
  info->imageId = m_gl->CreateGpuMemoryBufferImageCHROMIUM(
      size.width(), size.height(), info->parameters.creationInternalColorFormat,
      GC3D_SCANOUT_CHROMIUM);
  if (info->imageId) {
    m_gl->BindTexture(info->parameters.target, info->textureId);
    m_gl->BindTexImage2DCHROMIUM(info->parameters.target, info->imageId);
    clearChromiumImageAlpha(*info);
  } else {
    // At this point, the texture still exists, but has no allocated
    // storage. This is intentional, and mimics the behavior of a texImage2D
    // failure.
  }
}

void DrawingBuffer::attachColorBufferToReadFramebuffer() {
  m_gl->BindFramebuffer(GL_FRAMEBUFFER, m_fbo);

  GLenum target = m_colorBuffer.parameters.target;
  GLenum id = m_colorBuffer.textureId;

  m_gl->BindTexture(target, id);

  if (m_antiAliasingMode == MSAAImplicitResolve)
    m_gl->FramebufferTexture2DMultisampleEXT(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, id, 0, m_sampleCount);
  else
    m_gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, id,
                               0);

  restoreTextureBindings();
  restoreFramebufferBindings();
}

bool DrawingBuffer::wantExplicitResolve() {
  return m_antiAliasingMode == MSAAExplicitResolve;
}

bool DrawingBuffer::wantDepthOrStencil() {
  return m_wantDepth || m_wantStencil;
}

GLenum DrawingBuffer::getMultisampledRenderbufferFormat() {
  DCHECK(wantExplicitResolve());
  if (m_wantAlphaChannel)
    return GL_RGBA8_OES;
  if (shouldUseChromiumImage() &&
      contextProvider()->getCapabilities().chromium_image_rgb_emulation)
    return GL_RGBA8_OES;
  if (contextProvider()
          ->getCapabilities()
          .disable_webgl_rgb_multisampling_usage)
    return GL_RGBA8_OES;
  return GL_RGB8_OES;
}

void DrawingBuffer::restoreTextureBindings() {
  // This class potentially modifies the bindings for GL_TEXTURE_2D and
  // GL_TEXTURE_RECTANGLE. Only GL_TEXTURE_2D needs to be restored since
  // the public interface for WebGL does not support GL_TEXTURE_RECTANGLE.
  m_gl->BindTexture(GL_TEXTURE_2D, m_texture2DBinding);
}

bool DrawingBuffer::shouldUseChromiumImage() {
  return RuntimeEnabledFeatures::webGLImageChromiumEnabled() &&
         m_chromiumImageUsage == AllowChromiumImage;
}

}  // namespace blink
