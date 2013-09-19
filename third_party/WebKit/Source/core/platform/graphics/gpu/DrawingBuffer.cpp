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

#include "config.h"

#include "core/platform/graphics/gpu/DrawingBuffer.h"

#include <algorithm>
#include "core/platform/chromium/TraceEvent.h"
#include "core/platform/graphics/Extensions3D.h"
#include "core/platform/graphics/GraphicsContext3D.h"
#include "core/platform/graphics/GraphicsLayer.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebExternalBitmap.h"
#include "public/platform/WebExternalTextureLayer.h"
#include "public/platform/WebGraphicsContext3D.h"

using namespace std;

namespace WebCore {

// Global resource ceiling (expressed in terms of pixels) for DrawingBuffer creation and resize.
// When this limit is set, DrawingBuffer::create() and DrawingBuffer::reset() calls that would
// exceed the global cap will instead clear the buffer.
static const int s_maximumResourceUsePixels = 16 * 1024 * 1024;
static int s_currentResourceUsePixels = 0;
static const float s_resourceAdjustedRatio = 0.5;

static const bool s_allowContextEvictionOnCreate = true;
static const int s_maxScaleAttempts = 3;

class ScopedTextureUnit0BindingRestorer {
public:
    ScopedTextureUnit0BindingRestorer(GraphicsContext3D* context, GC3Denum activeTextureUnit, Platform3DObject textureUnitZeroId)
        : m_context(context)
        , m_oldActiveTextureUnit(activeTextureUnit)
        , m_oldTextureUnitZeroId(textureUnitZeroId)
    {
        m_context->activeTexture(GraphicsContext3D::TEXTURE0);
    }
    ~ScopedTextureUnit0BindingRestorer()
    {
        m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_oldTextureUnitZeroId);
        m_context->activeTexture(m_oldActiveTextureUnit);
    }

private:
    GraphicsContext3D* m_context;
    GC3Denum m_oldActiveTextureUnit;
    Platform3DObject m_oldTextureUnitZeroId;
};

PassRefPtr<DrawingBuffer> DrawingBuffer::create(GraphicsContext3D* context, const IntSize& size, PreserveDrawingBuffer preserve, PassRefPtr<ContextEvictionManager> contextEvictionManager)
{
    Extensions3D* extensions = context->extensions();
    bool multisampleSupported = extensions->supports("GL_ANGLE_framebuffer_blit")
        && extensions->supports("GL_ANGLE_framebuffer_multisample")
        && extensions->supports("GL_OES_rgb8_rgba8");
    if (multisampleSupported) {
        extensions->ensureEnabled("GL_ANGLE_framebuffer_blit");
        extensions->ensureEnabled("GL_ANGLE_framebuffer_multisample");
        extensions->ensureEnabled("GL_OES_rgb8_rgba8");
    }
    bool packedDepthStencilSupported = extensions->supports("GL_OES_packed_depth_stencil");
    if (packedDepthStencilSupported)
        extensions->ensureEnabled("GL_OES_packed_depth_stencil");

    RefPtr<DrawingBuffer> drawingBuffer = adoptRef(new DrawingBuffer(context, size, multisampleSupported, packedDepthStencilSupported, preserve, contextEvictionManager));
    return drawingBuffer.release();
}

DrawingBuffer::DrawingBuffer(GraphicsContext3D* context,
                             const IntSize& size,
                             bool multisampleExtensionSupported,
                             bool packedDepthStencilExtensionSupported,
                             PreserveDrawingBuffer preserve,
                             PassRefPtr<ContextEvictionManager> contextEvictionManager)
    : m_preserveDrawingBuffer(preserve)
    , m_scissorEnabled(false)
    , m_texture2DBinding(0)
    , m_framebufferBinding(0)
    , m_activeTextureUnit(GraphicsContext3D::TEXTURE0)
    , m_context(context)
    , m_size(-1, -1)
    , m_multisampleExtensionSupported(multisampleExtensionSupported)
    , m_packedDepthStencilExtensionSupported(packedDepthStencilExtensionSupported)
    , m_fbo(0)
    , m_colorBuffer(0)
    , m_frontColorBuffer(0)
    , m_depthStencilBuffer(0)
    , m_depthBuffer(0)
    , m_stencilBuffer(0)
    , m_multisampleFBO(0)
    , m_multisampleColorBuffer(0)
    , m_contentsChanged(true)
    , m_internalColorFormat(0)
    , m_colorFormat(0)
    , m_internalRenderbufferFormat(0)
    , m_maxTextureSize(0)
    , m_contextEvictionManager(contextEvictionManager)
{
    // Used by browser tests to detect the use of a DrawingBuffer.
    TRACE_EVENT_INSTANT0("test_gpu", "DrawingBufferCreation");
    initialize(size);
}

