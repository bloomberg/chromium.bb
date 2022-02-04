// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/built_in_backend_to_android_backend_migrator.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/fake_password_store_backend.h"
#include "components/password_manager/core/browser/mock_password_store_backend.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::UnorderedElementsAreArray;
using ::testing::WithArg;

namespace password_manager {

namespace {

constexpr base::TimeDelta kLatencyDelta = base::Milliseconds(123u);

PasswordForm CreateTestPasswordForm(int index = 0) {
  PasswordForm form;
  form.url = GURL("https://test" + base::NumberToString(index) + ".com");
  form.signon_realm = form.url.spec();
  form.username_value = u"username" + base::NumberToString16(index);
  form.password_value = u"password" + base::NumberToString16(index);
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

}  // namespace

// Checks that initial/rolling migration is started only when all the conditions
// are satisfied. It also check that migration result is properly recorded in
// prefs.
class BuiltInBackendToAndroidBackendMigratorTest : public testing::Test {
 protected:
  BuiltInBackendToAndroidBackendMigratorTest() = default;
  ~BuiltInBackendToAndroidBackendMigratorTest() override = default;

  void Init(int current_migration_version = 0) {
    prefs_.registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
    prefs_.SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                      current_migration_version);
    prefs_.registry()->RegisterDoublePref(prefs::kTimeOfLastMigrationAttempt,
                                          0.0);
    migrator_ = std::make_unique<BuiltInBackendToAndroidBackendMigrator>(
        &built_in_backend_, &android_backend_, &prefs_,
        /*is_syncing_passwords_callback=*/is_sync_enabled_callback_.Get());
  }

  PasswordStoreBackend& built_in_backend() { return built_in_backend_; }
  PasswordStoreBackend& android_backend() { return android_backend_; }

  base::test::ScopedFeatureList& feature_list() { return feature_list_; }
  TestingPrefServiceSimple* prefs() { return &prefs_; }
  BuiltInBackendToAndroidBackendMigrator* migrator() { return migrator_.get(); }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }
  void FastForwardBy(base::TimeDelta delta) { task_env_.FastForwardBy(delta); }

  void ExpectSyncCallbackAndSetResult(bool enabled) {
    EXPECT_CALL(is_sync_enabled_callback_, Run())
        .WillOnce(testing::Return(enabled));
  }

