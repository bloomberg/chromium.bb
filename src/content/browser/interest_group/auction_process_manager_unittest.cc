// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_process_manager.h"

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

// Alias constants to improve readability.
const size_t kMaxSellerProcesses = AuctionProcessManager::kMaxSellerProcesses;
const size_t kMaxBidderProcesses = AuctionProcessManager::kMaxBidderProcesses;

class TestAuctionProcessManager
    : public AuctionProcessManager,
      public auction_worklet::mojom::AuctionWorkletService {
 public:
  TestAuctionProcessManager() = default;

  TestAuctionProcessManager(const TestAuctionProcessManager&) = delete;
  const TestAuctionProcessManager& operator=(const TestAuctionProcessManager&) =
      delete;

  ~TestAuctionProcessManager() override = default;

  void LoadBidderWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          bidder_worklet_receiver,
      bool pause_for_debugger_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      const GURL& script_source_url,
      const absl::optional<GURL>& bidding_wasm_helper_url,
      const absl::optional<GURL>& trusted_bidding_signals_url,
      const url::Origin& top_window_origin) override {
    NOTREACHED();
  }

  void LoadSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          seller_worklet,
      bool should_pause_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      const GURL& script_source_url,
      const absl::optional<GURL>& trusted_scoring_signals_url,
      const url::Origin& top_window_origin) override {
    NOTREACHED();
  }

  size_t NumReceivers() {
    // Flush so that any closed pipes are removed. No need to worry about
    // pending creation requests, since this class is called into directly,
    // rather than over a Mojo pipe.
    receiver_set_.FlushForTesting();
    return receiver_set_.size();
  }

  void ClosePipes() {
    receiver_set_.Clear();
    // No wait to flush a closed pipe from the end that was closed. Run until
    // the other side has noticed the pipe was closed instead.
    base::RunLoop().RunUntilIdle();
  }

 private:
  void LaunchProcess(
      mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
          auction_worklet_service_receiver,
      const std::string& display_name) override {
    receiver_set_.Add(this, std::move(auction_worklet_service_receiver));
  }

  mojo::ReceiverSet<auction_worklet::mojom::AuctionWorkletService>
      receiver_set_;
};

class AuctionProcessManagerTest
    : public testing::TestWithParam<AuctionProcessManager::WorkletType> {
 protected:
  // Request a worklet service and expect the request to complete synchronously.
  // There's no async version, since async calls are only triggered by deleting
  // another handle.
  std::unique_ptr<AuctionProcessManager::ProcessHandle>
  GetServiceOfTypeExpectSuccess(AuctionProcessManager::WorkletType worklet_type,
                                const url::Origin& origin) {
    auto process_handle =
        std::make_unique<AuctionProcessManager::ProcessHandle>();
    EXPECT_TRUE(auction_process_manager_.RequestWorkletService(
        worklet_type, origin, process_handle.get(), NeverInvokedClosure()));
    EXPECT_TRUE(process_handle->GetService());
    return process_handle;
  }

  // Requests a process of type GetParam().
  std::unique_ptr<AuctionProcessManager::ProcessHandle> GetServiceExpectSuccess(
      const url::Origin& origin) {
    return GetServiceOfTypeExpectSuccess(GetParam(), origin);
  }

  // Returns the maximum number of processes of type GetParam().
  size_t GetMaxProcesses() const {
    switch (GetParam()) {
      case AuctionProcessManager::WorkletType::kSeller:
        return kMaxSellerProcesses;
      case AuctionProcessManager::WorkletType::kBidder:
        return kMaxBidderProcesses;
    }
  }

  // Returns the number of pending requests of GetParam() type.
  size_t GetPendingRequestsOfParamType() const {
    switch (GetParam()) {
      case AuctionProcessManager::WorkletType::kSeller:
        return auction_process_manager_.GetPendingSellerRequestsForTesting();
      case AuctionProcessManager::WorkletType::kBidder:
        return auction_process_manager_.GetPendingBidderRequestsForTesting();
    }
  }

  base::OnceClosure NeverInvokedClosure() {
    return base::BindOnce(
        []() { ADD_FAILURE() << "This should not be called"; });
  }

  base::test::TaskEnvironment task_environment_;
  TestAuctionProcessManager auction_process_manager_;

  const url::Origin kOriginA = url::Origin::Create(GURL("https://a.test"));
  const url::Origin kOriginB = url::Origin::Create(GURL("https://b.test"));
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AuctionProcessManagerTest,
    testing::Values(AuctionProcessManager::WorkletType::kSeller,
                    AuctionProcessManager::WorkletType::kBidder));

