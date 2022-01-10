// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend.h"
#include "base/memory/raw_ptr.h"

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::Return;
using testing::StrictMock;
using testing::WithArg;
using JobId = PasswordStoreAndroidBackendBridge::JobId;

const std::u16string kTestUsername(u"Todd Tester");
const std::u16string kTestPassword(u"S3cr3t");
constexpr char kTestUrl[] = "https://example.com";
const base::Time kTestDateCreated = base::Time::FromTimeT(1500);

MATCHER_P(ExpectError, expectation, "") {
  return absl::holds_alternative<PasswordStoreBackendError>(arg) &&
         expectation == absl::get<PasswordStoreBackendError>(arg);
}

std::vector<std::unique_ptr<PasswordForm>> CreateTestLogins() {
  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(CreateEntry("Todd Tester", "S3cr3t",
                              GURL(u"https://example.com"),
                              /*psl=*/false,
                              /*affiliation=*/false));
  forms.push_back(CreateEntry("Marcus McSpartanGregor", "S0m3th1ngCr34t1v3",
                              GURL(u"https://m.example.com"), /*psl=*/true,
                              /*affiliation=*/false));
  return forms;
}

PasswordForm CreateTestLogin(const std::u16string& username_value,
                             const std::u16string& password_value,
                             const std::string& url,
                             base::Time date_created) {
  PasswordForm form;
  form.username_value = username_value;
  form.password_value = password_value;
  form.url = GURL(url);
  form.signon_realm = url;
  form.date_created = date_created;
  return form;
}

std::vector<PasswordForm> UnwrapForms(
    std::vector<std::unique_ptr<PasswordForm>> password_ptrs) {
  std::vector<PasswordForm> forms;
  forms.reserve(password_ptrs.size());
  for (auto& password : password_ptrs) {
    forms.push_back(std::move(*password));
  }
  return forms;
}

class MockPasswordStoreAndroidBackendBridge
    : public PasswordStoreAndroidBackendBridge {
 public:
  MOCK_METHOD(void, SetConsumer, (base::WeakPtr<Consumer>), (override));
  MOCK_METHOD(JobId, GetAllLogins, (), (override));
  MOCK_METHOD(JobId, GetAutofillableLogins, (), (override));
  MOCK_METHOD(JobId, GetLoginsForSignonRealm, (const std::string&), (override));
  MOCK_METHOD(JobId, AddLogin, (const PasswordForm&), (override));
  MOCK_METHOD(JobId, UpdateLogin, (const PasswordForm&), (override));
  MOCK_METHOD(JobId, RemoveLogin, (const PasswordForm&), (override));
};

}  // namespace

class PasswordStoreAndroidBackendTest : public testing::Test {
 protected:
  PasswordStoreAndroidBackendTest() {
    backend_ =
        std::make_unique<PasswordStoreAndroidBackend>(CreateMockBridge());
  }

  ~PasswordStoreAndroidBackendTest() override {
    testing::Mock::VerifyAndClearExpectations(bridge_);
  }

  PasswordStoreBackend& backend() { return *backend_; }
  PasswordStoreAndroidBackendBridge::Consumer& consumer() { return *backend_; }
  MockPasswordStoreAndroidBackendBridge* bridge() { return bridge_; }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<PasswordStoreAndroidBackendBridge> CreateMockBridge() {
    auto unique_bridge =
        std::make_unique<StrictMock<MockPasswordStoreAndroidBackendBridge>>();
    bridge_ = unique_bridge.get();
    EXPECT_CALL(*bridge_, SetConsumer);
    return unique_bridge;
  }

  std::unique_ptr<PasswordStoreAndroidBackend> backend_;
  raw_ptr<StrictMock<MockPasswordStoreAndroidBackendBridge>> bridge_;
};