 private:
  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;
  FakePasswordStoreBackend built_in_backend_;
  FakePasswordStoreBackend android_backend_;
  std::unique_ptr<BuiltInBackendToAndroidBackendMigrator> migrator_;
  base::MockCallback<base::RepeatingCallback<bool(void)>>
      is_sync_enabled_callback_;
};

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       CurrentMigrationVersionIsUpdatedWhenMigrationIsNeeded_SyncOn) {
  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerMigration,
      {{"migration_version", "1"}});
  Init();
  ExpectSyncCallbackAndSetResult(true);

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  // Since for syncing users we don't manually migrate passwords
  // |kTimeOfLastMigrationAttempt| shouldn't be updated.
  EXPECT_EQ(0, prefs()->GetDouble(
                   password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       AllPrefsAreUpdatedWhenMigrationIsNeeded_SyncOff) {
  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerMigration,
      {{"migration_version", "1"}});
  Init();

  ExpectSyncCallbackAndSetResult(false);

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(
      base::Time::Now().ToDoubleT(),
      prefs()->GetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       PrefsUnchangedWhenAttemptedMigrationEarlierToday) {
  feature_list().InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerMigration, {{"migration_version", "1"}});
  Init();

  prefs()->SetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt,
                     (base::Time::Now() - base::Hours(2)).ToDoubleT());
  ExpectSyncCallbackAndSetResult(false);

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(0, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(
      (base::Time::Now() - base::Hours(2)).ToDoubleT(),
      prefs()->GetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       LastAttemptUnchangedWhenRollingMigrationDisabled) {
  // Setup the pref to indicate that the initial migration has happened already.
  feature_list().InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kUnifiedPasswordManagerMigration,
                             {{"migration_version", "1"}}}},
      /*disabled_features=*/{features::kUnifiedPasswordManagerAndroid});
  Init(/*current_migration_version=*/1);

  ExpectSyncCallbackAndSetResult(false);

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(0, prefs()->GetDouble(
                   password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       LastAttemptUpdatedInPrefsWhenRollingMigrationEnabled) {
  // Setup the pref to indicate that the initial migration has happened already.
  feature_list().InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kUnifiedPasswordManagerMigration,
                             {{"migration_version", "1"}}},
                            {features::kUnifiedPasswordManagerAndroid, {{}}}},
      /*disabled_features=*/{});
  Init(/*current_migration_version=*/1);

  ExpectSyncCallbackAndSetResult(false);

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(
      base::Time::Now().ToDoubleT(),
      prefs()->GetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       InitialMigrationNeverStartedMetrics) {
  base::HistogramTester histogram_tester;
  const char kMigrationFinishedMetric[] =
      "PasswordManager.UnifiedPasswordManager.WasMigrationDone";

  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerMigration,
      {{"migration_version", "1"}});
  Init();

  histogram_tester.ExpectTotalCount(kMigrationFinishedMetric, 1);
  histogram_tester.ExpectBucketCount(kMigrationFinishedMetric, false, 1);
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       InitialMigrationFinishedMetrics) {
  base::HistogramTester histogram_tester;
  const char kMigrationFinishedMetric[] =
      "PasswordManager.UnifiedPasswordManager.WasMigrationDone";

  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerMigration,
      {{"migration_version", "1"}});
  Init(/*current_migration_version=*/1);

  histogram_tester.ExpectTotalCount(kMigrationFinishedMetric, 1);
  histogram_tester.ExpectBucketCount(kMigrationFinishedMetric, true, 1);
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       InitialMigrationNeedsRestartMetrics) {
  base::HistogramTester histogram_tester;
  const char kMigrationFinishedMetric[] =
      "PasswordManager.UnifiedPasswordManager.WasMigrationDone";

  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerMigration,
      {{"migration_version", "2"}});

  Init(/*current_migration_version=*/1);

  histogram_tester.ExpectTotalCount(kMigrationFinishedMetric, 1);
  histogram_tester.ExpectBucketCount(kMigrationFinishedMetric, false, 1);
}

// Holds the built in and android backend's logins and the expected result after
// the migration.
struct MigrationParam {
  struct Entry {
    Entry(int index,
          std::string password = "",
          base::TimeDelta date_created = base::TimeDelta())
        : index(index), password(password), date_created(date_created) {}

    std::unique_ptr<PasswordForm> ToPasswordForm() const {
      PasswordForm form = CreateTestPasswordForm(index);
      form.password_value = base::ASCIIToUTF16(password);
      form.date_created = base::Time() + date_created;
      return std::make_unique<PasswordForm>(form);
    }

    int index;
    std::string password;
    base::TimeDelta date_created;
  };

  std::vector<std::unique_ptr<PasswordForm>> GetBuiltInLogins() const {
    return EntriesToPasswordForms(built_in_logins);
  }

  std::vector<std::unique_ptr<PasswordForm>> GetAndroidLogins() const {
    return EntriesToPasswordForms(android_logins);
  }

  std::vector<std::unique_ptr<PasswordForm>> GetMergedLogins() const {
    return EntriesToPasswordForms(merged_logins);
  }

  std::vector<std::unique_ptr<PasswordForm>> EntriesToPasswordForms(
      const std::vector<Entry>& entries) const {
    std::vector<std::unique_ptr<PasswordForm>> v;
    base::ranges::transform(entries, std::back_inserter(v),
                            &Entry::ToPasswordForm);
    return v;
  }

  std::vector<Entry> built_in_logins;
  std::vector<Entry> android_logins;
  std::vector<Entry> merged_logins;
};

