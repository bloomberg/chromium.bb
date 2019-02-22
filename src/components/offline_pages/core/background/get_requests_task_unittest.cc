// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/get_requests_task.h"

#include <memory>

#include "base/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/request_queue_task_test_base.h"
#include "components/offline_pages/core/background/test_request_queue_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
const int64_t kRequestId1 = 42;
const int64_t kRequestId2 = 44;
const GURL kUrl1("http://example.com");
const GURL kUrl2("http://otherexample.com");
const ClientId kClientId1("download", "1234");
const ClientId kClientId2("download", "5678");

class GetRequestsTaskTest : public RequestQueueTaskTestBase {
 public:
  GetRequestsTaskTest() = default;
  ~GetRequestsTaskTest() override = default;

  void AddItemsToStore(RequestQueueStore* store);
  void GetRequestsCallback(
      bool success,
      std::vector<std::unique_ptr<SavePageRequest>> requests);

  bool callback_called() const { return callback_called_; }

  bool last_call_successful() const { return success_; }

  const std::vector<std::unique_ptr<SavePageRequest>>& last_requests() const {
    return requests_;
  }

 private:
  void AddRequestDone(ItemActionStatus status);

  bool callback_called_ = false;
  bool success_ = false;
  std::vector<std::unique_ptr<SavePageRequest>> requests_;
};

void GetRequestsTaskTest::AddItemsToStore(RequestQueueStore* store) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request_1(kRequestId1, kUrl1, kClientId1, creation_time,
                            true);
  store->AddRequest(request_1,
                    base::BindOnce(&GetRequestsTaskTest::AddRequestDone,
                                   base::Unretained(this)));
  creation_time = base::Time::Now();
  SavePageRequest request_2(kRequestId2, kUrl2, kClientId2, creation_time,
                            true);
  store->AddRequest(request_2,
                    base::BindOnce(&GetRequestsTaskTest::AddRequestDone,
                                   base::Unretained(this)));
  PumpLoop();
}

void GetRequestsTaskTest::GetRequestsCallback(
    bool success,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  callback_called_ = true;
  success_ = success;
  requests_ = std::move(requests);
}

void GetRequestsTaskTest::AddRequestDone(ItemActionStatus status) {
  ASSERT_EQ(ItemActionStatus::SUCCESS, status);
}

TEST_F(GetRequestsTaskTest, GetFromEmptyStore) {
  InitializeStore();
  GetRequestsTask task(&store_,
                       base::BindOnce(&GetRequestsTaskTest::GetRequestsCallback,
                                      base::Unretained(this)));
  task.Run();
  PumpLoop();
  EXPECT_TRUE(callback_called());
  EXPECT_TRUE(last_call_successful());
  EXPECT_TRUE(last_requests().empty());
}

TEST_F(GetRequestsTaskTest, GetMultipleRequests) {
  InitializeStore();
  AddItemsToStore(&store_);

  GetRequestsTask task(&store_,
                       base::BindOnce(&GetRequestsTaskTest::GetRequestsCallback,
                                      base::Unretained(this)));
  task.Run();
  PumpLoop();
  EXPECT_TRUE(callback_called());
  EXPECT_TRUE(last_call_successful());
  ASSERT_EQ(2UL, last_requests().size());

  int id_1_index = last_requests().at(0)->request_id() == kRequestId1 ? 0 : 1;
  int id_2_index = 1 - id_1_index;
  EXPECT_EQ(kRequestId1, last_requests().at(id_1_index)->request_id());
  EXPECT_EQ(kRequestId2, last_requests().at(id_2_index)->request_id());
}

}  // namespace
}  // namespace offline_pages