TEST_P(AuctionProcessManagerTest, Basic) {
  auto seller = GetServiceExpectSuccess(kOriginA);
  EXPECT_TRUE(seller->GetService());
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());
}

// Make sure requests for different origins don't share processes, nor do
// sellers and bidders.
//
// This test doesn't use the parameterization, but using TEST_F() for a single
// test would require another test fixture, and so would add more complexity
// than it's worth, for only a single unit test.
TEST_P(AuctionProcessManagerTest, MultipleRequestsForDifferentProcesses) {
  auto seller_a = GetServiceOfTypeExpectSuccess(
      AuctionProcessManager::WorkletType::kSeller, kOriginA);
  auto seller_b = GetServiceOfTypeExpectSuccess(
      AuctionProcessManager::WorkletType::kSeller, kOriginB);
  auto buyer_a = GetServiceOfTypeExpectSuccess(
      AuctionProcessManager::WorkletType::kBidder, kOriginA);
  auto buyer_b = GetServiceOfTypeExpectSuccess(
      AuctionProcessManager::WorkletType::kBidder, kOriginB);

  EXPECT_EQ(4u, auction_process_manager_.NumReceivers());
  EXPECT_NE(seller_a->GetService(), seller_b->GetService());
  EXPECT_NE(seller_a->GetService(), buyer_a->GetService());
  EXPECT_NE(seller_a->GetService(), buyer_b->GetService());
  EXPECT_NE(seller_b->GetService(), buyer_a->GetService());
  EXPECT_NE(seller_b->GetService(), buyer_b->GetService());
  EXPECT_NE(buyer_a->GetService(), buyer_b->GetService());
}

TEST_P(AuctionProcessManagerTest, MultipleRequestsForSameProcess) {
  // Request 3 processes of the same type for the same origin. All requests
  // should get the same process.
  auto process_a1 = GetServiceExpectSuccess(kOriginA);
  EXPECT_TRUE(process_a1->GetService());
  auto process_a2 = GetServiceExpectSuccess(kOriginA);
  EXPECT_EQ(process_a1->GetService(), process_a2->GetService());
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());
  auto process_a3 = GetServiceExpectSuccess(kOriginA);
  EXPECT_EQ(process_a1->GetService(), process_a3->GetService());
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());

  // Request process of the other type with the same origin. It should get a
  // different process.
  std::unique_ptr<AuctionProcessManager::ProcessHandle> other_process_a1;
  switch (GetParam()) {
    case AuctionProcessManager::WorkletType::kSeller:
      other_process_a1 = GetServiceOfTypeExpectSuccess(
          AuctionProcessManager::WorkletType::kBidder, kOriginA);
      break;
    case AuctionProcessManager::WorkletType::kBidder:
      other_process_a1 = GetServiceOfTypeExpectSuccess(
          AuctionProcessManager::WorkletType::kSeller, kOriginA);
      break;
  }
  EXPECT_EQ(2u, auction_process_manager_.NumReceivers());
  EXPECT_NE(process_a1->GetService(), other_process_a1->GetService());
}

