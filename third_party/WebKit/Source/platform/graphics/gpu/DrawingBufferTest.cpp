/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "platform/graphics/test/MockWebGraphicsContext3D.h"
#include "public/platform/Platform.h"
#include "public/platform/WebExternalTextureMailbox.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefPtr.h"

using testing::Test;
using testing::_;

namespace blink {

namespace {

// The target to use when binding a texture to a Chromium image.
WGC3Denum imageTextureTarget()
{
#if OS(MACOSX)
    return GC3D_TEXTURE_RECTANGLE_ARB;
#else
    return GL_TEXTURE_2D;
#endif
}

// The target to use when preparing a mailbox texture.
WGC3Denum drawingBufferTextureTarget(bool allowImageChromium)
{
    if (RuntimeEnabledFeatures::webGLImageChromiumEnabled() && allowImageChromium)
        return imageTextureTarget();
    return GL_TEXTURE_2D;
}

} // namespace

class WebGraphicsContext3DForTests : public MockWebGraphicsContext3D {
public:
    WebGraphicsContext3DForTests()
        : MockWebGraphicsContext3D()
        , m_boundTexture(0)
        , m_boundTextureTarget(0)
        , m_currentMailboxByte(0)
        , m_mostRecentlyWaitedSyncToken(0)
        , m_currentImageId(1)
        , m_allowImageChromium(true)
    {
    }

    void bindTexture(WGC3Denum target, WebGLId texture) override
    {
        if (target != m_boundTextureTarget && texture == 0)
            return;

        // For simplicity, only allow one target to ever be bound.
        ASSERT_TRUE(m_boundTextureTarget == 0 || target == m_boundTextureTarget);
        m_boundTextureTarget = target;
        m_boundTexture = texture;
    }

    void texImage2D(WGC3Denum target, WGC3Dint level, WGC3Denum internalformat, WGC3Dsizei width, WGC3Dsizei height, WGC3Dint border, WGC3Denum format, WGC3Denum type, const void* pixels) override
    {
        if (target == GL_TEXTURE_2D && !level) {
            m_textureSizes.set(m_boundTexture, IntSize(width, height));
        }
    }

    void genMailboxCHROMIUM(WGC3Dbyte* mailbox) override
    {
        ++m_currentMailboxByte;
        WebExternalTextureMailbox temp;
        memset(mailbox, m_currentMailboxByte, sizeof(temp.name));
    }

    void produceTextureDirectCHROMIUM(WebGLId texture, WGC3Denum target, const WGC3Dbyte* mailbox) override
    {
        ASSERT_EQ(target, drawingBufferTextureTarget(m_allowImageChromium));
        ASSERT_TRUE(m_textureSizes.contains(texture));
        m_mostRecentlyProducedSize = m_textureSizes.get(texture);
    }

    IntSize mostRecentlyProducedSize()
    {
        return m_mostRecentlyProducedSize;
    }

    WGC3Duint64 insertFenceSyncCHROMIUM() override
    {
        static WGC3Duint64 syncPointGenerator = 0;
        return ++syncPointGenerator;
    }

    bool genSyncTokenCHROMIUM(WGC3Duint64 fenceSync, WGC3Dbyte* syncToken) override
    {
        memcpy(syncToken, &fenceSync, sizeof(fenceSync));
        return true;
    }

    void waitSyncTokenCHROMIUM(const WGC3Dbyte* syncToken) override
    {
        memcpy(&m_mostRecentlyWaitedSyncToken, syncToken, sizeof(m_mostRecentlyWaitedSyncToken));
    }

    WGC3Duint createGpuMemoryBufferImageCHROMIUM(WGC3Dsizei width, WGC3Dsizei height, WGC3Denum internalformat, WGC3Denum usage) override
    {
        if (!m_allowImageChromium)
            return false;
        m_imageSizes.set(m_currentImageId, IntSize(width, height));
        return m_currentImageId++;
    }

    MOCK_METHOD1(destroyImageMock, void(WGC3Duint imageId));
    void destroyImageCHROMIUM(WGC3Duint imageId)
    {
        m_imageSizes.remove(imageId);
        // No textures should be bound to this.
        ASSERT(m_imageToTextureMap.find(imageId) == m_imageToTextureMap.end());
        m_imageSizes.remove(imageId);
        destroyImageMock(imageId);
    }

