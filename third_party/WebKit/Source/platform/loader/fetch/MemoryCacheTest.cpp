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

#include "platform/loader/fetch/MemoryCache.h"

#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/testing/MockResourceClient.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MemoryCacheTest : public ::testing::Test {
 public:
  class FakeDecodedResource final : public Resource {
   public:
    static FakeDecodedResource* Create(const String& url, Type type) {
      ResourceRequest request(url);
      request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);
      ResourceLoaderOptions options;
      return new FakeDecodedResource(request, type, options);
    }

    virtual void AppendData(const char* data, size_t len) {
      Resource::AppendData(data, len);
      SetDecodedSize(this->size());
    }

   private:
    FakeDecodedResource(const ResourceRequest& request,
                        Type type,
                        const ResourceLoaderOptions& options)
        : Resource(request, type, options) {}

    void DestroyDecodedDataIfPossible() override { SetDecodedSize(0); }
  };

  class FakeResource final : public Resource {
   public:
    static FakeResource* Create(const char* url, Type type) {
      return Create(KURL(url), type);
    }
    static FakeResource* Create(const KURL& url, Type type) {
      ResourceRequest request(url);
      request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);

      ResourceLoaderOptions options;

      return new FakeResource(request, type, options);
    }

    void FakeEncodedSize(size_t size) { SetEncodedSize(size); }

   private:
    FakeResource(const ResourceRequest& request,
                 Type type,
                 const ResourceLoaderOptions& options)
        : Resource(request, type, options) {}
  };

 protected:
  virtual void SetUp() {
    // Save the global memory cache to restore it upon teardown.
    global_memory_cache_ = ReplaceMemoryCacheForTesting(MemoryCache::Create());
  }

  virtual void TearDown() {
    ReplaceMemoryCacheForTesting(global_memory_cache_.Release());
  }

  Persistent<MemoryCache> global_memory_cache_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

// Verifies that setters and getters for cache capacities work correcty.
TEST_F(MemoryCacheTest, CapacityAccounting) {
  const size_t kSizeMax = ~static_cast<size_t>(0);
  const size_t kTotalCapacity = kSizeMax / 4;
  GetMemoryCache()->SetCapacity(kTotalCapacity);
  EXPECT_EQ(kTotalCapacity, GetMemoryCache()->Capacity());
}

TEST_F(MemoryCacheTest, VeryLargeResourceAccounting) {
  const size_t kSizeMax = ~static_cast<size_t>(0);
  const size_t kTotalCapacity = kSizeMax / 4;
  const size_t kResourceSize1 = kSizeMax / 16;
  const size_t kResourceSize2 = kSizeMax / 20;
  GetMemoryCache()->SetCapacity(kTotalCapacity);
  FakeResource* cached_resource =
      FakeResource::Create("http://test/resource", Resource::kRaw);
  cached_resource->FakeEncodedSize(kResourceSize1);

  EXPECT_EQ(0u, GetMemoryCache()->size());
  GetMemoryCache()->Add(cached_resource);
  EXPECT_EQ(cached_resource->size(), GetMemoryCache()->size());

  Persistent<MockResourceClient> client =
      new MockResourceClient(cached_resource);
  EXPECT_EQ(cached_resource->size(), GetMemoryCache()->size());

  cached_resource->FakeEncodedSize(kResourceSize2);
  EXPECT_EQ(cached_resource->size(), GetMemoryCache()->size());
}

static void RunTask1(Resource* resource1, Resource* resource2) {
  // The resource size has to be nonzero for this test to be meaningful, but
  // we do not rely on it having any particular value.
  EXPECT_GT(resource1->size(), 0u);
  EXPECT_GT(resource2->size(), 0u);

  EXPECT_EQ(0u, GetMemoryCache()->size());

  GetMemoryCache()->Add(resource1);
  GetMemoryCache()->Add(resource2);

  size_t total_size = resource1->size() + resource2->size();
  EXPECT_EQ(total_size, GetMemoryCache()->size());
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);

  // We expect actual pruning doesn't occur here synchronously but deferred
  // to the end of this task, due to the previous pruning invoked in
  // testResourcePruningAtEndOfTask().
  GetMemoryCache()->Prune();
  EXPECT_EQ(total_size, GetMemoryCache()->size());
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);
}