TEST_F(PasswordStoreAndroidBackendTest, CallsCompletionCallbackAfterInit) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;
  EXPECT_CALL(completion_callback, Run(true));
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), completion_callback.Get());
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForLogins) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  const JobId kJobId{1337};
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());

  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  consumer().OnCompleteWithLogins(kJobId, UnwrapForms(CreateTestLogins()));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest, FillMatchingLoginsNoPSL) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  base::MockCallback<LoginsReply> mock_reply;

  const JobId kFirstJobId{1337};
  EXPECT_CALL(*bridge(), GetLoginsForSignonRealm).WillOnce(Return(kFirstJobId));
  constexpr auto kLatencyDelta = base::Milliseconds(123u);

  std::string TestURL1("https://firstexample.com");
  std::string TestURL2("https://secondexample.com");

  std::vector<PasswordFormDigest> forms;
  forms.push_back(PasswordFormDigest(PasswordForm::Scheme::kHtml, TestURL1,
                                     GURL(TestURL1)));
  forms.push_back(PasswordFormDigest(PasswordForm::Scheme::kHtml, TestURL2,
                                     GURL(TestURL2)));
  backend().FillMatchingLoginsAsync(mock_reply.Get(), /*include_psl=*/false,
                                    forms);

  // Imitate login retrieval.
  PasswordForm matching_signon_realm =
      CreateTestLogin(kTestUsername, kTestPassword, TestURL1, kTestDateCreated);
  PasswordForm matching_federated = CreateTestLogin(
      kTestUsername, kTestPassword,
      "federation://secondexample.com/google.com/", kTestDateCreated);
  PasswordForm not_matching =
      CreateTestLogin(kTestUsername, kTestPassword,
                      "https://mobile.secondexample.com/", kTestDateCreated);

  const JobId kSecondJobId{1338};
  EXPECT_CALL(*bridge(), GetLoginsForSignonRealm)
      .WillOnce(Return(kSecondJobId));
  // Logins will be retrieved for forms from |forms| in a backwards order.
  consumer().OnCompleteWithLogins(kFirstJobId,
                                  {matching_federated, not_matching});
  RunUntilIdle();

  // Retrieving logins for the last form should trigger the final callback.
  LoginsResult expected_logins;
  expected_logins.push_back(std::make_unique<PasswordForm>(matching_federated));
  expected_logins.push_back(
      std::make_unique<PasswordForm>(matching_signon_realm));
  EXPECT_CALL(mock_reply,
              Run(UnorderedPasswordFormElementsAre(&expected_logins)));

  task_environment_.FastForwardBy(kLatencyDelta);
  consumer().OnCompleteWithLogins(kSecondJobId, {matching_signon_realm});
  RunUntilIdle();

  histogram_tester.ExpectTimeBucketCount(
      "PasswordManager.PasswordStoreAndroidBackend.FillMatchingLoginsAsync."
      "Latency",
      kLatencyDelta, 1);
}