    MOCK_METHOD1(bindTexImage2DMock, void(WGC3Dint imageId));
    void bindTexImage2DCHROMIUM(WGC3Denum target, WGC3Dint imageId)
    {
        if (target == imageTextureTarget()) {
            m_textureSizes.set(m_boundTexture, m_imageSizes.find(imageId)->value);
            m_imageToTextureMap.set(imageId, m_boundTexture);
            bindTexImage2DMock(imageId);
        }
    }

    MOCK_METHOD1(releaseTexImage2DMock, void(WGC3Dint imageId));
    void releaseTexImage2DCHROMIUM(WGC3Denum target, WGC3Dint imageId)
    {
        if (target == imageTextureTarget()) {
            m_imageSizes.set(m_currentImageId, IntSize());
            m_imageToTextureMap.remove(imageId);
            releaseTexImage2DMock(imageId);
        }
    }

    WGC3Duint mostRecentlyWaitedSyncToken()
    {
        return m_mostRecentlyWaitedSyncToken;
    }

    WGC3Duint nextImageIdToBeCreated()
    {
        return m_currentImageId;
    }

    void setAllowImageChromium(bool allow)
    {
        m_allowImageChromium = allow;
    }

private:
    WebGLId m_boundTexture;
    WGC3Denum m_boundTextureTarget;
    HashMap<WebGLId, IntSize> m_textureSizes;
    WGC3Dbyte m_currentMailboxByte;
    IntSize m_mostRecentlyProducedSize;
    WGC3Duint m_mostRecentlyWaitedSyncToken;
    WGC3Duint m_currentImageId;
    HashMap<WGC3Duint, IntSize> m_imageSizes;
    HashMap<WGC3Duint, WebGLId> m_imageToTextureMap;
    bool m_allowImageChromium;
};

static const int initialWidth = 100;
static const int initialHeight = 100;
static const int alternateHeight = 50;

class DrawingBufferForTests : public DrawingBuffer {
public:
    static PassRefPtr<DrawingBufferForTests> create(PassOwnPtr<WebGraphicsContext3D> context,
        const IntSize& size, PreserveDrawingBuffer preserve)
    {
        OwnPtr<Extensions3DUtil> extensionsUtil = Extensions3DUtil::create(context.get());
        RefPtr<DrawingBufferForTests> drawingBuffer =
            adoptRef(new DrawingBufferForTests(context, extensionsUtil.release(), preserve));
        if (!drawingBuffer->initialize(size)) {
            drawingBuffer->beginDestruction();
            return PassRefPtr<DrawingBufferForTests>();
        }
        return drawingBuffer.release();
    }

    DrawingBufferForTests(PassOwnPtr<WebGraphicsContext3D> context,
        PassOwnPtr<Extensions3DUtil> extensionsUtil,
        PreserveDrawingBuffer preserve)
        : DrawingBuffer(context, extensionsUtil, DrawingBuffer::SupportedExtensions(), preserve, WebGraphicsContext3D::Attributes())
        , m_live(0)
    { }

    ~DrawingBufferForTests() override
    {
        if (m_live)
            *m_live = false;
    }

    bool* m_live;
};

class DrawingBufferTest : public Test {
protected:
    void SetUp() override
    {
        OwnPtr<WebGraphicsContext3DForTests> context = adoptPtr(new WebGraphicsContext3DForTests);
        m_context = context.get();
        m_drawingBuffer = DrawingBufferForTests::create(context.release(),
            IntSize(initialWidth, initialHeight), DrawingBuffer::Preserve);
    }

    WebGraphicsContext3DForTests* webContext()
    {
        return m_context;
    }

