// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_queue.h"

#include <stdio.h>

#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/proto/test.pb.h"
#include "chrome/browser/policy/messaging_layer/public/report_queue_configuration.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/test_storage_module.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::policy::DMToken;

using ::testing::_;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithArg;

using ::reporting::test::TestStorageModule;

namespace reporting {
namespace {

// Usage (in tests only):
//
//   TestEvent<ResType> e;
//   ... Do some async work passing e.cb() as a completion callback of
//   base::OnceCallback<void(ResType* res)> type which also may perform some
//   other action specified by |done| callback provided by the caller.
//   ... = e.result();  // Will wait for e.cb() to be called and return the
//   collected result.
//
template <typename ResType>
class TestEvent {
 public:
  TestEvent() : run_loop_(std::make_unique<base::RunLoop>()) {}
  ~TestEvent() = default;
  TestEvent(const TestEvent& other) = delete;
  TestEvent& operator=(const TestEvent& other) = delete;
  ResType result() {
    run_loop_->Run();
    return std::forward<ResType>(result_);
  }

  // Completion callback to hand over to the processing method.
  base::OnceCallback<void(ResType res)> cb() {
    return base::BindOnce(
        [](base::RunLoop* run_loop, ResType* result, ResType res) {
          *result = std::forward<ResType>(res);
          run_loop->Quit();
        },
        base::Unretained(run_loop_.get()), base::Unretained(&result_));
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  ResType result_;
};

// Creates a |ReportQueue| using |TestStorageModule| and |TestEncryptionModule|.
// Allows access to the storage module for checking stored values.
class ReportQueueTest : public testing::Test {
 protected:
  ReportQueueTest()
      : priority_(Priority::IMMEDIATE),
        dm_token_(DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN")),
        destination_(Destination::UPLOAD_EVENTS),
        storage_module_(base::MakeRefCounted<TestStorageModule>()),
        policy_check_callback_(
            base::BindRepeating(&MockFunction<Status()>::Call,
                                base::Unretained(&mocked_policy_check_))) {}

  void SetUp() override {
    ON_CALL(mocked_policy_check_, Call())
        .WillByDefault(Return(Status::StatusOK()));

    StatusOr<std::unique_ptr<ReportQueueConfiguration>> config_result =
        ReportQueueConfiguration::Create(dm_token_, destination_,
                                         policy_check_callback_);

    ASSERT_TRUE(config_result.ok());

    StatusOr<std::unique_ptr<ReportQueue>> report_queue_result =
        ReportQueue::Create(std::move(config_result.ValueOrDie()),
                            storage_module_);

    ASSERT_TRUE(report_queue_result.ok());

    report_queue_ = std::move(report_queue_result.ValueOrDie());
  }

  TestStorageModule* test_storage_module() const {
    TestStorageModule* test_storage_module =
        google::protobuf::down_cast<TestStorageModule*>(storage_module_.get());
    DCHECK(test_storage_module);
    return test_storage_module;
  }

  NiceMock<MockFunction<Status()>> mocked_policy_check_;

  content::BrowserTaskEnvironment task_envrionment_;

  const Priority priority_;

  std::unique_ptr<ReportQueue> report_queue_;
  base::OnceCallback<void(Status)> callback_;

 private:
  const DMToken dm_token_;
  const Destination destination_;
  scoped_refptr<StorageModuleInterface> storage_module_;
  ReportQueueConfiguration::PolicyCheckCallback policy_check_callback_;
};

// Enqueues a random string and ensures that the string arrives unaltered in the
// |StorageModuleInterface|.
TEST_F(ReportQueueTest, SuccessfulStringRecord) {
  constexpr char kTestString[] = "El-Chupacabra";
  TestEvent<Status> a;
  report_queue_->Enqueue(kTestString, priority_, a.cb());
  EXPECT_OK(a.result());
  EXPECT_EQ(test_storage_module()->priority(), priority_);
  EXPECT_EQ(test_storage_module()->record().data(), kTestString);
}

// Enqueues a |base::Value| dictionary and ensures it arrives unaltered in the
// |StorageModuleInterface|.
TEST_F(ReportQueueTest, SuccessfulBaseValueRecord) {
  constexpr char kTestKey[] = "TEST_KEY";
  constexpr char kTestValue[] = "TEST_VALUE";
  base::Value test_dict(base::Value::Type::DICTIONARY);
  test_dict.SetStringKey(kTestKey, kTestValue);
  TestEvent<Status> a;
  report_queue_->Enqueue(test_dict, priority_, a.cb());
  EXPECT_OK(a.result());

  EXPECT_EQ(test_storage_module()->priority(), priority_);

  base::Optional<base::Value> value_result =
      base::JSONReader::Read(test_storage_module()->record().data());
  ASSERT_TRUE(value_result);
  EXPECT_EQ(value_result.value(), test_dict);
}

// Enqueues a |TestMessage| and ensures that it arrives unaltered in the
// |StorageModuleInterface|.
TEST_F(ReportQueueTest, SuccessfulProtoRecord) {
  reporting::test::TestMessage test_message;
  test_message.set_test("TEST_MESSAGE");
  TestEvent<Status> a;
  report_queue_->Enqueue(&test_message, priority_, a.cb());
  EXPECT_OK(a.result());

  EXPECT_EQ(test_storage_module()->priority(), priority_);

  reporting::test::TestMessage result_message;
  ASSERT_TRUE(
      result_message.ParseFromString(test_storage_module()->record().data()));
  ASSERT_EQ(result_message.test(), test_message.test());
}

// The call to enqueue should succeed, indicating that the storage operation has
// been scheduled. The callback should fail, indicating that storage was
// unsuccessful.
TEST_F(ReportQueueTest, CallSuccessCallbackFailure) {
  EXPECT_CALL(*test_storage_module(), AddRecord(_, _, _))
      .WillOnce(
          WithArg<2>(Invoke([](base::OnceCallback<void(Status)> callback) {
            std::move(callback).Run(Status(error::UNKNOWN, "Failing for Test"));
          })));

  reporting::test::TestMessage test_message;
  test_message.set_test("TEST_MESSAGE");
  TestEvent<Status> a;
  report_queue_->Enqueue(&test_message, priority_, a.cb());
  const auto result = a.result();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNKNOWN);
}

TEST_F(ReportQueueTest, EnqueueStringFailsOnPolicy) {
  EXPECT_CALL(mocked_policy_check_, Call())
      .WillOnce(Return(Status(error::UNAUTHENTICATED, "Failing for tests")));
  constexpr char kTestString[] = "El-Chupacabra";
  TestEvent<Status> a;
  report_queue_->Enqueue(kTestString, priority_, a.cb());
  const auto result = a.result();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNAUTHENTICATED);
}

TEST_F(ReportQueueTest, EnqueueProtoFailsOnPolicy) {
  EXPECT_CALL(mocked_policy_check_, Call())
      .WillOnce(Return(Status(error::UNAUTHENTICATED, "Failing for tests")));
  reporting::test::TestMessage test_message;
  test_message.set_test("TEST_MESSAGE");
  TestEvent<Status> a;
  report_queue_->Enqueue(&test_message, priority_, a.cb());
  const auto result = a.result();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNAUTHENTICATED);
}

TEST_F(ReportQueueTest, EnqueueValueFailsOnPolicy) {
  EXPECT_CALL(mocked_policy_check_, Call())
      .WillOnce(Return(Status(error::UNAUTHENTICATED, "Failing for tests")));
  constexpr char kTestKey[] = "TEST_KEY";
  constexpr char kTestValue[] = "TEST_VALUE";
  base::Value test_dict(base::Value::Type::DICTIONARY);
  test_dict.SetStringKey(kTestKey, kTestValue);
  TestEvent<Status> a;
  report_queue_->Enqueue(test_dict, priority_, a.cb());
  const auto result = a.result();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNAUTHENTICATED);
}

}  // namespace
}  // namespace reporting
