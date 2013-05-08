/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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
#include "core/loader/cache/CachedImage.h"

#include "core/loader/cache/CachedImageClient.h"
#include "core/loader/cache/CachedResourceHandle.h"
#include "core/platform/SharedBuffer.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class MockCachedImageClient : public WebCore::CachedImageClient {
public:
    MockCachedImageClient()
        : m_imageChangedCount(0)
        , m_notifyFinishedCalled(false)
    {
    }

    virtual ~MockCachedImageClient() { }
    virtual void imageChanged(CachedImage*, const IntRect*)
    {
        m_imageChangedCount++;
    }

    virtual void notifyFinished(CachedResource*)
    {
        ASSERT_FALSE(m_notifyFinishedCalled);
        m_notifyFinishedCalled = true;
    }

    int imageChangedCount() const { return m_imageChangedCount; }
    bool notifyFinishedCalled() const { return m_notifyFinishedCalled; }

private:
    int m_imageChangedCount;
    bool m_notifyFinishedCalled;
};

TEST(CachedImageTest, MultipartImage)
{
    CachedResourceHandle<CachedImage> cachedImage = new CachedImage(ResourceRequest());
    cachedImage->setLoading(true);

    MockCachedImageClient client;
    cachedImage->addClient(&client);

    // Send the multipart response. No image or data buffer is created.
    cachedImage->responseReceived(ResourceResponse(KURL(), "multipart/x-mixed-replace", 0, String(), String()));
    ASSERT_FALSE(cachedImage->resourceBuffer());
    ASSERT_FALSE(cachedImage->hasImage());
    ASSERT_EQ(client.imageChangedCount(), 0);
    ASSERT_FALSE(client.notifyFinishedCalled());

    // Send the response for the first real part. No image or data buffer is created.
    const char* svgData = "<svg xmlns='http://www.w3.org/2000/svg' width='1' height='1'><rect width='1' height='1' fill='green'/></svg>";
    unsigned svgDataLength = strlen(svgData);
    cachedImage->responseReceived(ResourceResponse(KURL(), "image/svg+xml", svgDataLength, String(), String()));
    ASSERT_FALSE(cachedImage->resourceBuffer());
    ASSERT_FALSE(cachedImage->hasImage());
    ASSERT_EQ(client.imageChangedCount(), 0);
    ASSERT_FALSE(client.notifyFinishedCalled());

    // The first bytes arrive. The data buffer is created, but no image is created.
    cachedImage->appendData(svgData, svgDataLength);
    ASSERT_TRUE(cachedImage->resourceBuffer());
    ASSERT_EQ(cachedImage->resourceBuffer()->size(), svgDataLength);
    ASSERT_FALSE(cachedImage->hasImage());
    ASSERT_EQ(client.imageChangedCount(), 0);
    ASSERT_FALSE(client.notifyFinishedCalled());

    // This part finishes. The image is created, callbacks are sent, and the data buffer is cleared.
    cachedImage->finish();
    ASSERT_FALSE(cachedImage->resourceBuffer());
    ASSERT_FALSE(cachedImage->errorOccurred());
    ASSERT_TRUE(cachedImage->hasImage());
    ASSERT_FALSE(cachedImage->image()->isNull());
    ASSERT_EQ(cachedImage->image()->width(), 1);
    ASSERT_EQ(cachedImage->image()->height(), 1);
    ASSERT_EQ(client.imageChangedCount(), 2);
    ASSERT_TRUE(client.notifyFinishedCalled());
}

} // namespace
