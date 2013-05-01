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

#include "core/platform/graphics/gpu/DrawingBuffer.h"

#include "FakeWebGraphicsContext3D.h"
#include "core/platform/chromium/support/GraphicsContext3DPrivate.h"
#include <public/Platform.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wtf/RefPtr.h>

using namespace WebCore;
using namespace WebKit;
using testing::Test;
using testing::_;

namespace {

class MockWebTextureUpdater : public WebTextureUpdater {
public:
    MOCK_METHOD3(appendCopy, void(unsigned, unsigned, WebSize));
};

class FakeContextEvictionManager : public ContextEvictionManager {
public:
    void forciblyLoseOldestContext(const String& reason) { }
    IntSize oldestContextSize() { return IntSize(); }
};

} // namespace

class DrawingBufferTest : public Test {
protected:
    virtual void SetUp()
    {
        RefPtr<FakeContextEvictionManager> contextEvictionManager = adoptRef(new FakeContextEvictionManager());
        RefPtr<GraphicsContext3D> context = GraphicsContext3DPrivate::createGraphicsContextFromWebContext(adoptPtr(new FakeWebGraphicsContext3D));
        const IntSize size(100, 100);
        m_drawingBuffer = DrawingBuffer::create(context.get(), size, DrawingBuffer::Discard, contextEvictionManager.release());
    }

    RefPtr<DrawingBuffer> m_drawingBuffer;
};

namespace {

TEST_F(DrawingBufferTest, verifyNoNewBuffersAfterContextLostWithTextures)
{
    // Tell the buffer its contents changed and context was lost.
    m_drawingBuffer->markContentsChanged();
    m_drawingBuffer->releaseResources();

    MockWebTextureUpdater updater;
    EXPECT_CALL(updater, appendCopy(_, _, _)).Times(0);

    unsigned zeroTexture = 0;
    EXPECT_EQ(zeroTexture, m_drawingBuffer->prepareTexture(updater));
}

TEST_F(DrawingBufferTest, verifyNoNewBuffersAfterContextLostWithMailboxes)
{
    // Tell the buffer its contents changed and context was lost.
    m_drawingBuffer->markContentsChanged();
    m_drawingBuffer->releaseResources();

    WebKit::WebExternalTextureMailbox mailbox;
    EXPECT_FALSE(m_drawingBuffer->prepareMailbox(&mailbox));
}

} // namespace