DrawingBuffer::~DrawingBuffer()
{
    releaseResources();
}

WebKit::WebGraphicsContext3D* DrawingBuffer::context()
{
    if (!m_context)
        return 0;
    return m_context->webContext();
}

bool DrawingBuffer::prepareMailbox(WebKit::WebExternalTextureMailbox* outMailbox, WebKit::WebExternalBitmap* bitmap)
{
    if (!m_context || !m_contentsChanged || !m_lastColorBuffer)
        return false;

    m_context->makeContextCurrent();

    // Resolve the multisampled buffer into the texture referenced by m_lastColorBuffer mailbox.
    if (multisample())
        commit();

    if (bitmap) {
        bitmap->setSize(size());

        unsigned char* pixels = bitmap->pixels();
        bool needPremultiply = m_attributes.alpha && !m_attributes.premultipliedAlpha;
        GraphicsContext3D::AlphaOp op = needPremultiply ? GraphicsContext3D::AlphaDoPremultiply : GraphicsContext3D::AlphaDoNothing;
        if (pixels)
            m_context->readBackFramebuffer(pixels, size().width(), size().height(), GraphicsContext3D::ReadbackSkia, op);
    }

    // We must restore the texture binding since creating new textures,
    // consuming and producing mailboxes changes it.
    ScopedTextureUnit0BindingRestorer restorer(m_context.get(), m_activeTextureUnit, m_texture2DBinding);

    // First try to recycle an old buffer.
    RefPtr<MailboxInfo> nextFrontColorBuffer = recycledMailbox();

    // No buffer available to recycle, create a new one.
    if (!nextFrontColorBuffer) {
        unsigned newColorBuffer = createColorTexture(m_size);
        // Bad things happened, abandon ship.
        if (!newColorBuffer)
            return false;

        nextFrontColorBuffer = createNewMailbox(newColorBuffer);
    }

    if (m_preserveDrawingBuffer == Discard) {
        m_colorBuffer = nextFrontColorBuffer->textureId;
        swap(nextFrontColorBuffer, m_lastColorBuffer);
        // It appears safe to overwrite the context's framebuffer binding in the Discard case since there will always be a
        // WebGLRenderingContext::clearIfComposited() call made before the next draw call which restores the framebuffer binding.
        // If this stops being true at some point, we should track the current framebuffer binding in the DrawingBuffer and restore
        // it after attaching the new back buffer here.
        m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
        m_context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, m_colorBuffer, 0);
    } else {
        Extensions3D* extensions = m_context->extensions();
        extensions->copyTextureCHROMIUM(GraphicsContext3D::TEXTURE_2D, m_colorBuffer, nextFrontColorBuffer->textureId, 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE);
    }

    if (multisample() && !m_framebufferBinding)
        bind();
    else
        restoreFramebufferBinding();

    m_contentsChanged = false;

    context()->bindTexture(GraphicsContext3D::TEXTURE_2D, nextFrontColorBuffer->textureId);
    context()->produceTextureCHROMIUM(GraphicsContext3D::TEXTURE_2D, nextFrontColorBuffer->mailbox.name);
    context()->flush();
    m_context->markLayerComposited();

    *outMailbox = nextFrontColorBuffer->mailbox;
    m_frontColorBuffer = nextFrontColorBuffer->textureId;
    return true;
}

void DrawingBuffer::mailboxReleased(const WebKit::WebExternalTextureMailbox& mailbox)
{
    for (size_t i = 0; i < m_textureMailboxes.size(); i++) {
         RefPtr<MailboxInfo> mailboxInfo = m_textureMailboxes[i];
         if (!memcmp(mailboxInfo->mailbox.name, mailbox.name, sizeof(mailbox.name))) {
             mailboxInfo->mailbox.syncPoint = mailbox.syncPoint;
             m_recycledMailboxes.append(mailboxInfo.release());
             return;
         }
     }
     ASSERT_NOT_REACHED();
}