    WebGraphicsContext3DForTests* m_context;
    RefPtr<DrawingBufferForTests> m_drawingBuffer;
};

TEST_F(DrawingBufferTest, verifyResizingProperlyAffectsMailboxes)
{
    WebExternalTextureMailbox mailbox;

    IntSize initialSize(initialWidth, initialHeight);
    IntSize alternateSize(initialWidth, alternateHeight);

    // Produce one mailbox at size 100x100.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, webContext()->mostRecentlyProducedSize());

    // Resize to 100x50.
    m_drawingBuffer->reset(IntSize(initialWidth, alternateHeight));
    m_drawingBuffer->mailboxReleased(mailbox, false);

    // Produce a mailbox at this size.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(alternateSize, webContext()->mostRecentlyProducedSize());

    // Reset to initial size.
    m_drawingBuffer->reset(IntSize(initialWidth, initialHeight));
    m_drawingBuffer->mailboxReleased(mailbox, false);

    // Prepare another mailbox and verify that it's the correct size.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, webContext()->mostRecentlyProducedSize());

    // Prepare one final mailbox and verify that it's the correct size.
    m_drawingBuffer->mailboxReleased(mailbox, false);
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, webContext()->mostRecentlyProducedSize());
    m_drawingBuffer->mailboxReleased(mailbox, false);
    m_drawingBuffer->beginDestruction();
}

TEST_F(DrawingBufferTest, verifyDestructionCompleteAfterAllMailboxesReleased)
{
    bool live = true;
    m_drawingBuffer->m_live = &live;

    WebExternalTextureMailbox mailbox1;
    WebExternalTextureMailbox mailbox2;
    WebExternalTextureMailbox mailbox3;

    IntSize initialSize(initialWidth, initialHeight);

    // Produce mailboxes.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox1, 0));
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox2, 0));
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox3, 0));

    m_drawingBuffer->markContentsChanged();
    m_drawingBuffer->mailboxReleased(mailbox1, false);

    m_drawingBuffer->beginDestruction();
    EXPECT_EQ(live, true);

    DrawingBufferForTests* weakPointer = m_drawingBuffer.get();
    m_drawingBuffer.clear();
    EXPECT_EQ(live, true);

    weakPointer->markContentsChanged();
    weakPointer->mailboxReleased(mailbox2, false);
    EXPECT_EQ(live, true);

    weakPointer->markContentsChanged();
    weakPointer->mailboxReleased(mailbox3, false);
    EXPECT_EQ(live, false);
}

TEST_F(DrawingBufferTest, verifyDrawingBufferStaysAliveIfResourcesAreLost)
{
    bool live = true;
    m_drawingBuffer->m_live = &live;
    WebExternalTextureMailbox mailbox1;
    WebExternalTextureMailbox mailbox2;
    WebExternalTextureMailbox mailbox3;

    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox1, 0));
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox2, 0));
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox3, 0));

    m_drawingBuffer->markContentsChanged();
    m_drawingBuffer->mailboxReleased(mailbox1, true);
    EXPECT_EQ(live, true);

    m_drawingBuffer->beginDestruction();
    EXPECT_EQ(live, true);

    m_drawingBuffer->markContentsChanged();
    m_drawingBuffer->mailboxReleased(mailbox2, false);
    EXPECT_EQ(live, true);

    DrawingBufferForTests* weakPtr = m_drawingBuffer.get();
    m_drawingBuffer.clear();
    EXPECT_EQ(live, true);

    weakPtr->markContentsChanged();
    weakPtr->mailboxReleased(mailbox3, true);
    EXPECT_EQ(live, false);
}

class TextureMailboxWrapper {
public:
    explicit TextureMailboxWrapper(const WebExternalTextureMailbox& mailbox)
        : m_mailbox(mailbox)
    { }

    bool operator==(const TextureMailboxWrapper& other) const
    {
        return !memcmp(m_mailbox.name, other.m_mailbox.name, sizeof(m_mailbox.name));
    }

    bool operator!=(const TextureMailboxWrapper& other) const
    {
        return !(*this == other);
    }

private:
    WebExternalTextureMailbox m_mailbox;
};

