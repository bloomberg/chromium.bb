// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/Resource.h"

#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"

namespace blink {

namespace {

class MockPlatform final : public TestingPlatformSupport {
public:
    MockPlatform() { }
    ~MockPlatform() override { }

    // From blink::Platform:
    void cacheMetadata(const WebURL& url, int64_t, const char*, size_t) override
    {
        m_cachedURLs.append(url);
    }

    const Vector<WebURL>& cachedURLs() const
    {
        return m_cachedURLs;
    }

private:
    Vector<WebURL> m_cachedURLs;
};

ResourceResponse createTestResourceResponse()
{
    ResourceResponse response;
    response.setURL(URLTestHelpers::toKURL("https://example.com/"));
    response.setHTTPStatusCode(200);
    return response;
}

void createTestResourceAndSetCachedMetadata(const ResourceResponse& response)
{
    const char testData[] = "test data";
    RefPtrWillBeRawPtr<Resource> resource = Resource::create(ResourceRequest(response.url()), Resource::Raw);
    resource->setResponse(response);
    resource->cacheHandler()->setCachedMetadata(100, testData, sizeof(testData), CachedMetadataHandler::SendToPlatform);
    return;
}

} // anonymous namespace

TEST(ResourceTest, SetCachedMetadata_SendsMetadataToPlatform)
{
    MockPlatform mock;
    ResourceResponse response(createTestResourceResponse());
    createTestResourceAndSetCachedMetadata(response);
    EXPECT_EQ(1u, mock.cachedURLs().size());
}

TEST(ResourceTest, SetCachedMetadata_DoesNotSendMetadataToPlatformWhenFetchedViaServiceWorker)
{
    MockPlatform mock;
    ResourceResponse response(createTestResourceResponse());
    response.setWasFetchedViaServiceWorker(true);
    createTestResourceAndSetCachedMetadata(response);
    EXPECT_EQ(0u, mock.cachedURLs().size());
}

} // namespace blink