// Tests that initial and rolling migration actually works by comparing
// passwords in built-in/android backend before and after migration.
class BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams
    : public BuiltInBackendToAndroidBackendMigratorTest,
      public testing::WithParamInterface<MigrationParam> {};

// Tests the initial migration result.
TEST_P(BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams,
       InitialMigration) {
  BuiltInBackendToAndroidBackendMigratorTest::Init();

  ExpectSyncCallbackAndSetResult(false);

  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerMigration,
      {{"migration_version", "1"}});

  const MigrationParam& p = GetParam();

  for (const auto& login : p.GetBuiltInLogins()) {
    built_in_backend().AddLoginAsync(*login, base::DoNothing());
  }
  for (const auto& login : p.GetAndroidLogins()) {
    android_backend().AddLoginAsync(*login, base::DoNothing());
  }
  RunUntilIdle();

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  for (auto* const backend : {&android_backend(), &built_in_backend()}) {
    base::MockCallback<LoginsOrErrorReply> mock_reply;
    auto expected_logins = p.GetMergedLogins();
    EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
    backend->GetAllLoginsAsync(mock_reply.Get());
    RunUntilIdle();
  }
}

TEST_P(BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams,
       RollingMigration) {
  // Setup the pref to indicate that the initial migration has happened already.
  // This implies that rolling migration will take place!
  feature_list().InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kUnifiedPasswordManagerMigration,
                             {{"migration_version", "1"}}},
                            {features::kUnifiedPasswordManagerAndroid, {{}}}},
      /*disabled_features=*/{});
  BuiltInBackendToAndroidBackendMigratorTest::Init(
      /*current_migration_version=*/1);

  const MigrationParam& p = GetParam();

  for (const auto& login : p.GetBuiltInLogins()) {
    built_in_backend().AddLoginAsync(*login, base::DoNothing());
  }
  for (const auto& login : p.GetAndroidLogins()) {
    android_backend().AddLoginAsync(*login, base::DoNothing());
  }
  RunUntilIdle();

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  for (auto* const backend : {&android_backend(), &built_in_backend()}) {
    base::MockCallback<LoginsOrErrorReply> mock_reply;
    auto expected_logins = p.GetAndroidLogins();
    EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
    backend->GetAllLoginsAsync(mock_reply.Get());
    RunUntilIdle();
  }
}

INSTANTIATE_TEST_SUITE_P(
    BuiltInBackendToAndroidBackendMigratorTest,
    BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams,
    testing::Values(
        MigrationParam{.built_in_logins = {},
                       .android_logins = {},
                       .merged_logins = {}},
        MigrationParam{.built_in_logins = {{1}, {2}},
                       .android_logins = {},
                       .merged_logins = {{1}, {2}}},
        MigrationParam{.built_in_logins = {},
                       .android_logins = {{1}, {2}},
                       .merged_logins = {{1}, {2}}},
        MigrationParam{.built_in_logins = {{1}, {2}},
                       .android_logins = {{3}},
                       .merged_logins = {{1}, {2}, {3}}},
        MigrationParam{.built_in_logins = {{1}, {2}, {3}},
                       .android_logins = {{1}, {2}, {3}},
                       .merged_logins = {{1}, {2}, {3}}},
        MigrationParam{
            .built_in_logins = {{1, "old_password", base::Days(1)}, {2}},
            .android_logins = {{1, "new_password", base::Days(2)}, {3}},
            .merged_logins = {{1, "new_password", base::Days(2)}, {2}, {3}}},
        MigrationParam{
            .built_in_logins = {{1, "new_password", base::Days(2)}, {2}},
            .android_logins = {{1, "old_password", base::Days(1)}, {3}},
            .merged_logins = {{1, "new_password", base::Days(2)}, {2}, {3}}}));