TEST_F(DrawingBufferTest, verifyOnlyOneRecycledMailboxMustBeKept)
{
    WebExternalTextureMailbox mailbox1;
    WebExternalTextureMailbox mailbox2;
    WebExternalTextureMailbox mailbox3;

    // Produce mailboxes.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox1, 0));
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox2, 0));
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox3, 0));

    // Release mailboxes by specific order; 1, 3, 2.
    m_drawingBuffer->markContentsChanged();
    m_drawingBuffer->mailboxReleased(mailbox1, false);
    m_drawingBuffer->markContentsChanged();
    m_drawingBuffer->mailboxReleased(mailbox3, false);
    m_drawingBuffer->markContentsChanged();
    m_drawingBuffer->mailboxReleased(mailbox2, false);

    // The first recycled mailbox must be 2. 1 and 3 were deleted by FIFO order because
    // DrawingBuffer never keeps more than one mailbox.
    WebExternalTextureMailbox recycledMailbox1;
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&recycledMailbox1, 0));
    EXPECT_EQ(TextureMailboxWrapper(mailbox2), TextureMailboxWrapper(recycledMailbox1));

    // The second recycled mailbox must be a new mailbox.
    WebExternalTextureMailbox recycledMailbox2;
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&recycledMailbox2, 0));
    EXPECT_NE(TextureMailboxWrapper(mailbox1), TextureMailboxWrapper(recycledMailbox2));
    EXPECT_NE(TextureMailboxWrapper(mailbox2), TextureMailboxWrapper(recycledMailbox2));
    EXPECT_NE(TextureMailboxWrapper(mailbox3), TextureMailboxWrapper(recycledMailbox2));

    m_drawingBuffer->mailboxReleased(recycledMailbox1, false);
    m_drawingBuffer->mailboxReleased(recycledMailbox2, false);
    m_drawingBuffer->beginDestruction();
}

TEST_F(DrawingBufferTest, verifyInsertAndWaitSyncTokenCorrectly)
{
    WebExternalTextureMailbox mailbox;

    // Produce mailboxes.
    m_drawingBuffer->markContentsChanged();
    EXPECT_EQ(0u, webContext()->mostRecentlyWaitedSyncToken());
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    // prepareMailbox() does not wait for any sync point.
    EXPECT_EQ(0u, webContext()->mostRecentlyWaitedSyncToken());

    WGC3Duint64 waitSyncToken = 0;
    webContext()->genSyncTokenCHROMIUM(webContext()->insertFenceSyncCHROMIUM(), reinterpret_cast<WGC3Dbyte*>(&waitSyncToken));
    memcpy(mailbox.syncToken, &waitSyncToken, sizeof(waitSyncToken));
    mailbox.validSyncToken = true;
    m_drawingBuffer->mailboxReleased(mailbox, false);
    // m_drawingBuffer will wait for the sync point when recycling.
    EXPECT_EQ(0u, webContext()->mostRecentlyWaitedSyncToken());

    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    // m_drawingBuffer waits for the sync point when recycling in prepareMailbox().
    EXPECT_EQ(waitSyncToken, webContext()->mostRecentlyWaitedSyncToken());

    m_drawingBuffer->beginDestruction();
    webContext()->genSyncTokenCHROMIUM(webContext()->insertFenceSyncCHROMIUM(), reinterpret_cast<WGC3Dbyte*>(&waitSyncToken));
    memcpy(mailbox.syncToken, &waitSyncToken, sizeof(waitSyncToken));
    mailbox.validSyncToken = true;
    m_drawingBuffer->mailboxReleased(mailbox, false);
    // m_drawingBuffer waits for the sync point because the destruction is in progress.
    EXPECT_EQ(waitSyncToken, webContext()->mostRecentlyWaitedSyncToken());
}

class DrawingBufferImageChromiumTest : public DrawingBufferTest {
protected:
    void SetUp() override
    {
        OwnPtr<WebGraphicsContext3DForTests> context = adoptPtr(new WebGraphicsContext3DForTests);
        m_context = context.get();
        RuntimeEnabledFeatures::setWebGLImageChromiumEnabled(true);
        m_imageId0 = webContext()->nextImageIdToBeCreated();
        EXPECT_CALL(*webContext(), bindTexImage2DMock(m_imageId0)).Times(1);
        m_drawingBuffer = DrawingBufferForTests::create(context.release(),
            IntSize(initialWidth, initialHeight), DrawingBuffer::Preserve);
        testing::Mock::VerifyAndClearExpectations(webContext());
    }