// Test requesting and releasing worklet processes, exceeding the limit. This
// test does not cover the case of multiple requests sharing the same process,
// which is covered by the next test.
TEST_P(AuctionProcessManagerTest, LimitExceeded) {
  // The list of operations below assumes at least 3 processes are allowed at
  // once.
  CHECK_GE(GetMaxProcesses(), 3u);

  // Operations applied to the process manager. All requests use unique origins,
  // so no need to specify that.
  struct Operation {
    enum class Op {
      // Request the specified number of handle. If there are less than
      // GetMaxProcesses() handles already, expects a process to be immediately
      // assigned. All requests use different origins from every other request.
      kRequestHandles,

      // Destroy a handle with the given index. If the index is less than
      // GetMaxProcesses(), then expect a ProcessHandle to have its callback
      // invoked, if there are more than GetMaxProcesses() already.
      kDestroyHandle,

      // Same as destroy handle, but additionally destroys the next handle that
      // would have been assigned the next available process slot, and makes
      // sure the handle after that one gets a process instead.
      kDestroyHandleAndNextInQueue,
    };

    Op op;

    // Number of handles to request for kRequestHandles operations.
    absl::optional<size_t> num_handles;

    // Used for kDestroyHandle and kDestroyHandleAndNextInQueue operations.
    absl::optional<size_t> index;

    // The number of total handles expected after this operation. This can be
    // inferred by sum of requested handles requests less handles destroyed
    // handles, but having it explcitly in the struct makes sure the test cases
    // are testing what they're expected to.
    size_t expected_total_handles;
  };

  const Operation kOperationList[] = {
      {Operation::Op::kRequestHandles, GetMaxProcesses() /* num_handles*/,
       absl::nullopt /* index */,
       GetMaxProcesses() /* expected_total_handles */},

      // Check destroying intermediate, last, and first handle when there are no
      // queued requests. Keep exactly GetMaxProcesses() requests, to ensure
      // there are in fact first, last, and intermediate requests (as long as
      // GetMaxProcesses() is at least 3).
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       1u /* index */, GetMaxProcesses() - 1 /* expected_total_handles */},
      {Operation::Op::kRequestHandles, 1 /* num_handles*/,
       absl::nullopt /* index */,
       GetMaxProcesses() /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       0u /* index */, GetMaxProcesses() - 1 /* expected_total_handles */},
      {Operation::Op::kRequestHandles, 1 /* num_handles*/,
       absl::nullopt /* index */,
       GetMaxProcesses() /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       GetMaxProcesses() - 1 /* index */,
       GetMaxProcesses() - 1 /* expected_total_handles */},
      {Operation::Op::kRequestHandles, 1 /* num_handles*/,
       absl::nullopt /* index */,
       GetMaxProcesses() /* expected_total_handles */},

      // Queue 3 more requests, but delete the last and first of them, to test
      // deleting queued requests.
      {Operation::Op::kRequestHandles, 3 /* num_handles*/,
       absl::nullopt /* index */,
       GetMaxProcesses() + 3 /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       GetMaxProcesses() /* index */,
       GetMaxProcesses() + 2 /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       GetMaxProcesses() + 1 /* index */,
       GetMaxProcesses() + 1 /* expected_total_handles */},

      // Request 4 more processes.
      {Operation::Op::kRequestHandles, 4 /* num_handles*/,
       absl::nullopt /* index */,
       GetMaxProcesses() + 5 /* expected_total_handles */},

      // Destroy the first handle and the first pending in the queue immediately
      // afterwards. The next pending request should get a process.
      {Operation::Op::kDestroyHandleAndNextInQueue,
       absl::nullopt /* num_handles*/, 0u /* index */,
       GetMaxProcesses() + 3 /* expected_total_handles */},

      // Destroy three more requests that have been asssigned processes, being
      // sure to destroy the first, last, and some request request with nether,
      // amongst requests with assigned processes.
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       GetMaxProcesses() - 1 /* index */,
       GetMaxProcesses() + 2 /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       0u /* index */, GetMaxProcesses() + 1 /* expected_total_handles */},
      {Operation::Op::kDestroyHandle, absl::nullopt /* num_handles*/,
       1u /* index */, GetMaxProcesses() /* expected_total_handles */},
  };

  struct ProcessHandleData {
    std::unique_ptr<AuctionProcessManager::ProcessHandle> process_handle =
        std::make_unique<AuctionProcessManager::ProcessHandle>();
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  };

  std::vector<ProcessHandleData> data;

  // Used to create distinct origins for each handle
  int num_origins = 0;
  for (const auto& operation : kOperationList) {
    switch (operation.op) {
      case Operation::Op::kRequestHandles:
        for (size_t i = 0; i < *operation.num_handles; ++i) {
          size_t original_size = data.size();
          data.emplace_back(ProcessHandleData());
          url::Origin distinct_origin = url::Origin::Create(
              GURL(base::StringPrintf("https://%i.test", ++num_origins)));
          ASSERT_EQ(
              original_size < GetMaxProcesses(),
              auction_process_manager_.RequestWorkletService(
                  GetParam(), distinct_origin, data.back().process_handle.get(),
                  data.back().run_loop->QuitClosure()));
        }
        break;

      case Operation::Op::kDestroyHandle: {
        size_t original_size = data.size();

        ASSERT_GT(data.size(), *operation.index);
        data.erase(data.begin() + *operation.index);
        // If destroying one of the first GetMaxProcesses() handles, and
        // there were more than GetMaxProcesses() handles before, the
        // first of the handles waiting on a process should get a process.
        if (*operation.index < GetMaxProcesses() &&
            original_size > GetMaxProcesses()) {
          data[GetMaxProcesses() - 1].run_loop->Run();
          EXPECT_TRUE(data[GetMaxProcesses() - 1].process_handle->GetService());
        }
        break;
      }

      case Operation::Op::kDestroyHandleAndNextInQueue: {
        ASSERT_GT(data.size(), *operation.index);
        ASSERT_GT(data.size(), GetMaxProcesses() + 1);

        data.erase(data.begin() + *operation.index);
        data.erase(data.begin() + GetMaxProcesses());
        data[GetMaxProcesses() - 1].run_loop->Run();
        EXPECT_TRUE(data[GetMaxProcesses() - 1].process_handle->GetService());
        break;
      }
    }

    EXPECT_EQ(operation.expected_total_handles, data.size());

    // The first GetMaxProcesses() ProcessHandles should all have
    // assigned processes, which should all be distinct.
    for (size_t i = 0; i < data.size() && i < GetMaxProcesses(); ++i) {
      EXPECT_TRUE(data[i].process_handle->GetService());
      for (size_t j = 0; j < i; ++j) {
        EXPECT_NE(data[i].process_handle->GetService(),
                  data[j].process_handle->GetService());
      }
    }

    // Make sure all pending tasks have been run.
    base::RunLoop().RunUntilIdle();

    // All other requests should not have been assigned processes yet.
    for (size_t i = GetMaxProcesses(); i < data.size(); ++i) {
      EXPECT_FALSE(data[i].run_loop->AnyQuitCalled());
      EXPECT_FALSE(data[i].process_handle->GetService());
    }
  }
}

