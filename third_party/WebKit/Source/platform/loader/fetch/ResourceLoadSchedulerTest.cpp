// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/ResourceLoadScheduler.h"

#include "platform/loader/testing/MockFetchContext.h"
#include "platform/runtime_enabled_features.h"
#include "platform/testing/TestingPlatformSupport.h"
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

  void Trace(blink::Visitor* visitor) override {
    ResourceLoadSchedulerClient::Trace(visitor);
  }

 private:
  bool was_ran_ = false;
};

class ResourceLoadSchedulerTest : public ::testing::Test {
 public:
  using ThrottleOption = ResourceLoadScheduler::ThrottleOption;
  void SetUp() override {
    DCHECK(RuntimeEnabledFeatures::ResourceLoadSchedulerEnabled());
    scheduler_ = ResourceLoadScheduler::Create(
        MockFetchContext::Create(MockFetchContext::kShouldNotLoadNewResource));
    scheduler()->SetOutstandingLimitForTesting(1);
  }
  void TearDown() override {
    scheduler()->Shutdown();
  }

  ResourceLoadScheduler* scheduler() { return scheduler_; }

 private:
  Persistent<ResourceLoadScheduler> scheduler_;
};

class RendererSideResourceSchedulerTest : public ::testing::Test {
 public:
  using ThrottleOption = ResourceLoadScheduler::ThrottleOption;
  class TestingPlatformSupport : public ::blink::TestingPlatformSupport {
   public:
    bool IsRendererSideResourceSchedulerEnabled() const override {
      return true;
    }
  };

  void SetUp() override {
    DCHECK(RuntimeEnabledFeatures::ResourceLoadSchedulerEnabled());
    scheduler_ = ResourceLoadScheduler::Create(
        MockFetchContext::Create(MockFetchContext::kShouldNotLoadNewResource));
    scheduler()->SetOutstandingLimitForTesting(1);
  }
  void TearDown() override { scheduler()->Shutdown(); }

  ResourceRequest CreateRequest(ResourceLoadPriority priority) const {
    ResourceRequest request(KURL("http://www.example.com/"));
    request.SetPriority(priority);
    return request;
  }

  ResourceLoadScheduler* scheduler() { return scheduler_; }

 private:
  ScopedTestingPlatformSupport<TestingPlatformSupport>
      testing_platform_support_;
  Persistent<ResourceLoadScheduler> scheduler_;
};