    void TearDown() override
    {
        RuntimeEnabledFeatures::setWebGLImageChromiumEnabled(false);
    }
    WGC3Duint m_imageId0;
};

TEST_F(DrawingBufferImageChromiumTest, verifyResizingReallocatesImages)
{
    WebExternalTextureMailbox mailbox;

    IntSize initialSize(initialWidth, initialHeight);
    IntSize alternateSize(initialWidth, alternateHeight);

    WGC3Duint m_imageId1 = webContext()->nextImageIdToBeCreated();
    EXPECT_CALL(*webContext(), bindTexImage2DMock(m_imageId1)).Times(1);
    // Produce one mailbox at size 100x100.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, webContext()->mostRecentlyProducedSize());
    EXPECT_TRUE(mailbox.allowOverlay);
    testing::Mock::VerifyAndClearExpectations(webContext());

    WGC3Duint m_imageId2 = webContext()->nextImageIdToBeCreated();
    EXPECT_CALL(*webContext(), bindTexImage2DMock(m_imageId2)).Times(1);
    EXPECT_CALL(*webContext(), destroyImageMock(m_imageId0)).Times(1);
    EXPECT_CALL(*webContext(), releaseTexImage2DMock(m_imageId0)).Times(1);
    // Resize to 100x50.
    m_drawingBuffer->reset(IntSize(initialWidth, alternateHeight));
    m_drawingBuffer->mailboxReleased(mailbox, false);
    testing::Mock::VerifyAndClearExpectations(webContext());

    WGC3Duint m_imageId3 = webContext()->nextImageIdToBeCreated();
    EXPECT_CALL(*webContext(), bindTexImage2DMock(m_imageId3)).Times(1);
    EXPECT_CALL(*webContext(), destroyImageMock(m_imageId1)).Times(1);
    EXPECT_CALL(*webContext(), releaseTexImage2DMock(m_imageId1)).Times(1);
    // Produce a mailbox at this size.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(alternateSize, webContext()->mostRecentlyProducedSize());
    EXPECT_TRUE(mailbox.allowOverlay);
    testing::Mock::VerifyAndClearExpectations(webContext());

    WGC3Duint m_imageId4 = webContext()->nextImageIdToBeCreated();
    EXPECT_CALL(*webContext(), bindTexImage2DMock(m_imageId4)).Times(1);
    EXPECT_CALL(*webContext(), destroyImageMock(m_imageId2)).Times(1);
    EXPECT_CALL(*webContext(), releaseTexImage2DMock(m_imageId2)).Times(1);
    // Reset to initial size.
    m_drawingBuffer->reset(IntSize(initialWidth, initialHeight));
    m_drawingBuffer->mailboxReleased(mailbox, false);
    testing::Mock::VerifyAndClearExpectations(webContext());

    WGC3Duint m_imageId5 = webContext()->nextImageIdToBeCreated();
    EXPECT_CALL(*webContext(), bindTexImage2DMock(m_imageId5)).Times(1);
    EXPECT_CALL(*webContext(), destroyImageMock(m_imageId3)).Times(1);
    EXPECT_CALL(*webContext(), releaseTexImage2DMock(m_imageId3)).Times(1);
    // Prepare another mailbox and verify that it's the correct size.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, webContext()->mostRecentlyProducedSize());
    EXPECT_TRUE(mailbox.allowOverlay);
    testing::Mock::VerifyAndClearExpectations(webContext());

    // Prepare one final mailbox and verify that it's the correct size.
    m_drawingBuffer->mailboxReleased(mailbox, false);
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, webContext()->mostRecentlyProducedSize());
    EXPECT_TRUE(mailbox.allowOverlay);
    m_drawingBuffer->mailboxReleased(mailbox, false);

    EXPECT_CALL(*webContext(), destroyImageMock(m_imageId5)).Times(1);
    EXPECT_CALL(*webContext(), releaseTexImage2DMock(m_imageId5)).Times(1);
    EXPECT_CALL(*webContext(), destroyImageMock(m_imageId4)).Times(1);
    EXPECT_CALL(*webContext(), releaseTexImage2DMock(m_imageId4)).Times(1);
    m_drawingBuffer->beginDestruction();
    testing::Mock::VerifyAndClearExpectations(webContext());
}