PassRefPtr<DrawingBuffer::MailboxInfo> DrawingBuffer::recycledMailbox()
{
    if (!m_context || m_recycledMailboxes.isEmpty())
        return PassRefPtr<MailboxInfo>();

    RefPtr<MailboxInfo> mailboxInfo = m_recycledMailboxes.last().release();
    m_recycledMailboxes.removeLast();

    if (mailboxInfo->mailbox.syncPoint) {
        context()->waitSyncPoint(mailboxInfo->mailbox.syncPoint);
        mailboxInfo->mailbox.syncPoint = 0;
    }

    context()->bindTexture(GraphicsContext3D::TEXTURE_2D, mailboxInfo->textureId);
    context()->consumeTextureCHROMIUM(GraphicsContext3D::TEXTURE_2D, mailboxInfo->mailbox.name);

    if (mailboxInfo->size != m_size) {
        m_context->texImage2DResourceSafe(GraphicsContext3D::TEXTURE_2D, 0, m_internalColorFormat, m_size.width(), m_size.height(), 0, m_colorFormat, GraphicsContext3D::UNSIGNED_BYTE);
        mailboxInfo->size = m_size;
    }

    return mailboxInfo.release();
}

PassRefPtr<DrawingBuffer::MailboxInfo> DrawingBuffer::createNewMailbox(unsigned textureId)
{
    RefPtr<MailboxInfo> returnMailbox = adoptRef(new MailboxInfo());
    context()->genMailboxCHROMIUM(returnMailbox->mailbox.name);
    returnMailbox->textureId = textureId;
    returnMailbox->size = m_size;
    m_textureMailboxes.append(returnMailbox);
    return returnMailbox.release();
}

void DrawingBuffer::initialize(const IntSize& size)
{
    ASSERT(m_context);
    m_attributes = m_context->getContextAttributes();

    if (m_attributes.alpha) {
        m_internalColorFormat = GraphicsContext3D::RGBA;
        m_colorFormat = GraphicsContext3D::RGBA;
        m_internalRenderbufferFormat = Extensions3D::RGBA8_OES;
    } else {
        m_internalColorFormat = GraphicsContext3D::RGB;
        m_colorFormat = GraphicsContext3D::RGB;
        m_internalRenderbufferFormat = Extensions3D::RGB8_OES;
    }

    m_context->getIntegerv(GraphicsContext3D::MAX_TEXTURE_SIZE, &m_maxTextureSize);

    m_fbo = m_context->createFramebuffer();

    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
    m_colorBuffer = createColorTexture();
    m_context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, m_colorBuffer, 0);
    createSecondaryBuffers();
    reset(size);
    m_lastColorBuffer = createNewMailbox(m_colorBuffer);
}

unsigned DrawingBuffer::frontColorBuffer() const
{
    return m_colorBuffer;
}

Platform3DObject DrawingBuffer::framebuffer() const
{
    return m_fbo;
}

WebKit::WebLayer* DrawingBuffer::platformLayer()
{
    if (!m_context)
        return 0;

    if (!m_layer) {
        m_layer = adoptPtr(WebKit::Platform::current()->compositorSupport()->createExternalTextureLayer(this));

        m_layer->setOpaque(!m_attributes.alpha);
        m_layer->setBlendBackgroundColor(m_attributes.alpha);
        m_layer->setPremultipliedAlpha(m_attributes.premultipliedAlpha);
        GraphicsLayer::registerContentsLayer(m_layer->layer());
    }

    return m_layer->layer();
}

void DrawingBuffer::paintCompositedResultsToCanvas(ImageBuffer* imageBuffer)
{
    if (!m_context || !m_context->makeContextCurrent() || m_context->extensions()->getGraphicsResetStatusARB() != GraphicsContext3D::NO_ERROR)
        return;

    Extensions3D* extensions = m_context->extensions();
    // Since the m_frontColorBuffer was produced and sent to the compositor, it cannot be bound to an fbo.
    // We have to make a copy of it here and bind that copy instead.
    // FIXME: That's not true any more, provided we don't change texture
    // parameters.
    unsigned sourceTexture = createColorTexture(m_size);
    extensions->copyTextureCHROMIUM(GraphicsContext3D::TEXTURE_2D, m_frontColorBuffer, sourceTexture, 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE);

    // Since we're using the same context as WebGL, we have to restore any state we change (in this case, just the framebuffer binding).
    // FIXME: The WebGLRenderingContext tracks the current framebuffer binding, it would be slightly more efficient to use this value
    // rather than querying it off of the context.
    GC3Dint previousFramebuffer = 0;
    m_context->getIntegerv(GraphicsContext3D::FRAMEBUFFER_BINDING, &previousFramebuffer);

    Platform3DObject framebuffer = m_context->createFramebuffer();
    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, framebuffer);
    m_context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, sourceTexture, 0);

    extensions->paintFramebufferToCanvas(framebuffer, size().width(), size().height(), !m_attributes.premultipliedAlpha, imageBuffer);
    m_context->deleteFramebuffer(framebuffer);
    m_context->deleteTexture(sourceTexture);

    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, previousFramebuffer);
}

