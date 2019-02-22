// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {
namespace {

class MockClient final : public GarbageCollectedFinalized<MockClient>,
                         public ResourceLoadSchedulerClient {
  USING_GARBAGE_COLLECTED_MIXIN(MockClient);

 public:
  // A delegate that can be used to determine the order clients were run in.
  class MockClientDelegate {
   public:
    MockClientDelegate() = default;
    ~MockClientDelegate() = default;

    void NotifyRun(MockClient* client) { client_order_.push_back(client); }

    // The call order that hte clients ran in.
    const std::vector<MockClient*>& client_order() { return client_order_; }

   private:
    std::vector<MockClient*> client_order_;
  };

  ~MockClient() = default;

  void SetDelegate(MockClientDelegate* delegate) { delegate_ = delegate; }

  void Run() override {
    if (delegate_) {
      delegate_->NotifyRun(this);
    }
    EXPECT_FALSE(was_run_);
    was_run_ = true;
  }
  bool WasRun() { return was_run_; }

  void Trace(blink::Visitor* visitor) override {
    ResourceLoadSchedulerClient::Trace(visitor);
  }

 private:
  MockClientDelegate* delegate_;
  bool was_run_ = false;
};

class ResourceLoadSchedulerTest : public testing::Test {
 public:
  using ThrottleOption = ResourceLoadScheduler::ThrottleOption;
  void SetUp() override {
    DCHECK(RuntimeEnabledFeatures::ResourceLoadSchedulerEnabled());
    scheduler_ = ResourceLoadScheduler::Create(
        MockFetchContext::Create(MockFetchContext::kShouldNotLoadNewResource));
    Scheduler()->SetOutstandingLimitForTesting(1);
  }
  void TearDown() override { Scheduler()->Shutdown(); }

  ResourceLoadScheduler* Scheduler() { return scheduler_; }

  bool Release(ResourceLoadScheduler::ClientId client) {
    return Scheduler()->Release(
        client, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
        ResourceLoadScheduler::TrafficReportHints::InvalidInstance());
  }
  bool ReleaseAndSchedule(ResourceLoadScheduler::ClientId client) {
    return Scheduler()->Release(
        client, ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule,
        ResourceLoadScheduler::TrafficReportHints::InvalidInstance());
  }

 private:
  Persistent<ResourceLoadScheduler> scheduler_;
};

TEST_F(ResourceLoadSchedulerTest, StopStoppableRequest) {
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kStopped);
  // A request that disallows throttling should be queued.
  MockClient* client1 = new MockClient;
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_FALSE(client1->WasRun());

  // Another request that disallows throttling, but allows stopping should also
  // be queued.
  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kStoppable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_FALSE(client2->WasRun());

  // Another request that disallows throttling and stopping also should be run
  // even it makes the outstanding number reaches to the limit.
  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kCanNotBeStoppedOrThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_TRUE(client3->WasRun());

  // Call Release() with different options just in case.
  EXPECT_TRUE(Release(id1));
  EXPECT_TRUE(ReleaseAndSchedule(id2));
  EXPECT_TRUE(ReleaseAndSchedule(id3));

  // Should not succeed to call with the same ID twice.
  EXPECT_FALSE(Release(id1));

  // Should not succeed to call with the invalid ID or unused ID.
  EXPECT_FALSE(Release(ResourceLoadScheduler::kInvalidClientId));

  EXPECT_FALSE(Release(static_cast<ResourceLoadScheduler::ClientId>(774)));
}

TEST_F(ResourceLoadSchedulerTest, ThrottleThrottleableRequest) {
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kThrottled);

  Scheduler()->SetOutstandingLimitForTesting(0);
  // A request that allows throttling should be queued.
  MockClient* client1 = new MockClient;
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_FALSE(client1->WasRun());

  // Another request that disallows throttling also should be run even it makes
  // the outstanding number reaches to the limit.
  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kStoppable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_TRUE(client2->WasRun());

  // Another request that disallows stopping should be run even it makes the
  // outstanding number reaches to the limit.
  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kCanNotBeStoppedOrThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_TRUE(client3->WasRun());

  // Call Release() with different options just in case.
  EXPECT_TRUE(Release(id1));
  EXPECT_TRUE(ReleaseAndSchedule(id2));
  EXPECT_TRUE(ReleaseAndSchedule(id3));

  // Should not succeed to call with the same ID twice.
  EXPECT_FALSE(Release(id1));

  // Should not succeed to call with the invalid ID or unused ID.
  EXPECT_FALSE(Release(ResourceLoadScheduler::kInvalidClientId));

  EXPECT_FALSE(Release(static_cast<ResourceLoadScheduler::ClientId>(774)));
}