TEST_F(ResourceLoadSchedulerTest, Bypass) {
  // A request that disallows throttling should be ran synchronously.
  MockClient* client1 = new MockClient;
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client1, ThrottleOption::kCanNotBeThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_TRUE(client1->WasRan());

  // Another request that disallows throttling also should be ran even it makes
  // the outstanding number reaches to the limit.
  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client2, ThrottleOption::kCanNotBeThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_TRUE(client2->WasRan());

  // Call Release() with different options just in case.
  EXPECT_TRUE(scheduler()->Release(
      id1, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(scheduler()->Release(
      id2, ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));

  // Should not succeed to call with the same ID twice.
  EXPECT_FALSE(scheduler()->Release(
      id1, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));

  // Should not succeed to call with the invalid ID or unused ID.
  EXPECT_FALSE(scheduler()->Release(
      ResourceLoadScheduler::kInvalidClientId,
      ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));

  EXPECT_FALSE(scheduler()->Release(
      static_cast<ResourceLoadScheduler::ClientId>(774),
      ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
}

TEST_F(ResourceLoadSchedulerTest, Throttled) {
  // The first request should be ran synchronously.
  MockClient* client1 = new MockClient;
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client1, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_TRUE(client1->WasRan());

  // Another request should be throttled until the first request calls Release.
  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client2, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_FALSE(client2->WasRan());

  // Two more requests.
  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client3, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_FALSE(client3->WasRan());

  MockClient* client4 = new MockClient;
  ResourceLoadScheduler::ClientId id4 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client4, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id4);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id4);
  EXPECT_FALSE(client4->WasRan());

  // Call Release() to run the second request.
  EXPECT_TRUE(scheduler()->Release(
      id1, ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(client2->WasRan());

  // Call Release() with kReleaseOnly should not run the third and the fourth
  // requests.
  EXPECT_TRUE(scheduler()->Release(
      id2, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_FALSE(client3->WasRan());
  EXPECT_FALSE(client4->WasRan());

  // Should be able to call Release() for a client that hasn't run yet. This
  // should run another scheduling to run the fourth request.
  EXPECT_TRUE(scheduler()->Release(
      id3, ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(client4->WasRan());
}

TEST_F(ResourceLoadSchedulerTest, Unthrottle) {
  // Push three requests.
  MockClient* client1 = new MockClient;
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client1, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_TRUE(client1->WasRan());

  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client2, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_FALSE(client2->WasRan());

  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client3, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_FALSE(client3->WasRan());

  // Allows to pass all requests.
  scheduler()->SetOutstandingLimitForTesting(3);
  EXPECT_TRUE(client2->WasRan());
  EXPECT_TRUE(client3->WasRan());

  // Release all.
  EXPECT_TRUE(scheduler()->Release(
      id3, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(scheduler()->Release(
      id2, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(scheduler()->Release(
      id1, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
}

TEST_F(ResourceLoadSchedulerTest, Stopped) {
  // Push three requests.
  MockClient* client1 = new MockClient;
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client1, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_TRUE(client1->WasRan());

  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client2, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_FALSE(client2->WasRan());

  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client3, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_FALSE(client3->WasRan());

  // Setting outstanding_limit_ to 0 in ThrottlingState::kStopped, prevents
  // further requests.
  scheduler()->SetOutstandingLimitForTesting(0);
  EXPECT_FALSE(client2->WasRan());
  EXPECT_FALSE(client3->WasRan());

  // Calling Release() still does not run the second request.
  EXPECT_TRUE(scheduler()->Release(
      id1, ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_FALSE(client2->WasRan());
  EXPECT_FALSE(client3->WasRan());

  // Release all.
  EXPECT_TRUE(scheduler()->Release(
      id3, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(scheduler()->Release(
      id2, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
}

TEST_F(ResourceLoadSchedulerTest, PriotrityIsNotConsidered) {
  // Push three requests.
  MockClient* client1 = new MockClient;

  scheduler()->SetOutstandingLimitForTesting(0);

  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client1, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kLowest, 10 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client2, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kLow, 1 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);

  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client3, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kLow, 3 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);

  EXPECT_FALSE(client1->WasRan());
  EXPECT_FALSE(client2->WasRan());
  EXPECT_FALSE(client3->WasRan());

  scheduler()->SetOutstandingLimitForTesting(1);

  EXPECT_TRUE(client1->WasRan());
  EXPECT_FALSE(client2->WasRan());
  EXPECT_FALSE(client3->WasRan());

  scheduler()->SetOutstandingLimitForTesting(2);

  EXPECT_TRUE(client1->WasRan());
  EXPECT_TRUE(client2->WasRan());
  EXPECT_FALSE(client3->WasRan());

  // Release all.
  EXPECT_TRUE(scheduler()->Release(
      id3, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(scheduler()->Release(
      id2, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(scheduler()->Release(
      id1, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
}

TEST_F(RendererSideResourceSchedulerTest, PriotrityIsConsidered) {
  // Push three requests.
  MockClient* client1 = new MockClient;

  scheduler()->SetOutstandingLimitForTesting(0);

  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client1, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kLowest, 10 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client2, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kLow, 1 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);

  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client3, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kLow, 3 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);

  MockClient* client4 = new MockClient;
  ResourceLoadScheduler::ClientId id4 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client4, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kHigh, 0 /* intra_priority */,
                       &id4);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id4);

  EXPECT_FALSE(client1->WasRan());
  EXPECT_FALSE(client2->WasRan());
  EXPECT_FALSE(client3->WasRan());
  EXPECT_TRUE(client4->WasRan());

  scheduler()->SetOutstandingLimitForTesting(2);

  EXPECT_FALSE(client1->WasRan());
  EXPECT_FALSE(client2->WasRan());
  EXPECT_TRUE(client3->WasRan());
  EXPECT_TRUE(client4->WasRan());

  scheduler()->SetOutstandingLimitForTesting(3);

  EXPECT_FALSE(client1->WasRan());
  EXPECT_TRUE(client2->WasRan());
  EXPECT_TRUE(client3->WasRan());
  EXPECT_TRUE(client4->WasRan());

  // Release all.
  EXPECT_TRUE(scheduler()->Release(
      id4, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(scheduler()->Release(
      id3, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(scheduler()->Release(
      id2, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(scheduler()->Release(
      id1, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
}

TEST_F(RendererSideResourceSchedulerTest, IsThrottablePriority) {
  EXPECT_TRUE(
      scheduler()->IsThrottablePriority(ResourceLoadPriority::kVeryLow));
  EXPECT_TRUE(scheduler()->IsThrottablePriority(ResourceLoadPriority::kLow));
  EXPECT_TRUE(scheduler()->IsThrottablePriority(ResourceLoadPriority::kMedium));
  EXPECT_FALSE(scheduler()->IsThrottablePriority(ResourceLoadPriority::kHigh));
  EXPECT_FALSE(
      scheduler()->IsThrottablePriority(ResourceLoadPriority::kVeryHigh));

  scheduler()->LoosenThrottlingPolicy();

  EXPECT_TRUE(
      scheduler()->IsThrottablePriority(ResourceLoadPriority::kVeryLow));
  EXPECT_TRUE(scheduler()->IsThrottablePriority(ResourceLoadPriority::kLow));
  EXPECT_FALSE(
      scheduler()->IsThrottablePriority(ResourceLoadPriority::kMedium));
  EXPECT_FALSE(scheduler()->IsThrottablePriority(ResourceLoadPriority::kHigh));
  EXPECT_FALSE(
      scheduler()->IsThrottablePriority(ResourceLoadPriority::kVeryHigh));
}

TEST_F(RendererSideResourceSchedulerTest, SetPriority) {
  // Start with the normal scheduling policy.
  scheduler()->LoosenThrottlingPolicy();
  // Push three requests.
  MockClient* client1 = new MockClient;

  scheduler()->SetOutstandingLimitForTesting(0);

  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client1, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client2, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kLow, 5 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);

  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client3, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kLow, 10 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);

  EXPECT_FALSE(client1->WasRan());
  EXPECT_FALSE(client2->WasRan());
  EXPECT_FALSE(client3->WasRan());

  scheduler()->SetPriority(id1, ResourceLoadPriority::kHigh, 0);

  EXPECT_TRUE(client1->WasRan());
  EXPECT_FALSE(client2->WasRan());
  EXPECT_FALSE(client3->WasRan());

  scheduler()->SetPriority(id3, ResourceLoadPriority::kLow, 2);

  EXPECT_TRUE(client1->WasRan());
  EXPECT_FALSE(client2->WasRan());
  EXPECT_FALSE(client3->WasRan());

  scheduler()->SetOutstandingLimitForTesting(2);

  EXPECT_TRUE(client1->WasRan());
  EXPECT_TRUE(client2->WasRan());
  EXPECT_FALSE(client3->WasRan());

  // Release all.
  EXPECT_TRUE(scheduler()->Release(
      id3, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(scheduler()->Release(
      id2, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(scheduler()->Release(
      id1, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
}

TEST_F(RendererSideResourceSchedulerTest, LoosenThrottlingPolicy) {
  MockClient* client1 = new MockClient;

  scheduler()->SetOutstandingLimitForTesting(0, 0);

  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client1, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client2, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);

  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client3, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);

  MockClient* client4 = new MockClient;
  ResourceLoadScheduler::ClientId id4 = ResourceLoadScheduler::kInvalidClientId;
  scheduler()->Request(client4, ThrottleOption::kCanBeThrottled,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id4);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id4);

  scheduler()->SetPriority(id2, ResourceLoadPriority::kLow, 0);
  scheduler()->SetPriority(id3, ResourceLoadPriority::kLow, 0);
  scheduler()->SetPriority(id4, ResourceLoadPriority::kMedium, 0);

  // As the policy is |kTight|, |kMedium| is throttled.
  EXPECT_FALSE(client1->WasRan());
  EXPECT_FALSE(client2->WasRan());
  EXPECT_FALSE(client3->WasRan());
  EXPECT_FALSE(client4->WasRan());

  scheduler()->SetOutstandingLimitForTesting(0, 2);

  // MockFetchContext's initial scheduling policy is |kTight|, setting the
  // outstanding limit for the normal mode doesn't take effect.
  EXPECT_FALSE(client1->WasRan());
  EXPECT_FALSE(client2->WasRan());
  EXPECT_FALSE(client3->WasRan());
  EXPECT_FALSE(client4->WasRan());

  // Now let's tighten the limit again.
  scheduler()->SetOutstandingLimitForTesting(0, 0);

  // ...and change the scheduling policy to |kNormal|, where priority >=
  // |kMedium| requests are not throttled.
  scheduler()->LoosenThrottlingPolicy();

  EXPECT_FALSE(client1->WasRan());
  EXPECT_FALSE(client2->WasRan());
  EXPECT_FALSE(client3->WasRan());
  EXPECT_TRUE(client4->WasRan());

  scheduler()->SetOutstandingLimitForTesting(0, 2);

  EXPECT_FALSE(client1->WasRan());
  EXPECT_TRUE(client2->WasRan());
  EXPECT_FALSE(client3->WasRan());
  EXPECT_TRUE(client4->WasRan());

  // Release all.
  EXPECT_TRUE(scheduler()->Release(
      id3, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(scheduler()->Release(
      id4, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(scheduler()->Release(
      id2, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
  EXPECT_TRUE(scheduler()->Release(
      id1, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
      ResourceLoadScheduler::TrafficReportHints::InvalidInstance()));
}

}  // namespace
}  // namespace blink