void DrawingBuffer::clearPlatformLayer()
{
    if (m_layer)
        m_layer->clearTexture();

    if (m_context)
        m_context->flush();
}

void DrawingBuffer::releaseResources()
{
    if (m_context) {
        m_context->makeContextCurrent();

        clearPlatformLayer();

        for (size_t i = 0; i < m_textureMailboxes.size(); i++)
            m_context->deleteTexture(m_textureMailboxes[i]->textureId);

        if (m_multisampleColorBuffer)
            m_context->deleteRenderbuffer(m_multisampleColorBuffer);

        if (m_depthStencilBuffer)
            m_context->deleteRenderbuffer(m_depthStencilBuffer);

        if (m_depthBuffer)
            m_context->deleteRenderbuffer(m_depthBuffer);

        if (m_stencilBuffer)
            m_context->deleteRenderbuffer(m_stencilBuffer);

        if (m_multisampleFBO)
            m_context->deleteFramebuffer(m_multisampleFBO);

        if (m_fbo)
            m_context->deleteFramebuffer(m_fbo);

        m_context.clear();
    }

    setSize(IntSize());

    m_colorBuffer = 0;
    m_frontColorBuffer = 0;
    m_multisampleColorBuffer = 0;
    m_depthStencilBuffer = 0;
    m_depthBuffer = 0;
    m_stencilBuffer = 0;
    m_multisampleFBO = 0;
    m_fbo = 0;
    m_contextEvictionManager.clear();

    m_lastColorBuffer.clear();
    m_recycledMailboxes.clear();
    m_textureMailboxes.clear();

    if (m_layer) {
        GraphicsLayer::unregisterContentsLayer(m_layer->layer());
        m_layer.clear();
    }
}

unsigned DrawingBuffer::createColorTexture(const IntSize& size)
{
    if (!m_context)
        return 0;

    unsigned offscreenColorTexture = m_context->createTexture();
    if (!offscreenColorTexture)
        return 0;

    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, offscreenColorTexture);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
    if (!size.isEmpty())
        m_context->texImage2DResourceSafe(GraphicsContext3D::TEXTURE_2D, 0, m_internalColorFormat, size.width(), size.height(), 0, m_colorFormat, GraphicsContext3D::UNSIGNED_BYTE);

    return offscreenColorTexture;
}

void DrawingBuffer::createSecondaryBuffers()
{
    // create a multisample FBO
    if (multisample()) {
        m_multisampleFBO = m_context->createFramebuffer();
        m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO);
        m_multisampleColorBuffer = m_context->createRenderbuffer();
    }
}

bool DrawingBuffer::resizeFramebuffer(const IntSize& size)
{
    // resize regular FBO
    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);

    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_colorBuffer);
    m_context->texImage2DResourceSafe(GraphicsContext3D::TEXTURE_2D, 0, m_internalColorFormat, size.width(), size.height(), 0, m_colorFormat, GraphicsContext3D::UNSIGNED_BYTE);
    if (m_lastColorBuffer)
        m_lastColorBuffer->size = m_size;

    m_context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, m_colorBuffer, 0);

    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, 0);

    if (!multisample())
        resizeDepthStencil(size, 0);
    if (m_context->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER) != GraphicsContext3D::FRAMEBUFFER_COMPLETE)
        return false;

    return true;
}