struct MigrationParamForMetrics {
  // Whether this is initial or rolling migration.
  bool is_initial_migration;
  // Whether migration was completed successfully or not.
  bool is_successful_migration;
};

class BuiltInBackendToAndroidBackendMigratorTestMetrics
    : public BuiltInBackendToAndroidBackendMigratorTest,
      public testing::WithParamInterface<MigrationParamForMetrics> {
 protected:
  BuiltInBackendToAndroidBackendMigratorTestMetrics() {
    prefs()->registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
    prefs()->registry()->RegisterDoublePref(prefs::kTimeOfLastMigrationAttempt,
                                            0.0);
    if (GetParam().is_initial_migration) {
      feature_list().InitAndEnableFeatureWithParameters(
          /*enabled_feature=*/features::kUnifiedPasswordManagerMigration,
          {{"migration_version", "1"}});
      latency_metric_ =
          "PasswordManager.UnifiedPasswordManager.InitialMigration.Latency";
      success_metric_ =
          "PasswordManager.UnifiedPasswordManager.InitialMigration.Success";
    } else {
      feature_list().InitWithFeaturesAndParameters(
          /*enabled_features=*/{{features::kUnifiedPasswordManagerMigration,
                                 {{"migration_version", "1"}}},
                                {features::kUnifiedPasswordManagerAndroid,
                                 {{}}}},
          /*disabled_features=*/{});
      // Setup the pref to indicate that the initial migration has happened
      // already.
      prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                          1);
      latency_metric_ =
          "PasswordManager.UnifiedPasswordManager.RollingMigration.Latency";
      success_metric_ =
          "PasswordManager.UnifiedPasswordManager.RollingMigration.Success";
    }

    migrator_ = std::make_unique<BuiltInBackendToAndroidBackendMigrator>(
        &built_in_backend_, &android_backend_, prefs(),
        /*is_syncing_passwords_callback=*/base::BindRepeating([]() {
          return false;
        }));
  }

 protected:
  std::string latency_metric_;
  std::string success_metric_;
  ::testing::StrictMock<MockPasswordStoreBackend> built_in_backend_;
  ::testing::StrictMock<MockPasswordStoreBackend> android_backend_;
  std::unique_ptr<BuiltInBackendToAndroidBackendMigrator> migrator_;
};

TEST_P(BuiltInBackendToAndroidBackendMigratorTestMetrics,
       MigrationMetricsTest) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(built_in_backend_, GetAllLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(reply), LoginsResult()));
      })));
  EXPECT_CALL(android_backend_, GetAllLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        LoginsResultOrError result =
            GetParam().is_successful_migration
                ? LoginsResultOrError(LoginsResult())
                : LoginsResultOrError(PasswordStoreBackendError::kUnspecified);
        base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE, base::BindOnce(std::move(reply), std::move(result)),
            kLatencyDelta);
      })));

  migrator_->StartMigrationIfNecessary();
  FastForwardBy(kLatencyDelta);

  histogram_tester.ExpectTotalCount(latency_metric_, 1);
  histogram_tester.ExpectTimeBucketCount(latency_metric_, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(success_metric_, 1);
  histogram_tester.ExpectBucketCount(success_metric_, true,
                                     GetParam().is_successful_migration);
  histogram_tester.ExpectBucketCount(success_metric_, false,
                                     !GetParam().is_successful_migration);
}

INSTANTIATE_TEST_SUITE_P(
    BuiltInBackendToAndroidBackendMigratorTest,
    BuiltInBackendToAndroidBackendMigratorTestMetrics,
    testing::Values(MigrationParamForMetrics{.is_initial_migration = true,
                                             .is_successful_migration = true},
                    MigrationParamForMetrics{.is_initial_migration = true,
                                             .is_successful_migration = false},
                    MigrationParamForMetrics{.is_initial_migration = false,
                                             .is_successful_migration = true},
                    MigrationParamForMetrics{
                        .is_initial_migration = false,
                        .is_successful_migration = false}));

}  // namespace password_manager