// Check the process sharing logic - specifically, that requests share processes
// when origins match, and that handles that share a process only count once
// towrads the process limit the process limit.
TEST_P(AuctionProcessManagerTest, ProcessSharing) {
  // This test assumes GetMaxProcesses() is greater than 1.
  DCHECK_GT(GetMaxProcesses(), 1u);

  // Make 2*GetMaxProcesses() requests for each of GetMaxProcesses() different
  // origins. All requests should succeed immediately.
  std::vector<std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>>>
      processes(GetMaxProcesses());
  for (size_t origin_index = 0; origin_index < GetMaxProcesses();
       ++origin_index) {
    url::Origin origin = url::Origin::Create(
        GURL(base::StringPrintf("https://%zu.test", origin_index)));
    for (size_t i = 0; i < 2 * GetMaxProcesses(); ++i) {
      processes[origin_index].emplace_back(GetServiceExpectSuccess(origin));
      // All requests for the same origin share a process.
      EXPECT_EQ(processes[origin_index].back()->GetService(),
                processes[origin_index].front()->GetService());
      EXPECT_EQ(origin_index + 1, auction_process_manager_.NumReceivers());
    }

    // Each origin should have a different process.
    for (size_t origin_index2 = 0; origin_index2 < origin_index;
         ++origin_index2) {
      EXPECT_NE(processes[origin_index].front()->GetService(),
                processes[origin_index2].front()->GetService());
    }
  }

  // Make two process requests for kOriginA and one one for kOriginB, which
  // should all be blocked due to the process limit being reached.

  base::RunLoop run_loop_delayed_a1;
  auto process_delayed_a1 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      GetParam(), kOriginA, process_delayed_a1.get(),
      run_loop_delayed_a1.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_a1.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_a1->GetService());
  EXPECT_EQ(GetMaxProcesses(), auction_process_manager_.NumReceivers());

  base::RunLoop run_loop_delayed_a2;
  auto process_delayed_a2 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      GetParam(), kOriginA, process_delayed_a2.get(),
      run_loop_delayed_a2.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_a2.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_a2->GetService());
  EXPECT_EQ(GetMaxProcesses(), auction_process_manager_.NumReceivers());

  base::RunLoop run_loop_delayed_b;
  auto process_delayed_b =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      GetParam(), kOriginB, process_delayed_b.get(),
      run_loop_delayed_b.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_b->GetService());
  EXPECT_EQ(GetMaxProcesses(), auction_process_manager_.NumReceivers());

  // Release processes for first origin one at a time, until only one is left.
  // The pending requests for kOriginA and kOriginB should remain stalled.
  while (processes[0].size() > 1u) {
    processes[0].pop_front();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(run_loop_delayed_a1.AnyQuitCalled());
    EXPECT_FALSE(process_delayed_a1->GetService());
    EXPECT_FALSE(run_loop_delayed_a2.AnyQuitCalled());
    EXPECT_FALSE(process_delayed_a2->GetService());
    EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
    EXPECT_FALSE(process_delayed_b->GetService());
    EXPECT_EQ(GetMaxProcesses(), auction_process_manager_.NumReceivers());
  }

  // Remove the final process for the first origin. It should queue a callback
  // to resume the kOriginA requests (prioritized alphabetically), but nothing
  // should happen until the callbacks are invoked.
  processes[0].pop_front();
  EXPECT_FALSE(run_loop_delayed_a1.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_a1->GetService());
  EXPECT_FALSE(run_loop_delayed_a2.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_a2->GetService());
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_b->GetService());

  // The two kOriginA callbacks should be invoked when the message loop next
  // spins. The two kOriginA requests should now have been assigned the same
  // service, while the kOriginB request is still pending.
  run_loop_delayed_a1.Run();
  run_loop_delayed_a2.Run();
  EXPECT_TRUE(process_delayed_a1->GetService());
  EXPECT_TRUE(process_delayed_a2->GetService());
  EXPECT_EQ(process_delayed_a1->GetService(), process_delayed_a2->GetService());
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_b->GetService());
  EXPECT_EQ(GetMaxProcesses(), auction_process_manager_.NumReceivers());

  // Freeing one of the two kOriginA processes should have no effect.
  process_delayed_a2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_b->GetService());

  // Freeing the other one should queue a task to give the kOriginB requests a
  // process.
  process_delayed_a1.reset();
  EXPECT_FALSE(run_loop_delayed_b.AnyQuitCalled());
  EXPECT_FALSE(process_delayed_b->GetService());

  run_loop_delayed_b.Run();
  EXPECT_TRUE(process_delayed_b->GetService());
  EXPECT_EQ(GetMaxProcesses(), auction_process_manager_.NumReceivers());
}