static void RunTask2(unsigned size_without_decode) {
  // Next task: now, the resources was pruned.
  EXPECT_EQ(size_without_decode, GetMemoryCache()->size());
}

static void TestResourcePruningAtEndOfTask(Resource* resource1,
                                           Resource* resource2) {
  GetMemoryCache()->SetDelayBeforeLiveDecodedPrune(0);

  // Enforce pruning by adding |dummyResource| and then call prune().
  Resource* dummy_resource =
      RawResource::CreateForTest("http://dummy", Resource::kRaw);
  GetMemoryCache()->Add(dummy_resource);
  EXPECT_GT(GetMemoryCache()->size(), 1u);
  const unsigned kTotalCapacity = 1;
  GetMemoryCache()->SetCapacity(kTotalCapacity);
  GetMemoryCache()->Prune();
  GetMemoryCache()->Remove(dummy_resource);
  EXPECT_EQ(0u, GetMemoryCache()->size());

  const char kData[6] = "abcde";
  resource1->AppendData(kData, 3u);
  resource1->FinishForTest();
  Persistent<MockResourceClient> client = new MockResourceClient(resource2);
  resource2->AppendData(kData, 4u);
  resource2->FinishForTest();

  Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&RunTask1, WrapPersistent(resource1),
                                 WrapPersistent(resource2)));
  Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      WTF::Bind(&RunTask2,
                resource1->EncodedSize() + resource1->OverheadSize() +
                    resource2->EncodedSize() + resource2->OverheadSize()));
  static_cast<TestingPlatformSupportWithMockScheduler*>(Platform::Current())
      ->RunUntilIdle();
}

// Verified that when ordering a prune in a runLoop task, the prune
// is deferred to the end of the task.
TEST_F(MemoryCacheTest, ResourcePruningAtEndOfTask_Basic) {
  Resource* resource1 =
      FakeDecodedResource::Create("http://test/resource1", Resource::kRaw);
  Resource* resource2 =
      FakeDecodedResource::Create("http://test/resource2", Resource::kRaw);
  TestResourcePruningAtEndOfTask(resource1, resource2);
}

TEST_F(MemoryCacheTest, ResourcePruningAtEndOfTask_MultipleResourceMaps) {
  {
    Resource* resource1 =
        FakeDecodedResource::Create("http://test/resource1", Resource::kRaw);
    Resource* resource2 =
        FakeDecodedResource::Create("http://test/resource2", Resource::kRaw);
    resource1->SetCacheIdentifier("foo");
    TestResourcePruningAtEndOfTask(resource1, resource2);
    GetMemoryCache()->EvictResources();
  }
  {
    Resource* resource1 =
        FakeDecodedResource::Create("http://test/resource1", Resource::kRaw);
    Resource* resource2 =
        FakeDecodedResource::Create("http://test/resource2", Resource::kRaw);
    resource1->SetCacheIdentifier("foo");
    resource2->SetCacheIdentifier("bar");
    TestResourcePruningAtEndOfTask(resource1, resource2);
    GetMemoryCache()->EvictResources();
  }
}