class DepthStencilTrackingContext : public MockWebGraphicsContext3D {
public:
    DepthStencilTrackingContext()
        : m_nextRenderBufferId(1)
        , m_stencilAttachment(0)
        , m_depthAttachment(0)
        , m_depthStencilAttachment(0) {}
    ~DepthStencilTrackingContext() override {}

    int numAllocatedRenderBuffer() const { return m_nextRenderBufferId - 1; }
    WebGLId stencilAttachment() const { return m_stencilAttachment; }
    WebGLId depthAttachment() const { return m_depthAttachment; }
    WebGLId depthStencilAttachment() const { return m_depthStencilAttachment; }

    WebString getString(WGC3Denum type) override
    {
        if (type == GL_EXTENSIONS) {
            return WebString::fromUTF8("GL_OES_packed_depth_stencil");
        }
        return WebString();
    }

    WebGLId createRenderbuffer() override
    {
        return ++m_nextRenderBufferId;
    }

    void framebufferRenderbuffer(WGC3Denum target, WGC3Denum attachment, WGC3Denum renderbuffertarget, WebGLId renderbuffer) override
    {
        switch (attachment) {
        case GL_DEPTH_ATTACHMENT:
            m_depthAttachment = renderbuffer;
            break;
        case GL_STENCIL_ATTACHMENT:
            m_stencilAttachment = renderbuffer;
            break;
        case GL_DEPTH_STENCIL_ATTACHMENT:
            m_depthStencilAttachment = renderbuffer;
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    void getIntegerv(WGC3Denum ptype, WGC3Dint* value) override
    {
        switch (ptype) {
        case GL_DEPTH_BITS:
            *value = (m_depthAttachment || m_depthStencilAttachment) ? 24 : 0;
            return;
        case GL_STENCIL_BITS:
            *value = (m_stencilAttachment || m_depthStencilAttachment) ? 8 : 0;
            return;
        }
        MockWebGraphicsContext3D::getIntegerv(ptype, value);
    }

private:
    WebGLId m_nextRenderBufferId;
    WebGLId m_stencilAttachment;
    WebGLId m_depthAttachment;
    WebGLId m_depthStencilAttachment;
};

struct DepthStencilTestCase {
    DepthStencilTestCase(bool requestStencil, bool requestDepth, int expectedRenderBuffers, const char* const testCaseName)
        : requestStencil(requestStencil)
        , requestDepth(requestDepth)
        , expectedRenderBuffers(expectedRenderBuffers)
        , testCaseName(testCaseName) { }

    bool requestStencil;
    bool requestDepth;
    int expectedRenderBuffers;
    const char* const testCaseName;
};

// This tests that when the packed depth+stencil extension is supported DrawingBuffer always allocates
// a single packed renderbuffer if either is requested and properly computes the actual context attributes
// as defined by WebGL. We always allocate a packed buffer in this case since many desktop OpenGL drivers
// that support this extension do not consider a framebuffer with only a depth or a stencil buffer attached
// to be complete.
TEST(DrawingBufferDepthStencilTest, packedDepthStencilSupported)
{
    DepthStencilTestCase cases[] = {
        DepthStencilTestCase(false, false, 0, "neither"),
        DepthStencilTestCase(true, false, 1, "stencil only"),
        DepthStencilTestCase(false, true, 1, "depth only"),
        DepthStencilTestCase(true, true, 1, "both"),
    };

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(cases); i++) {
        SCOPED_TRACE(cases[i].testCaseName);
        OwnPtr<DepthStencilTrackingContext> context = adoptPtr(new DepthStencilTrackingContext);
        DepthStencilTrackingContext* trackingContext = context.get();
        DrawingBuffer::PreserveDrawingBuffer preserve = DrawingBuffer::Preserve;

        WebGraphicsContext3D::Attributes requestedAttributes;
        requestedAttributes.stencil = cases[i].requestStencil;
        requestedAttributes.depth = cases[i].requestDepth;
        RefPtr<DrawingBuffer> drawingBuffer = DrawingBuffer::create(context.release(), IntSize(10, 10), preserve, requestedAttributes);

        EXPECT_EQ(cases[i].requestDepth, drawingBuffer->getActualAttributes().depth);
        EXPECT_EQ(cases[i].requestStencil, drawingBuffer->getActualAttributes().stencil);
        EXPECT_EQ(cases[i].expectedRenderBuffers, trackingContext->numAllocatedRenderBuffer());
        if (cases[i].requestDepth || cases[i].requestStencil) {
            EXPECT_NE(0u, trackingContext->depthStencilAttachment());
            EXPECT_EQ(0u, trackingContext->depthAttachment());
            EXPECT_EQ(0u, trackingContext->stencilAttachment());
        } else {
            EXPECT_EQ(0u, trackingContext->depthStencilAttachment());
            EXPECT_EQ(0u, trackingContext->depthAttachment());
            EXPECT_EQ(0u, trackingContext->stencilAttachment());
        }

        drawingBuffer->reset(IntSize(10, 20));
        EXPECT_EQ(cases[i].requestDepth, drawingBuffer->getActualAttributes().depth);
        EXPECT_EQ(cases[i].requestStencil, drawingBuffer->getActualAttributes().stencil);
        EXPECT_EQ(cases[i].expectedRenderBuffers, trackingContext->numAllocatedRenderBuffer());
        if (cases[i].requestDepth || cases[i].requestStencil) {
            EXPECT_NE(0u, trackingContext->depthStencilAttachment());
            EXPECT_EQ(0u, trackingContext->depthAttachment());
            EXPECT_EQ(0u, trackingContext->stencilAttachment());
        } else {
            EXPECT_EQ(0u, trackingContext->depthStencilAttachment());
            EXPECT_EQ(0u, trackingContext->depthAttachment());
            EXPECT_EQ(0u, trackingContext->stencilAttachment());
        }

        drawingBuffer->beginDestruction();
    }
}

TEST_F(DrawingBufferTest, verifySetIsHiddenProperlyAffectsMailboxes)
{
    blink::WebExternalTextureMailbox mailbox;

    // Produce mailboxes.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));

    mailbox.validSyncToken = webContext()->genSyncTokenCHROMIUM(webContext()->insertFenceSyncCHROMIUM(), mailbox.syncToken);
    m_drawingBuffer->setIsHidden(true);
    m_drawingBuffer->mailboxReleased(mailbox);
    // m_drawingBuffer deletes mailbox immediately when hidden.

    WGC3Duint waitSyncToken = 0;
    memcpy(&waitSyncToken, mailbox.syncToken, sizeof(waitSyncToken));
    EXPECT_EQ(waitSyncToken, webContext()->mostRecentlyWaitedSyncToken());

    m_drawingBuffer->beginDestruction();
}

class DrawingBufferImageChromiumFallbackTest : public DrawingBufferTest {
protected:
    void SetUp() override
    {
        OwnPtr<WebGraphicsContext3DForTests> context = adoptPtr(new WebGraphicsContext3DForTests);
        context->setAllowImageChromium(false);
        m_context = context.get();
        RuntimeEnabledFeatures::setWebGLImageChromiumEnabled(true);
        m_drawingBuffer = DrawingBufferForTests::create(context.release(),
            IntSize(initialWidth, initialHeight), DrawingBuffer::Preserve);
    }

    void TearDown() override
    {
        RuntimeEnabledFeatures::setWebGLImageChromiumEnabled(false);
    }
};

TEST_F(DrawingBufferImageChromiumFallbackTest, verifyImageChromiumFallback)
{
    WebExternalTextureMailbox mailbox;

    IntSize initialSize(initialWidth, initialHeight);
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, webContext()->mostRecentlyProducedSize());
    EXPECT_FALSE(mailbox.allowOverlay);

    m_drawingBuffer->mailboxReleased(mailbox, false);
    m_drawingBuffer->beginDestruction();
}

} // namespace blink
