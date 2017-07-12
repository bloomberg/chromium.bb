// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/ResourceLoadScheduler.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/loader/testing/MockFetchContext.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class MockClient final : public GarbageCollectedFinalized<MockClient>,
                         public ResourceLoadSchedulerClient {
  USING_GARBAGE_COLLECTED_MIXIN(MockClient);

 public:
  ~MockClient() {}

  void Run() override {
    EXPECT_FALSE(was_ran_);
    was_ran_ = true;
  }
  bool WasRan() { return was_ran_; }

  DEFINE_INLINE_VIRTUAL_TRACE() { ResourceLoadSchedulerClient::Trace(visitor); }

 private:
  bool was_ran_ = false;
};

class ResourceLoadSchedulerTest : public ::testing::Test {
 public:
  void SetUp() override {
    // TODO(toyoshim): blink_platform_unittests should enable experimental
    // runtime features by default.
    DCHECK(!RuntimeEnabledFeatures::ResourceLoadSchedulerEnabled());
    RuntimeEnabledFeatures::SetResourceLoadSchedulerEnabled(true);
    scheduler_ = ResourceLoadScheduler::Create(
        MockFetchContext::Create(MockFetchContext::kShouldNotLoadNewResource));
    scheduler()->SetOutstandingLimitForTesting(1);
  }
  void TearDown() override {
    scheduler()->Shutdown();
    RuntimeEnabledFeatures::SetResourceLoadSchedulerEnabled(false);
  }

  ResourceLoadScheduler* scheduler() { return scheduler_; }

 private:
  Persistent<ResourceLoadScheduler> scheduler_;
};

TEST_F(ResourceLoadSchedulerTest, Bypass) {
  // A request that disallows throttling should be ran synchronously.
  MockClient* client1 = new MockClient;
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(
      client1, ResourceLoadScheduler::ThrottleOption::kCanNotBeThrottled, &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_TRUE(client1->WasRan());

  // Another request that disallows throttling also should be ran even it makes
  // the outstanding number reaches to the limit.
  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(
      client2, ResourceLoadScheduler::ThrottleOption::kCanNotBeThrottled, &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_TRUE(client2->WasRan());

  // Call Release() with different options just in case.
  EXPECT_TRUE(scheduler()->Release(
      id1, ResourceLoadScheduler::ReleaseOption::kReleaseOnly));
  EXPECT_TRUE(scheduler()->Release(
      id2, ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule));

  // Should not succeed to call with the same ID twice.
  EXPECT_FALSE(scheduler()->Release(
      id1, ResourceLoadScheduler::ReleaseOption::kReleaseOnly));

  // Should not succeed to call with the invalid ID or unused ID.
  EXPECT_FALSE(
      scheduler()->Release(ResourceLoadScheduler::kInvalidClientId,
                           ResourceLoadScheduler::ReleaseOption::kReleaseOnly));

  EXPECT_FALSE(
      scheduler()->Release(static_cast<ResourceLoadScheduler::ClientId>(774),
                           ResourceLoadScheduler::ReleaseOption::kReleaseOnly));
}

TEST_F(ResourceLoadSchedulerTest, Throttled) {
  // The first request should be ran synchronously.
  MockClient* client1 = new MockClient;
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(
      client1, ResourceLoadScheduler::ThrottleOption::kCanBeThrottled, &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_TRUE(client1->WasRan());

  // Another request should be throttled until the first request calls Release.
  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(
      client2, ResourceLoadScheduler::ThrottleOption::kCanBeThrottled, &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_FALSE(client2->WasRan());

  // Two more requests.
  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(
      client3, ResourceLoadScheduler::ThrottleOption::kCanBeThrottled, &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_FALSE(client3->WasRan());

  MockClient* client4 = new MockClient;
  ResourceLoadScheduler::ClientId id4 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(
      client4, ResourceLoadScheduler::ThrottleOption::kCanBeThrottled, &id4);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id4);
  EXPECT_FALSE(client4->WasRan());

  // Call Release() to run the second request.
  EXPECT_TRUE(scheduler()->Release(
      id1, ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule));
  EXPECT_TRUE(client2->WasRan());

  // Call Release() with kReleaseOnly should not run the third and the fourth
  // requests.
  EXPECT_TRUE(scheduler()->Release(
      id2, ResourceLoadScheduler::ReleaseOption::kReleaseOnly));
  EXPECT_FALSE(client3->WasRan());
  EXPECT_FALSE(client4->WasRan());

  // Should be able to call Release() for a client that hasn't run yet. This
  // should run another scheduling to run the fourth request.
  EXPECT_TRUE(scheduler()->Release(
      id3, ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule));
  EXPECT_TRUE(client4->WasRan());
}

TEST_F(ResourceLoadSchedulerTest, Unthrottle) {
  // Push three requests.
  MockClient* client1 = new MockClient;
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(
      client1, ResourceLoadScheduler::ThrottleOption::kCanBeThrottled, &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_TRUE(client1->WasRan());

  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(
      client2, ResourceLoadScheduler::ThrottleOption::kCanBeThrottled, &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_FALSE(client2->WasRan());

  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(
      client3, ResourceLoadScheduler::ThrottleOption::kCanBeThrottled, &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_FALSE(client3->WasRan());

  // Allows to pass all requests.
  scheduler()->SetOutstandingLimitForTesting(3);
  EXPECT_TRUE(client2->WasRan());
  EXPECT_TRUE(client3->WasRan());

  // Release all.
  EXPECT_TRUE(scheduler()->Release(
      id3, ResourceLoadScheduler::ReleaseOption::kReleaseOnly));
  EXPECT_TRUE(scheduler()->Release(
      id2, ResourceLoadScheduler::ReleaseOption::kReleaseOnly));
  EXPECT_TRUE(scheduler()->Release(
      id1, ResourceLoadScheduler::ReleaseOption::kReleaseOnly));
}

}  // namespace
}  // namespace blink