bool DrawingBuffer::resizeMultisampleFramebuffer(const IntSize& size)
{
    if (multisample()) {
        int maxSampleCount = 0;

        m_context->getIntegerv(Extensions3D::MAX_SAMPLES, &maxSampleCount);
        int sampleCount = std::min(4, maxSampleCount);

        m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO);

        m_context->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_multisampleColorBuffer);
        m_context->extensions()->renderbufferStorageMultisample(GraphicsContext3D::RENDERBUFFER, sampleCount, m_internalRenderbufferFormat, size.width(), size.height());

        if (m_context->getError() == GraphicsContext3D::OUT_OF_MEMORY)
            return false;

        m_context->framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::RENDERBUFFER, m_multisampleColorBuffer);
        resizeDepthStencil(size, sampleCount);
        if (m_context->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER) != GraphicsContext3D::FRAMEBUFFER_COMPLETE)
            return false;
    }

    return true;
}

void DrawingBuffer::resizeDepthStencil(const IntSize& size, int sampleCount)
{
    if (m_attributes.depth && m_attributes.stencil && m_packedDepthStencilExtensionSupported) {
        if (!m_depthStencilBuffer)
            m_depthStencilBuffer = m_context->createRenderbuffer();
        m_context->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_depthStencilBuffer);
        if (multisample())
            m_context->extensions()->renderbufferStorageMultisample(GraphicsContext3D::RENDERBUFFER, sampleCount, Extensions3D::DEPTH24_STENCIL8, size.width(), size.height());
        else
            m_context->renderbufferStorage(GraphicsContext3D::RENDERBUFFER, Extensions3D::DEPTH24_STENCIL8, size.width(), size.height());
        m_context->framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::STENCIL_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_depthStencilBuffer);
        m_context->framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::DEPTH_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_depthStencilBuffer);
    } else {
        if (m_attributes.depth) {
            if (!m_depthBuffer)
                m_depthBuffer = m_context->createRenderbuffer();
            m_context->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_depthBuffer);
            if (multisample())
                m_context->extensions()->renderbufferStorageMultisample(GraphicsContext3D::RENDERBUFFER, sampleCount, GraphicsContext3D::DEPTH_COMPONENT16, size.width(), size.height());
            else
                m_context->renderbufferStorage(GraphicsContext3D::RENDERBUFFER, GraphicsContext3D::DEPTH_COMPONENT16, size.width(), size.height());
            m_context->framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::DEPTH_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_depthBuffer);
        }
        if (m_attributes.stencil) {
            if (!m_stencilBuffer)
                m_stencilBuffer = m_context->createRenderbuffer();
            m_context->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_stencilBuffer);
            if (multisample())
                m_context->extensions()->renderbufferStorageMultisample(GraphicsContext3D::RENDERBUFFER, sampleCount, GraphicsContext3D::STENCIL_INDEX8, size.width(), size.height());
            else
                m_context->renderbufferStorage(GraphicsContext3D::RENDERBUFFER, GraphicsContext3D::STENCIL_INDEX8, size.width(), size.height());
            m_context->framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::STENCIL_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_stencilBuffer);
        }
    }
    m_context->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, 0);
}



void DrawingBuffer::clearFramebuffers(GC3Dbitfield clearMask)
{
    if (!m_context)
        return;

    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO ? m_multisampleFBO : m_fbo);

    m_context->clear(clearMask);

    // The multisample fbo was just cleared, but we also need to clear the non-multisampled buffer too.
    if (m_multisampleFBO) {
        m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
        m_context->clear(GraphicsContext3D::COLOR_BUFFER_BIT);
        m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO);
    }
}

void DrawingBuffer::setSize(const IntSize& size) {
    if (m_size == size)
        return;

    s_currentResourceUsePixels += pixelDelta(size);
    m_size = size;
}

int DrawingBuffer::pixelDelta(const IntSize& size) {
    return (max(0, size.width()) * max(0, size.height())) - (max(0, m_size.width()) * max(0, m_size.height()));
}

IntSize DrawingBuffer::adjustSize(const IntSize& size) {
    IntSize adjustedSize = size;

    // Clamp if the desired size is greater than the maximum texture size for the device.
    if (adjustedSize.height() > m_maxTextureSize)
        adjustedSize.setHeight(m_maxTextureSize);

    if (adjustedSize.width() > m_maxTextureSize)
        adjustedSize.setWidth(m_maxTextureSize);

    // Try progressively smaller sizes until we find a size that fits or reach a scale limit.
    int scaleAttempts = 0;
    while ((s_currentResourceUsePixels + pixelDelta(adjustedSize)) > s_maximumResourceUsePixels) {
        scaleAttempts++;
        if (scaleAttempts > s_maxScaleAttempts)
            return IntSize();

        adjustedSize.scale(s_resourceAdjustedRatio);

        if (adjustedSize.isEmpty())
            return IntSize();
    }

    return adjustedSize;
}

