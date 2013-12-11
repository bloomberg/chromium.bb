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

#include "config.h"

#include "platform/graphics/gpu/DrawingBuffer.h"

#include "platform/graphics/GraphicsContext3D.h"
#include "public/platform/Platform.h"
#include "public/platform/WebExternalTextureMailbox.h"
#include "web/tests/MockWebGraphicsContext3D.h"
#include "wtf/RefPtr.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;
using namespace blink;
using testing::Test;
using testing::_;

namespace {

class FakeContextEvictionManager : public ContextEvictionManager {
public:
    void forciblyLoseOldestContext(const String& reason) { }
    IntSize oldestContextSize() { return IntSize(); }
};

class WebGraphicsContext3DForTests : public MockWebGraphicsContext3D {
public:
    WebGraphicsContext3DForTests()
        : MockWebGraphicsContext3D()
        , m_boundTexture(0)
        , m_currentMailboxByte(0) { }

    virtual void bindTexture(WGC3Denum target, WebGLId texture)
    {
        if (target == GL_TEXTURE_2D) {
            m_boundTexture = texture;
        }
    }

    virtual void texImage2D(WGC3Denum target, WGC3Dint level, WGC3Denum internalformat, WGC3Dsizei width, WGC3Dsizei height, WGC3Dint border, WGC3Denum format, WGC3Denum type, const void* pixels)
    {
        if (target == GL_TEXTURE_2D && !level) {
            m_textureSizes.set(m_boundTexture, IntSize(width, height));
        }
    }

    virtual void genMailboxCHROMIUM(WGC3Dbyte* mailbox)
    {
        ++m_currentMailboxByte;
        WebExternalTextureMailbox temp;
        memset(mailbox, m_currentMailboxByte, sizeof(temp.name));
    }

    virtual void produceTextureCHROMIUM(WGC3Denum target, const WGC3Dbyte* mailbox)
    {
        ASSERT_EQ(target, static_cast<WGC3Denum>(GL_TEXTURE_2D));
        m_mostRecentlyProducedSize = m_textureSizes.get(m_boundTexture);
    }

    IntSize mostRecentlyProducedSize()
    {
        return m_mostRecentlyProducedSize;
    }

private:
    WebGLId m_boundTexture;
    HashMap<WebGLId, IntSize> m_textureSizes;
    WGC3Dbyte m_currentMailboxByte;
    IntSize m_mostRecentlyProducedSize;
};

static const int initialWidth = 100;
static const int initialHeight = 100;
static const int alternateHeight = 50;

} // namespace

class DrawingBufferTest : public Test {
protected:
    virtual void SetUp()
    {
        RefPtr<FakeContextEvictionManager> contextEvictionManager = adoptRef(new FakeContextEvictionManager());
        RefPtr<GraphicsContext3D> context = GraphicsContext3D::createGraphicsContextFromWebContext(adoptPtr(new WebGraphicsContext3DForTests));
        m_drawingBuffer = DrawingBuffer::create(context.get(), IntSize(initialWidth, initialHeight), DrawingBuffer::Discard, contextEvictionManager.release());
    }

    WebGraphicsContext3DForTests* webContext()
    {
        return static_cast<WebGraphicsContext3DForTests*>(m_drawingBuffer->context());
    }

    RefPtr<DrawingBuffer> m_drawingBuffer;
};

namespace {

TEST_F(DrawingBufferTest, verifyNoNewBuffersAfterContextLostWithMailboxes)
{
    // Tell the buffer its contents changed and context was lost.
    m_drawingBuffer->markContentsChanged();
    m_drawingBuffer->releaseResources();

    blink::WebExternalTextureMailbox mailbox;
    EXPECT_FALSE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
}

TEST_F(DrawingBufferTest, verifyResizingProperlyAffectsMailboxes)
{
    blink::WebExternalTextureMailbox mailbox;

    IntSize initialSize(initialWidth, initialHeight);
    IntSize alternateSize(initialWidth, alternateHeight);

    // Produce one mailbox at size 100x100.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, webContext()->mostRecentlyProducedSize());

    // Resize to 100x50.
    m_drawingBuffer->reset(IntSize(initialWidth, alternateHeight));
    m_drawingBuffer->mailboxReleased(mailbox);

    // Produce a mailbox at this size.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(alternateSize, webContext()->mostRecentlyProducedSize());

    // Reset to initial size.
    m_drawingBuffer->reset(IntSize(initialWidth, initialHeight));
    m_drawingBuffer->mailboxReleased(mailbox);

    // Prepare another mailbox and verify that it's the correct size.
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, webContext()->mostRecentlyProducedSize());

    // Prepare one final mailbox and verify that it's the correct size.
    m_drawingBuffer->mailboxReleased(mailbox);
    m_drawingBuffer->markContentsChanged();
    EXPECT_TRUE(m_drawingBuffer->prepareMailbox(&mailbox, 0));
    EXPECT_EQ(initialSize, webContext()->mostRecentlyProducedSize());
}

} // namespace