// Verifies that
// - Resources are not pruned synchronously when ResourceClient is removed.
// - size() is updated appropriately when Resources are added to MemoryCache
//   and garbage collected.
static void TestClientRemoval(Resource* resource1, Resource* resource2) {
  const char kData[6] = "abcde";
  Persistent<MockResourceClient> client1 = new MockResourceClient(resource1);
  resource1->AppendData(kData, 4u);
  Persistent<MockResourceClient> client2 = new MockResourceClient(resource2);
  resource2->AppendData(kData, 4u);

  GetMemoryCache()->SetCapacity(0);
  GetMemoryCache()->Add(resource1);
  GetMemoryCache()->Add(resource2);

  size_t original_total_size = resource1->size() + resource2->size();

  // Call prune. There is nothing to prune, but this will initialize
  // the prune timestamp, allowing future prunes to be deferred.
  GetMemoryCache()->Prune();
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);
  EXPECT_EQ(original_total_size, GetMemoryCache()->size());

  // Removing the client from resource1 should not trigger pruning.
  client1->RemoveAsClient();
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);
  EXPECT_EQ(original_total_size, GetMemoryCache()->size());
  EXPECT_TRUE(GetMemoryCache()->Contains(resource1));
  EXPECT_TRUE(GetMemoryCache()->Contains(resource2));

  // Removing the client from resource2 should not trigger pruning.
  client2->RemoveAsClient();
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);
  EXPECT_EQ(original_total_size, GetMemoryCache()->size());
  EXPECT_TRUE(GetMemoryCache()->Contains(resource1));
  EXPECT_TRUE(GetMemoryCache()->Contains(resource2));

  WeakPersistent<Resource> resource1_weak = resource1;
  WeakPersistent<Resource> resource2_weak = resource2;

  ThreadState::Current()->CollectGarbage(BlinkGC::kNoHeapPointersOnStack,
                                         BlinkGC::kGCWithSweep,
                                         BlinkGC::kForcedGC);
  // Resources are garbage-collected (WeakMemoryCache) and thus removed
  // from MemoryCache.
  EXPECT_FALSE(resource1_weak);
  EXPECT_FALSE(resource2_weak);
  EXPECT_EQ(0u, GetMemoryCache()->size());
}

TEST_F(MemoryCacheTest, ClientRemoval_Basic) {
  Resource* resource1 =
      FakeDecodedResource::Create("http://foo.com", Resource::kRaw);
  Resource* resource2 =
      FakeDecodedResource::Create("http://test/resource", Resource::kRaw);
  TestClientRemoval(resource1, resource2);
}

TEST_F(MemoryCacheTest, ClientRemoval_MultipleResourceMaps) {
  {
    Resource* resource1 =
        FakeDecodedResource::Create("http://foo.com", Resource::kRaw);
    resource1->SetCacheIdentifier("foo");
    Resource* resource2 =
        FakeDecodedResource::Create("http://test/resource", Resource::kRaw);
    TestClientRemoval(resource1, resource2);
    GetMemoryCache()->EvictResources();
  }
  {
    Resource* resource1 =
        FakeDecodedResource::Create("http://foo.com", Resource::kRaw);
    Resource* resource2 =
        FakeDecodedResource::Create("http://test/resource", Resource::kRaw);
    resource2->SetCacheIdentifier("foo");
    TestClientRemoval(resource1, resource2);
    GetMemoryCache()->EvictResources();
  }
  {
    Resource* resource1 =
        FakeDecodedResource::Create("http://test/resource", Resource::kRaw);
    resource1->SetCacheIdentifier("foo");
    Resource* resource2 =
        FakeDecodedResource::Create("http://test/resource", Resource::kRaw);
    resource2->SetCacheIdentifier("bar");
    TestClientRemoval(resource1, resource2);
    GetMemoryCache()->EvictResources();
  }
}