IntSize DrawingBuffer::adjustSizeWithContextEviction(const IntSize& size, bool& evictContext) {
    IntSize adjustedSize = adjustSize(size);
    if (!adjustedSize.isEmpty()) {
        evictContext = false;
        return adjustedSize; // Buffer fits without evicting a context.
    }

    // Speculatively adjust the pixel budget to see if the buffer would fit should the oldest context be evicted.
    IntSize oldestSize = m_contextEvictionManager->oldestContextSize();
    int pixelDelta = oldestSize.width() * oldestSize.height();

    s_currentResourceUsePixels -= pixelDelta;
    adjustedSize = adjustSize(size);
    s_currentResourceUsePixels += pixelDelta;

    evictContext = !adjustedSize.isEmpty();
    return adjustedSize;
}

void DrawingBuffer::reset(const IntSize& newSize)
{
    if (!m_context)
        return;

    IntSize adjustedSize;
    bool evictContext = false;
    bool isNewContext = m_size.isEmpty();
    if (s_allowContextEvictionOnCreate && isNewContext)
        adjustedSize = adjustSizeWithContextEviction(newSize, evictContext);
    else
        adjustedSize = adjustSize(newSize);

    if (adjustedSize.isEmpty())
        return;

    if (evictContext)
        m_contextEvictionManager->forciblyLoseOldestContext("WARNING: WebGL contexts have exceeded the maximum allowed backbuffer area. Oldest context will be lost.");

    if (adjustedSize != m_size) {
        do {
            // resize multisample FBO
            if (!resizeMultisampleFramebuffer(adjustedSize) || !resizeFramebuffer(adjustedSize)) {
                adjustedSize.scale(s_resourceAdjustedRatio);
                continue;
            }
            break;
        } while (!adjustedSize.isEmpty());

        setSize(adjustedSize);

        if (adjustedSize.isEmpty())
            return;
    }

    m_context->disable(GraphicsContext3D::SCISSOR_TEST);
    m_context->clearColor(0, 0, 0, 0);
    m_context->colorMask(true, true, true, true);

    GC3Dbitfield clearMask = GraphicsContext3D::COLOR_BUFFER_BIT;
    if (m_attributes.depth) {
        m_context->clearDepth(1.0f);
        clearMask |= GraphicsContext3D::DEPTH_BUFFER_BIT;
        m_context->depthMask(true);
    }
    if (m_attributes.stencil) {
        m_context->clearStencil(0);
        clearMask |= GraphicsContext3D::STENCIL_BUFFER_BIT;
        m_context->stencilMaskSeparate(GraphicsContext3D::FRONT, 0xFFFFFFFF);
    }

    clearFramebuffers(clearMask);
}

void DrawingBuffer::commit(long x, long y, long width, long height)
{
    if (!m_context)
        return;

    if (width < 0)
        width = m_size.width();
    if (height < 0)
        height = m_size.height();

    m_context->makeContextCurrent();

    if (m_multisampleFBO) {
        m_context->bindFramebuffer(Extensions3D::READ_FRAMEBUFFER, m_multisampleFBO);
        m_context->bindFramebuffer(Extensions3D::DRAW_FRAMEBUFFER, m_fbo);

        if (m_scissorEnabled)
            m_context->disable(GraphicsContext3D::SCISSOR_TEST);

        // Use NEAREST, because there is no scale performed during the blit.
        m_context->extensions()->blitFramebuffer(x, y, width, height, x, y, width, height, GraphicsContext3D::COLOR_BUFFER_BIT, GraphicsContext3D::NEAREST);

        if (m_scissorEnabled)
            m_context->enable(GraphicsContext3D::SCISSOR_TEST);
    }

    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
}

void DrawingBuffer::restoreFramebufferBinding()
{
    if (!m_context || !m_framebufferBinding)
        return;

    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_framebufferBinding);
}

bool DrawingBuffer::multisample() const
{
    return m_attributes.antialias && m_multisampleExtensionSupported;
}

void DrawingBuffer::bind()
{
    if (!m_context)
        return;

    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO ? m_multisampleFBO : m_fbo);
}

} // namespace WebCore
