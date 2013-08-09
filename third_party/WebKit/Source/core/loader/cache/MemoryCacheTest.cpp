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
#include "core/loader/cache/MemoryCache.h"

#include "core/loader/cache/MockImageResourceClient.h"
#include "core/loader/cache/RawResource.h"
#include "core/loader/cache/ResourcePtr.h"
#include "core/platform/network/ResourceRequest.h"
#include "wtf/OwnPtr.h"

#include <gtest/gtest.h>

namespace WebCore {

class MemoryCacheTest : public ::testing::Test {
public:
    class MockImageResource : public WebCore::Resource {
    public:
        MockImageResource(const ResourceRequest& request, Type type)
            : Resource(request, type)
        {
        }

        virtual void appendData(const char* data, int len)
        {
            Resource::appendData(data, len);
            setDecodedSize(this->size());
        }

        virtual void destroyDecodedData()
        {
            setDecodedSize(0);
        }
    };

protected:
    virtual void SetUp()
    {
        // Save the global memory cache to restore it upon teardown.
        m_globalMemoryCache = adoptPtr(memoryCache());
        // Create the test memory cache instance and hook it in.
        m_testingMemoryCache = adoptPtr(new MemoryCache());
        setMemoryCacheForTesting(m_testingMemoryCache.leakPtr());
    }

    virtual void TearDown()
    {
        // Regain the ownership of testing memory cache, so that it will be
        // destroyed.
        m_testingMemoryCache = adoptPtr(memoryCache());
        // Yield the ownership of the global memory cache back.
        setMemoryCacheForTesting(m_globalMemoryCache.leakPtr());
    }

    OwnPtr<MemoryCache> m_testingMemoryCache;
    OwnPtr<MemoryCache> m_globalMemoryCache;
};

// Verifies that setters and getters for cache capacities work correcty.
TEST_F(MemoryCacheTest, CapacityAccounting)
{
    const unsigned totalCapacity = 100;
    const unsigned minDeadCapacity = 10;
    const unsigned maxDeadCapacity = 50;
    memoryCache()->setCapacities(minDeadCapacity, maxDeadCapacity, totalCapacity);

    ASSERT_EQ(totalCapacity, memoryCache()->capacity());
    ASSERT_EQ(minDeadCapacity, memoryCache()->minDeadCapacity());
    ASSERT_EQ(maxDeadCapacity, memoryCache()->maxDeadCapacity());
}

// Verifies that dead resources that exceed dead resource capacity are evicted
// from cache when pruning.
TEST_F(MemoryCacheTest, DeadResourceEviction)
{
    const unsigned totalCapacity = 1000000;
    const unsigned minDeadCapacity = 0;
    const unsigned maxDeadCapacity = 0;
    memoryCache()->setCapacities(minDeadCapacity, maxDeadCapacity, totalCapacity);

    ResourcePtr<Resource> cachedResource =
        new Resource(ResourceRequest(""), Resource::Raw);
    const char data[5] = "abcd";
    cachedResource->appendData(data, 3);
    // The resource size has to be nonzero for this test to be meaningful, but
    // we do not rely on it having any particular value.
    ASSERT_GT(cachedResource->size(), 0u);

    ASSERT_EQ(0u, memoryCache()->deadSize());
    ASSERT_EQ(0u, memoryCache()->liveSize());

    memoryCache()->add(cachedResource.get());
    ASSERT_EQ(cachedResource->size(), memoryCache()->deadSize());
    ASSERT_EQ(0u, memoryCache()->liveSize());

    memoryCache()->prune();
    ASSERT_EQ(0u, memoryCache()->deadSize());
    ASSERT_EQ(0u, memoryCache()->liveSize());
}

// Verifies that CachedResources are evicted from the decode cache
// according to their DecodeCachePriority.
TEST_F(MemoryCacheTest, DecodeCacheOrder)
{
    memoryCache()->setDelayBeforeLiveDecodedPrune(0);
    ResourcePtr<MockImageResource> cachedImageLowPriority =
        new MockImageResource(ResourceRequest(""), Resource::Raw);
    ResourcePtr<MockImageResource> cachedImageHighPriority =
        new MockImageResource(ResourceRequest(""), Resource::Raw);

    MockImageResourceClient clientLowPriority;
    MockImageResourceClient clientHighPriority;
    cachedImageLowPriority->addClient(&clientLowPriority);
    cachedImageHighPriority->addClient(&clientHighPriority);

    const char data[5] = "abcd";
    cachedImageLowPriority->appendData(data, 1);
    cachedImageHighPriority->appendData(data, 4);
    const unsigned lowPrioritySize = cachedImageLowPriority->size();
    const unsigned highPrioritySize = cachedImageHighPriority->size();
    const unsigned lowPriorityMockDecodeSize = cachedImageLowPriority->decodedSize();
    const unsigned highPriorityMockDecodeSize = cachedImageHighPriority->decodedSize();
    const unsigned totalSize = lowPrioritySize + highPrioritySize;

    // Verify that the sizes are different to ensure that we can test eviction order.
    ASSERT_GT(lowPrioritySize, 0u);
    ASSERT_NE(lowPrioritySize, highPrioritySize);
    ASSERT_GT(lowPriorityMockDecodeSize, 0u);
    ASSERT_NE(lowPriorityMockDecodeSize, highPriorityMockDecodeSize);

    ASSERT_EQ(memoryCache()->deadSize(), 0u);
    ASSERT_EQ(memoryCache()->liveSize(), 0u);

    // Add the items. The item added first would normally be evicted first.
    memoryCache()->add(cachedImageHighPriority.get());
    ASSERT_EQ(memoryCache()->deadSize(), 0u);
    ASSERT_EQ(memoryCache()->liveSize(), highPrioritySize);

    memoryCache()->add(cachedImageLowPriority.get());
    ASSERT_EQ(memoryCache()->deadSize(), 0u);
    ASSERT_EQ(memoryCache()->liveSize(), highPrioritySize + lowPrioritySize);

    // Insert all items in the decoded items list with the same priority
    memoryCache()->insertInLiveDecodedResourcesList(cachedImageHighPriority.get());
    memoryCache()->insertInLiveDecodedResourcesList(cachedImageLowPriority.get());
    ASSERT_EQ(memoryCache()->deadSize(), 0u);
    ASSERT_EQ(memoryCache()->liveSize(), totalSize);

    // Now we will assign their priority and make sure they are moved to the correct buckets.
    cachedImageLowPriority->setCacheLiveResourcePriority(Resource::CacheLiveResourcePriorityLow);
    cachedImageHighPriority->setCacheLiveResourcePriority(Resource::CacheLiveResourcePriorityHigh);

    // Should first prune the LowPriority item.
    memoryCache()->setCapacities(memoryCache()->minDeadCapacity(), memoryCache()->liveSize() - 10, memoryCache()->liveSize() - 10);
    memoryCache()->prune();
    ASSERT_EQ(memoryCache()->deadSize(), 0u);
    ASSERT_EQ(memoryCache()->liveSize(), totalSize - lowPriorityMockDecodeSize);

    // Should prune the HighPriority item.
    memoryCache()->setCapacities(memoryCache()->minDeadCapacity(), memoryCache()->liveSize() - 10, memoryCache()->liveSize() - 10);
    memoryCache()->prune();
    ASSERT_EQ(memoryCache()->deadSize(), 0u);
    ASSERT_EQ(memoryCache()->liveSize(), totalSize - lowPriorityMockDecodeSize - highPriorityMockDecodeSize);
}
} // namespace