TEST_F(PasswordStoreAndroidBackendTest, FillMatchingLoginsPSL) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  base::MockCallback<LoginsReply> mock_reply;

  const JobId kFirstJobId{1337};
  EXPECT_CALL(*bridge(), GetLoginsForSignonRealm).WillOnce(Return(kFirstJobId));
  constexpr auto kLatencyDelta = base::Milliseconds(123u);

  std::string TestURL1("https://firstexample.com");
  std::string TestURL2("https://secondexample.com");

  std::vector<PasswordFormDigest> forms;
  forms.push_back(PasswordFormDigest(PasswordForm::Scheme::kHtml, TestURL1,
                                     GURL(TestURL1)));
  forms.push_back(PasswordFormDigest(PasswordForm::Scheme::kHtml, TestURL2,
                                     GURL(TestURL2)));
  backend().FillMatchingLoginsAsync(mock_reply.Get(), /*include_psl=*/true,
                                    forms);

  // Imitate login retrieval.
  PasswordForm psl_matching =
      CreateTestLogin(kTestUsername, kTestPassword,
                      "https://mobile.firstexample.com/", kTestDateCreated);
  PasswordForm not_matching =
      CreateTestLogin(kTestUsername, kTestPassword,
                      "https://nomatchfirstexample.com/", kTestDateCreated);
  PasswordForm psl_matching_federated = CreateTestLogin(
      kTestUsername, kTestPassword,
      "federation://mobile.secondexample.com/google.com/", kTestDateCreated);

  const JobId kSecondJobId{1338};
  EXPECT_CALL(*bridge(), GetLoginsForSignonRealm)
      .WillOnce(Return(kSecondJobId));
  // Logins will be retrieved for forms from |forms| in a backwards order.
  consumer().OnCompleteWithLogins(kFirstJobId, {psl_matching_federated});
  RunUntilIdle();

  // Retrieving logins for the last form should trigger the final callback.
  LoginsResult expected_logins;
  expected_logins.push_back(std::make_unique<PasswordForm>(psl_matching));
  expected_logins.push_back(
      std::make_unique<PasswordForm>(psl_matching_federated));
  EXPECT_CALL(mock_reply,
              Run(UnorderedPasswordFormElementsAre(&expected_logins)));

  task_environment_.FastForwardBy(kLatencyDelta);
  consumer().OnCompleteWithLogins(kSecondJobId, {psl_matching, not_matching});
  RunUntilIdle();
  histogram_tester.ExpectTimeBucketCount(
      "PasswordManager.PasswordStoreAndroidBackend.FillMatchingLoginsAsync."
      "Latency",
      kLatencyDelta, 1);
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForAutofillableLogins) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  const JobId kJobId{1337};
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge(), GetAutofillableLogins).WillOnce(Return(kJobId));
  backend().GetAutofillableLoginsAsync(mock_reply.Get());

  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  consumer().OnCompleteWithLogins(kJobId, UnwrapForms(CreateTestLogins()));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForRemoveLogin) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  const JobId kJobId{13388};
  base::MockCallback<PasswordStoreChangeListReply> mock_reply;

  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge(), RemoveLogin(form)).WillOnce(Return(kJobId));
  backend().RemoveLoginAsync(form, mock_reply.Get());

  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::REMOVE, form));
  EXPECT_CALL(mock_reply, Run(expected_changes));
  consumer().OnLoginsChanged(kJobId, expected_changes);
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest,
       CallsBridgeForRemoveLoginsByURLAndTime) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  base::MockCallback<PasswordStoreChangeListReply> mock_deletion_reply;
  base::RepeatingCallback<bool(const GURL&)> url_filter = base::BindRepeating(
      [](const GURL& url) { return url == GURL(kTestUrl); });
  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);

  // Check that calling RemoveLoginsByURLAndTime triggers logins retrieval
  // first.
  base::MockCallback<LoginsReply> mock_logins_reply;
  const JobId kGetLoginsJobId{13387};
  EXPECT_CALL(*bridge(), GetAllLogins).WillOnce(Return(kGetLoginsJobId));
  backend().RemoveLoginsByURLAndTimeAsync(url_filter, delete_begin, delete_end,
                                          base::OnceCallback<void(bool)>(),
                                          mock_deletion_reply.Get());

  // Imitate login retrieval and check that it triggers the removal of matching
  // forms.
  const JobId kRemoveLoginJobId{13388};
  EXPECT_CALL(*bridge(), RemoveLogin).WillOnce(Return(kRemoveLoginJobId));
  PasswordForm form_to_delete = CreateTestLogin(
      kTestUsername, kTestPassword, kTestUrl, base::Time::FromTimeT(1500));
  PasswordForm form_to_keep =
      CreateTestLogin(kTestUsername, kTestPassword, "https://differentsite.com",
                      base::Time::FromTimeT(1500));
  consumer().OnCompleteWithLogins(kGetLoginsJobId,
                                  {form_to_delete, form_to_keep});
  RunUntilIdle();

  // Verify that the callback is called.
  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::REMOVE, form_to_delete));
  EXPECT_CALL(mock_deletion_reply, Run(expected_changes));
  consumer().OnLoginsChanged(kRemoveLoginJobId, expected_changes);
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest,
       CallsBridgeForRemoveLoginsCreatedBetween) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  base::MockCallback<PasswordStoreChangeListReply> mock_deletion_reply;
  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);

  // Check that calling RemoveLoginsCreatedBetween triggers logins retrieval
  // first.
  base::MockCallback<LoginsReply> mock_logins_reply;
  const JobId kGetLoginsJobId{13387};
  EXPECT_CALL(*bridge(), GetAllLogins).WillOnce(Return(kGetLoginsJobId));
  backend().RemoveLoginsCreatedBetweenAsync(delete_begin, delete_end,
                                            mock_deletion_reply.Get());

  // Imitate login retrieval and check that it triggers the removal of matching
  // forms.
  const JobId kRemoveLoginJobId{13388};
  EXPECT_CALL(*bridge(), RemoveLogin).WillOnce(Return(kRemoveLoginJobId));
  PasswordForm form_to_delete = CreateTestLogin(
      kTestUsername, kTestPassword, kTestUrl, base::Time::FromTimeT(1500));
  PasswordForm form_to_keep = CreateTestLogin(
      kTestUsername, kTestPassword, kTestUrl, base::Time::FromTimeT(2500));
  consumer().OnCompleteWithLogins(kGetLoginsJobId,
                                  {form_to_delete, form_to_keep});
  RunUntilIdle();

  // Verify that the callback is called.
  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::REMOVE, form_to_delete));
  EXPECT_CALL(mock_deletion_reply, Run(expected_changes));
  consumer().OnLoginsChanged(kRemoveLoginJobId, expected_changes);
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForAddLogin) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  const JobId kJobId{13388};
  base::MockCallback<PasswordStoreChangeListReply> mock_reply;

  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge(), AddLogin(form)).WillOnce(Return(kJobId));
  backend().AddLoginAsync(form, mock_reply.Get());

  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::ADD, form));
  EXPECT_CALL(mock_reply, Run(expected_changes));
  consumer().OnLoginsChanged(kJobId, expected_changes);
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForUpdateLogin) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  const JobId kJobId{13388};
  base::MockCallback<PasswordStoreChangeListReply> mock_reply;

  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge(), UpdateLogin(form)).WillOnce(Return(kJobId));
  backend().UpdateLoginAsync(form, mock_reply.Get());

  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::UPDATE, form));
  EXPECT_CALL(mock_reply, Run(expected_changes));
  consumer().OnLoginsChanged(kJobId, expected_changes);
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest, OnExternalError) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  const JobId kJobId{1337};
  base::HistogramTester histogram_tester;
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  EXPECT_CALL(mock_reply,
              Run(ExpectError(PasswordStoreBackendError::kUnspecified)));
  AndroidBackendError error{AndroidBackendErrorType::kExternalError};
  error.api_error_code = absl::optional<int>(11004);
  consumer().OnError(kJobId, std::move(error));
  RunUntilIdle();

  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kAPIErrorMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.APIError";

  histogram_tester.ExpectBucketCount(kErrorCodeMetric, 7, 1);
  histogram_tester.ExpectBucketCount(kAPIErrorMetric, 11004, 1);
}