TEST_F(ResourceLoadSchedulerTest, Throttled) {
  // The first request should be ran synchronously.
  MockClient* client1 = new MockClient;
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_TRUE(client1->WasRun());

  // Another request should be throttled until the first request calls Release.
  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_FALSE(client2->WasRun());

  // Two more requests.
  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_FALSE(client3->WasRun());

  MockClient* client4 = new MockClient;
  ResourceLoadScheduler::ClientId id4 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client4, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id4);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id4);
  EXPECT_FALSE(client4->WasRun());

  // Call Release() to run the second request.
  EXPECT_TRUE(ReleaseAndSchedule(id1));
  EXPECT_TRUE(client2->WasRun());

  // Call Release() with kReleaseOnly should not run the third and the fourth
  // requests.
  EXPECT_TRUE(Release(id2));
  EXPECT_FALSE(client3->WasRun());
  EXPECT_FALSE(client4->WasRun());

  // Should be able to call Release() for a client that hasn't run yet. This
  // should run another scheduling to run the fourth request.
  EXPECT_TRUE(ReleaseAndSchedule(id3));
  EXPECT_TRUE(client4->WasRun());
}

TEST_F(ResourceLoadSchedulerTest, Unthrottle) {
  // Push three requests.
  MockClient* client1 = new MockClient;
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_TRUE(client1->WasRun());

  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_FALSE(client2->WasRun());

  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_FALSE(client3->WasRun());

  // Allows to pass all requests.
  Scheduler()->SetOutstandingLimitForTesting(3);
  EXPECT_TRUE(client2->WasRun());
  EXPECT_TRUE(client3->WasRun());

  // Release all.
  EXPECT_TRUE(Release(id3));
  EXPECT_TRUE(Release(id2));
  EXPECT_TRUE(Release(id1));
}

TEST_F(ResourceLoadSchedulerTest, Stopped) {
  // Push three requests.
  MockClient* client1 = new MockClient;
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_TRUE(client1->WasRun());

  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_FALSE(client2->WasRun());

  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_FALSE(client3->WasRun());

  // Setting outstanding_limit_ to 0 in ThrottlingState::kStopped, prevents
  // further requests.
  Scheduler()->SetOutstandingLimitForTesting(0);
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  // Calling Release() still does not run the second request.
  EXPECT_TRUE(ReleaseAndSchedule(id1));
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  // Release all.
  EXPECT_TRUE(Release(id3));
  EXPECT_TRUE(Release(id2));
}

TEST_F(ResourceLoadSchedulerTest, PriorityIsConsidered) {
  // Push three requests.
  MockClient* client1 = new MockClient;

  Scheduler()->SetOutstandingLimitForTesting(0);

  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLowest, 10 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLow, 1 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);

  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLow, 3 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);

  MockClient* client4 = new MockClient;
  ResourceLoadScheduler::ClientId id4 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client4, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kHigh, 0 /* intra_priority */,
                       &id4);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id4);

  EXPECT_FALSE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());
  EXPECT_TRUE(client4->WasRun());

  // Client 4 does not count against the limit as it was not delayable when it
  // was created.
  Scheduler()->SetOutstandingLimitForTesting(1);

  EXPECT_FALSE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_TRUE(client3->WasRun());
  EXPECT_TRUE(client4->WasRun());

  Scheduler()->SetOutstandingLimitForTesting(2);

  EXPECT_FALSE(client1->WasRun());
  EXPECT_TRUE(client2->WasRun());
  EXPECT_TRUE(client3->WasRun());
  EXPECT_TRUE(client4->WasRun());

  Scheduler()->SetOutstandingLimitForTesting(3);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_TRUE(client2->WasRun());
  EXPECT_TRUE(client3->WasRun());
  EXPECT_TRUE(client4->WasRun());

  // Release the rest.
  EXPECT_TRUE(Release(id4));
  EXPECT_TRUE(Release(id3));
  EXPECT_TRUE(Release(id2));
  EXPECT_TRUE(Release(id1));
}