TEST_P(AuctionProcessManagerTest, DestroyHandlesWithPendingRequests) {
  // Make GetMaxProcesses() requests for worklets with different origins.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> processes;
  for (size_t i = 0; i < GetMaxProcesses(); ++i) {
    url::Origin origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%zu.test", i)));
    processes.emplace_back(GetServiceExpectSuccess(origin));
  }

  // Make a pending request.
  auto pending_process1 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      GetParam(), kOriginA, pending_process1.get(), NeverInvokedClosure()));
  EXPECT_EQ(1u, GetPendingRequestsOfParamType());

  // Destroy the pending request. Its callback should not be invoked.
  pending_process1.reset();
  EXPECT_EQ(0u, GetPendingRequestsOfParamType());
  base::RunLoop().RunUntilIdle();

  // Make two more pending process requests.
  auto pending_process2 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      GetParam(), kOriginA, pending_process2.get(), NeverInvokedClosure()));
  auto pending_process3 =
      std::make_unique<AuctionProcessManager::ProcessHandle>();
  base::RunLoop pending_process3_run_loop;
  ASSERT_FALSE(auction_process_manager_.RequestWorkletService(
      GetParam(), kOriginB, pending_process3.get(),
      pending_process3_run_loop.QuitClosure()));
  EXPECT_EQ(2u, GetPendingRequestsOfParamType());

  // Delete a process. This should result in a posted task to give
  // `pending_process2` a process.
  processes.pop_front();
  EXPECT_EQ(1u, GetPendingRequestsOfParamType());

  // Destroy `pending_process2` before it gets passed a process.
  pending_process2.reset();

  // `pending_process3` should get a process instead.
  pending_process3_run_loop.Run();
  EXPECT_TRUE(pending_process3->GetService());
  EXPECT_EQ(0u, auction_process_manager_.GetPendingSellerRequestsForTesting());
}

// Check that process is automatically re-created on crash. Likely not the most
// important behavior in the world, as auctions aren't restarted on crash, and
// worklet handles should be freed on process crash fairly promptly, but best to
// be safe.
TEST_P(AuctionProcessManagerTest, ProcessCrash) {
  auto process = GetServiceExpectSuccess(kOriginA);
  EXPECT_TRUE(process->GetService());
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());

  // Close pipes. No new pipe should be created.
  auction_process_manager_.ClosePipes();
  EXPECT_EQ(0u, auction_process_manager_.NumReceivers());

  // Request the worklet's service again. A new pipe will automatically be
  // created.
  EXPECT_TRUE(process->GetService());
  EXPECT_EQ(1u, auction_process_manager_.NumReceivers());
}

}  // namespace
}  // namespace content