class PasswordStoreAndroidBackendTestForMetrics
    : public PasswordStoreAndroidBackendTest,
      public testing::WithParamInterface<bool> {
 public:
  bool ShouldSucceed() const { return GetParam(); }
};

// Tests the PasswordManager.PasswordStore.GetAllLoginsAsync metric.
TEST_P(PasswordStoreAndroidBackendTestForMetrics, GetAllLoginsAsyncMetrics) {
  backend().InitBackend(
      PasswordStoreAndroidBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::RepeatingClosure(),
      /*completion=*/base::DoNothing());
  constexpr auto kLatencyDelta = base::Milliseconds(123u);
  constexpr JobId kJobId{1337};
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.GetAllLoginsAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.GetAllLoginsAsync.Success";
  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kPerApiErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.GetAllLoginsAsync.ErrorCode";
  base::HistogramTester histogram_tester;
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  EXPECT_CALL(mock_reply, Run(_)).Times(1);
  task_environment_.FastForwardBy(kLatencyDelta);
  if (ShouldSucceed())
    consumer().OnCompleteWithLogins(kJobId, {});
  else
    consumer().OnError(
        kJobId, AndroidBackendError(AndroidBackendErrorType::kUncategorized));
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, ShouldSucceed());
  histogram_tester.ExpectBucketCount(kSuccessMetric, false, !ShouldSucceed());
  if (!ShouldSucceed()) {
    histogram_tester.ExpectBucketCount(kErrorCodeMetric, 0, 1);
    histogram_tester.ExpectBucketCount(kPerApiErrorCodeMetric, 0, 1);
  }
}