TEST_F(ResourceLoadSchedulerTest, AllowedRequestsRunInPriorityOrder) {
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kStopped);
  Scheduler()->SetOutstandingLimitForTesting(0);

  MockClient::MockClientDelegate delegate;
  // Push two requests.
  MockClient* client1 = new MockClient;
  MockClient* client2 = new MockClient;

  client1->SetDelegate(&delegate);
  client2->SetDelegate(&delegate);

  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kStoppable,
                       ResourceLoadPriority::kLowest, 10 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kHigh, 1 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);

  EXPECT_FALSE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());

  Scheduler()->SetOutstandingLimitForTesting(1);

  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kThrottled);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_TRUE(client2->WasRun());

  // Verify high priority request ran first.
  std::vector<MockClient*> order = delegate.client_order();
  EXPECT_EQ(order[0], client2);
  EXPECT_EQ(order[1], client1);

  // Release all.
  EXPECT_TRUE(Release(id1));
  EXPECT_TRUE(Release(id2));
}

TEST_F(ResourceLoadSchedulerTest, StoppableRequestResumesWhenThrottled) {
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kStopped);
  // Push two requests.
  MockClient* client1 = new MockClient;

  Scheduler()->SetOutstandingLimitForTesting(0);

  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kStoppable,
                       ResourceLoadPriority::kLowest, 10 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kHigh, 1 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);

  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kStoppable,
                       ResourceLoadPriority::kLowest, 10 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);

  EXPECT_FALSE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kThrottled);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_TRUE(client3->WasRun());

  Scheduler()->SetOutstandingLimitForTesting(1);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_TRUE(client2->WasRun());
  EXPECT_TRUE(client3->WasRun());

  // Release all.
  EXPECT_TRUE(Release(id1));
  EXPECT_TRUE(Release(id2));
  EXPECT_TRUE(Release(id3));
}

TEST_F(ResourceLoadSchedulerTest, SetPriority) {
  // Start with the normal scheduling policy.
  Scheduler()->LoosenThrottlingPolicy();
  // Push three requests.
  MockClient* client1 = new MockClient;

  Scheduler()->SetOutstandingLimitForTesting(0);

  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLow, 5 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);

  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLow, 10 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);

  EXPECT_FALSE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  Scheduler()->SetPriority(id1, ResourceLoadPriority::kHigh, 0);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  Scheduler()->SetPriority(id3, ResourceLoadPriority::kLow, 2);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  Scheduler()->SetOutstandingLimitForTesting(2);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_TRUE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  // Release all.
  EXPECT_TRUE(Release(id3));
  EXPECT_TRUE(Release(id2));
  EXPECT_TRUE(Release(id1));
}

TEST_F(ResourceLoadSchedulerTest, LoosenThrottlingPolicy) {
  MockClient* client1 = new MockClient;

  Scheduler()->SetOutstandingLimitForTesting(0, 0);

  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  MockClient* client2 = new MockClient;
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);

  MockClient* client3 = new MockClient;
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);

  MockClient* client4 = new MockClient;
  ResourceLoadScheduler::ClientId id4 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client4, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id4);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id4);

  Scheduler()->SetPriority(id2, ResourceLoadPriority::kLow, 0);
  Scheduler()->SetPriority(id3, ResourceLoadPriority::kLow, 0);
  Scheduler()->SetPriority(id4, ResourceLoadPriority::kMedium, 0);

  // As the policy is |kTight|, |kMedium| is throttled.
  EXPECT_FALSE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());
  EXPECT_FALSE(client4->WasRun());

  Scheduler()->SetOutstandingLimitForTesting(0, 2);

  // MockFetchContext's initial scheduling policy is |kTight|, setting the
  // outstanding limit for the normal mode doesn't take effect.
  EXPECT_FALSE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());
  EXPECT_FALSE(client4->WasRun());

  // Now let's tighten the limit again.
  Scheduler()->SetOutstandingLimitForTesting(0, 0);

  // ...and change the scheduling policy to |kNormal|.
  Scheduler()->LoosenThrottlingPolicy();

  EXPECT_FALSE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());
  EXPECT_FALSE(client4->WasRun());

  Scheduler()->SetOutstandingLimitForTesting(0, 2);

  EXPECT_FALSE(client1->WasRun());
  EXPECT_TRUE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());
  EXPECT_TRUE(client4->WasRun());

  // Release all.
  EXPECT_TRUE(Release(id4));
  EXPECT_TRUE(Release(id3));
  EXPECT_TRUE(Release(id2));
  EXPECT_TRUE(Release(id1));
}

}  // namespace
}  // namespace blink