TEST_F(MemoryCacheTest, RemoveDuringRevalidation) {
  FakeResource* resource1 =
      FakeResource::Create("http://test/resource", Resource::kRaw);
  GetMemoryCache()->Add(resource1);

  FakeResource* resource2 =
      FakeResource::Create("http://test/resource", Resource::kRaw);
  GetMemoryCache()->Remove(resource1);
  GetMemoryCache()->Add(resource2);
  EXPECT_TRUE(GetMemoryCache()->Contains(resource2));
  EXPECT_FALSE(GetMemoryCache()->Contains(resource1));

  FakeResource* resource3 =
      FakeResource::Create("http://test/resource", Resource::kRaw);
  GetMemoryCache()->Remove(resource2);
  GetMemoryCache()->Add(resource3);
  EXPECT_TRUE(GetMemoryCache()->Contains(resource3));
  EXPECT_FALSE(GetMemoryCache()->Contains(resource2));
}

TEST_F(MemoryCacheTest, ResourceMapIsolation) {
  FakeResource* resource1 =
      FakeResource::Create("http://test/resource", Resource::kRaw);
  GetMemoryCache()->Add(resource1);

  FakeResource* resource2 =
      FakeResource::Create("http://test/resource", Resource::kRaw);
  resource2->SetCacheIdentifier("foo");
  GetMemoryCache()->Add(resource2);
  EXPECT_TRUE(GetMemoryCache()->Contains(resource1));
  EXPECT_TRUE(GetMemoryCache()->Contains(resource2));

  const KURL url = KURL("http://test/resource");
  EXPECT_EQ(resource1, GetMemoryCache()->ResourceForURL(url));
  EXPECT_EQ(resource1, GetMemoryCache()->ResourceForURL(
                           url, GetMemoryCache()->DefaultCacheIdentifier()));
  EXPECT_EQ(resource2, GetMemoryCache()->ResourceForURL(url, "foo"));
  EXPECT_EQ(nullptr, GetMemoryCache()->ResourceForURL(NullURL()));

  FakeResource* resource3 =
      FakeResource::Create("http://test/resource", Resource::kRaw);
  resource3->SetCacheIdentifier("foo");
  GetMemoryCache()->Remove(resource2);
  GetMemoryCache()->Add(resource3);
  EXPECT_TRUE(GetMemoryCache()->Contains(resource1));
  EXPECT_FALSE(GetMemoryCache()->Contains(resource2));
  EXPECT_TRUE(GetMemoryCache()->Contains(resource3));

  HeapVector<Member<Resource>> resources =
      GetMemoryCache()->ResourcesForURL(url);
  EXPECT_EQ(2u, resources.size());

  GetMemoryCache()->EvictResources();
  EXPECT_FALSE(GetMemoryCache()->Contains(resource1));
  EXPECT_FALSE(GetMemoryCache()->Contains(resource3));
}

TEST_F(MemoryCacheTest, FragmentIdentifier) {
  const KURL url1 = KURL("http://test/resource#foo");
  FakeResource* resource = FakeResource::Create(url1, Resource::kRaw);
  GetMemoryCache()->Add(resource);
  EXPECT_TRUE(GetMemoryCache()->Contains(resource));

  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(url1));

  const KURL url2 = MemoryCache::RemoveFragmentIdentifierIfNeeded(url1);
  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(url2));
}

TEST_F(MemoryCacheTest, RemoveURLFromCache) {
  const KURL url1 = KURL("http://test/resource1");
  Persistent<FakeResource> resource1 =
      FakeResource::Create(url1, Resource::kRaw);
  GetMemoryCache()->Add(resource1);
  EXPECT_TRUE(GetMemoryCache()->Contains(resource1));

  GetMemoryCache()->RemoveURLFromCache(url1);
  EXPECT_FALSE(GetMemoryCache()->Contains(resource1));

  const KURL url2 = KURL("http://test/resource2#foo");
  FakeResource* resource2 = FakeResource::Create(url2, Resource::kRaw);
  GetMemoryCache()->Add(resource2);
  EXPECT_TRUE(GetMemoryCache()->Contains(resource2));

  GetMemoryCache()->RemoveURLFromCache(url2);
  EXPECT_FALSE(GetMemoryCache()->Contains(resource2));
}

}  // namespace blink