// Tests the PasswordManager.PasswordStore.AddLoginAsync.* metric.
TEST_P(PasswordStoreAndroidBackendTestForMetrics, AddLoginAsyncMetrics) {
  backend().InitBackend(
      PasswordStoreAndroidBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::RepeatingClosure(),
      /*completion=*/base::DoNothing());
  constexpr auto kLatencyDelta = base::Milliseconds(123u);
  constexpr JobId kJobId{1337};
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.AddLoginAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.AddLoginAsync.Success";
  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kPerApiErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.AddLoginAsync.ErrorCode";
  base::HistogramTester histogram_tester;

  base::MockCallback<PasswordStoreChangeListReply> mock_reply;
  EXPECT_CALL(*bridge(), AddLogin).WillOnce(Return(kJobId));
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  backend().AddLoginAsync(form, mock_reply.Get());
  EXPECT_CALL(mock_reply, Run);
  task_environment_.FastForwardBy(kLatencyDelta);

  if (ShouldSucceed()) {
    consumer().OnLoginsChanged(kJobId, {});
  } else {
    consumer().OnError(
        kJobId, AndroidBackendError(AndroidBackendErrorType::kUncategorized));
  }
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, ShouldSucceed());
  histogram_tester.ExpectBucketCount(kSuccessMetric, false, !ShouldSucceed());
  if (!ShouldSucceed()) {
    histogram_tester.ExpectBucketCount(kErrorCodeMetric, 0, 1);
    histogram_tester.ExpectBucketCount(kPerApiErrorCodeMetric, 0, 1);
  }
}

// Tests the PasswordManager.PasswordStore.UpdateLoginAsync metric.
TEST_P(PasswordStoreAndroidBackendTestForMetrics, UpdateLoginAsyncMetrics) {
  backend().InitBackend(
      PasswordStoreAndroidBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::RepeatingClosure(),
      /*completion=*/base::DoNothing());
  constexpr auto kLatencyDelta = base::Milliseconds(123u);
  constexpr JobId kJobId{1337};
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.UpdateLoginAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.UpdateLoginAsync.Success";
  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kPerApiErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.UpdateLoginAsync.ErrorCode";
  base::HistogramTester histogram_tester;

  base::MockCallback<PasswordStoreChangeListReply> mock_reply;
  EXPECT_CALL(*bridge(), UpdateLogin).WillOnce(Return(kJobId));
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  backend().UpdateLoginAsync(form, mock_reply.Get());
  EXPECT_CALL(mock_reply, Run);
  task_environment_.FastForwardBy(kLatencyDelta);

  if (ShouldSucceed()) {
    consumer().OnLoginsChanged(kJobId, {});
  } else {
    consumer().OnError(
        kJobId, AndroidBackendError(AndroidBackendErrorType::kUncategorized));
  }
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, ShouldSucceed());
  histogram_tester.ExpectBucketCount(kSuccessMetric, false, !ShouldSucceed());
  if (!ShouldSucceed()) {
    histogram_tester.ExpectBucketCount(kErrorCodeMetric, 0, 1);
    histogram_tester.ExpectBucketCount(kPerApiErrorCodeMetric, 0, 1);
  }
}

// Tests the PasswordManager.PasswordStore.RemoveLoginAsync metric.
TEST_P(PasswordStoreAndroidBackendTestForMetrics, RemoveLoginAsyncMetrics) {
  backend().InitBackend(
      PasswordStoreAndroidBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::RepeatingClosure(),
      /*completion=*/base::DoNothing());
  constexpr auto kLatencyDelta = base::Milliseconds(123u);
  constexpr JobId kJobId{1337};
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.RemoveLoginAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.RemoveLoginAsync.Success";
  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kPerApiErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.RemoveLoginAsync.ErrorCode";
  base::HistogramTester histogram_tester;

  base::MockCallback<PasswordStoreChangeListReply> mock_reply;
  EXPECT_CALL(*bridge(), RemoveLogin).WillOnce(Return(kJobId));
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  backend().RemoveLoginAsync(form, mock_reply.Get());
  EXPECT_CALL(mock_reply, Run);
  task_environment_.FastForwardBy(kLatencyDelta);

  if (ShouldSucceed()) {
    consumer().OnLoginsChanged(kJobId, {});
  } else {
    consumer().OnError(
        kJobId, AndroidBackendError(AndroidBackendErrorType::kUncategorized));
  }
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, ShouldSucceed());
  histogram_tester.ExpectBucketCount(kSuccessMetric, false, !ShouldSucceed());
  if (!ShouldSucceed()) {
    histogram_tester.ExpectBucketCount(kErrorCodeMetric, 0, 1);
    histogram_tester.ExpectBucketCount(kPerApiErrorCodeMetric, 0, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         PasswordStoreAndroidBackendTestForMetrics,
                         testing::Bool());

}  // namespace password_manager
