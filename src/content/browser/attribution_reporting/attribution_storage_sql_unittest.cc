// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql.h"

#include <functional>
#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/aggregatable_attribution.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;

class AttributionStorageSqlTest : public testing::Test {
 public:
  AttributionStorageSqlTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  void OpenDatabase() {
    storage_.reset();
    auto delegate = std::make_unique<ConfigurableStorageDelegate>();
    delegate_ = delegate.get();
    storage_ = std::make_unique<AttributionStorageSql>(
        temp_directory_.GetPath(), std::move(delegate));
  }

  void CloseDatabase() { storage_.reset(); }

  void AddReportToStorage() {
    storage_->StoreSource(SourceBuilder().Build());
    storage_->MaybeCreateAndStoreReport(DefaultTrigger());
  }

  void ExpectAllTablesEmpty() {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    static constexpr const char* kTables[] = {
        "conversions",
        "impressions",
        "rate_limits",
        "dedup_keys",
        "aggregatable_report_metadata",
        "aggregatable_contributions",
    };

    for (const char* table : kTables) {
      size_t rows;
      sql::test::CountTableRows(&raw_db, table, &rows);
      EXPECT_EQ(0u, rows) << table;
    }
  }

  base::FilePath db_path() {
    return temp_directory_.GetPath().Append(FILE_PATH_LITERAL("Conversions"));
  }

  AttributionStorage* storage() { return storage_.get(); }

  ConfigurableStorageDelegate* delegate() { return delegate_; }

  void ExpectImpressionRows(size_t expected) {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));
    size_t rows;
    sql::test::CountTableRows(&raw_db, "impressions", &rows);
    EXPECT_EQ(expected, rows);
  }

  void ExpectAggregatableReportMetadataRows(size_t expected) {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));
    size_t rows;
    sql::test::CountTableRows(&raw_db, "aggregatable_report_metadata", &rows);
    EXPECT_EQ(expected, rows);
  }

  AttributionTrigger::Result MaybeCreateAndStoreReport(
      const AttributionTrigger& conversion) {
    return storage_->MaybeCreateAndStoreReport(conversion).status();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_directory_;

 private:
  std::unique_ptr<AttributionStorage> storage_;
  raw_ptr<ConfigurableStorageDelegate> delegate_ = nullptr;
};

}  // namespace

TEST_F(AttributionStorageSqlTest,
       DatabaseInitialized_TablesAndIndexesLazilyInitialized) {
  base::HistogramTester histograms;

  OpenDatabase();
  CloseDatabase();

  // An unused AttributionStorageSql instance should not create the database.
  EXPECT_FALSE(base::PathExists(db_path()));

  // Operations which don't need to run on an empty database should not create
  // the database.
  OpenDatabase();
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());
  CloseDatabase();

  EXPECT_FALSE(base::PathExists(db_path()));

  // DB init UMA should not be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 0);

  // Storing an impression should create and initialize the database.
  OpenDatabase();
  storage()->StoreSource(SourceBuilder().Build());
  CloseDatabase();

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 1);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 0);

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    // [impressions], [conversions], [meta], [rate_limits], [dedup_keys],
    // [aggregatable_report_metadata], [aggregatable_contributions],
    // [sqlite_sequence] (for AUTOINCREMENT support).
    EXPECT_EQ(8u, sql::test::CountSQLTables(&raw_db));

    // [conversion_domain_idx], [impression_expiry_idx],
    // [impression_origin_idx], [impression_site_idx],
    // [conversion_report_time_idx], [conversion_impression_id_idx],
    // [rate_limit_attribution_idx], [rate_limit_reporting_origin_idx],
    // [rate_limit_time_idx], [rate_limit_impression_id_idx],
    // [aggregate_source_id_idx], [aggregate_trigger_time_idx],
    // [contribution_aggregation_id_idx], [contribution_report_time_idx] and
    // the meta table index.
    EXPECT_EQ(15u, sql::test::CountSQLIndices(&raw_db));
  }
}

TEST_F(AttributionStorageSqlTest, DatabaseReopened_DataPersisted) {
  OpenDatabase();
  AddReportToStorage();
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), SizeIs(1));
  CloseDatabase();
  OpenDatabase();
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), SizeIs(1));
}

TEST_F(AttributionStorageSqlTest, CorruptDatabase_RecoveredOnOpen) {
  OpenDatabase();
  AddReportToStorage();
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), SizeIs(1));
  CloseDatabase();

  // Corrupt the database.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(db_path()));

  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(SQLITE_CORRUPT);

  // Open that database and ensure that it does not fail.
  EXPECT_NO_FATAL_FAILURE(OpenDatabase());

  // Data should be recovered.
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), SizeIs(1));

  EXPECT_TRUE(expecter.SawExpectedErrors());
}

TEST_F(AttributionStorageSqlTest, VersionTooNew_RazesDB) {
  OpenDatabase();
  AddReportToStorage();
  ASSERT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), SizeIs(1));
  CloseDatabase();

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    sql::MetaTable meta;
    // The values here are irrelevant, as the meta table already exists.
    ASSERT_TRUE(meta.Init(&raw_db, /*version=*/1, /*compatible_version=*/1));

    meta.SetVersionNumber(meta.GetVersionNumber() + 1);
    meta.SetCompatibleVersionNumber(meta.GetCompatibleVersionNumber() + 1);
  }

  // The DB should be razed because the version is too new.
  ASSERT_NO_FATAL_FAILURE(OpenDatabase());
  ASSERT_THAT(storage()->GetAttributionsToReport(base::Time::Now()), IsEmpty());
}

// Create an impression with three conversions and craft a query that will
// target all.
TEST_F(AttributionStorageSqlTest, ClearDataRangeMultipleReports) {
  base::HistogramTester histograms;

  OpenDatabase();

  base::Time start = base::Time::Now();
  auto impression = SourceBuilder(start).SetExpiry(base::Days(30)).Build();
  storage()->StoreSource(impression);

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  // Use a time range that targets all conversions.
  storage()->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindRepeating(std::equal_to<url::Origin>(),
                          impression.common_info().impression_origin()));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), IsEmpty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();

  histograms.ExpectUniqueSample(
      "Conversions.ImpressionsDeletedInDataClearOperation", 1, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation", 3, 1);
}

//  Create an impression with two conversions (C1 and C2). Craft a query that
//  will target C2, which will in turn delete the impression. We should ensure
//  that C1 is properly deleted (conversions should not be stored unattributed).
TEST_F(AttributionStorageSqlTest, ClearDataWithVestigialConversion) {
  base::HistogramTester histograms;

  OpenDatabase();

  base::Time start = base::Time::Now();
  auto impression = SourceBuilder(start).SetExpiry(base::Days(30)).Build();
  storage()->StoreSource(impression);

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  // Use a time range that only intersects the last conversion.
  storage()->ClearData(
      base::Time::Now(), base::Time::Now(),
      base::BindRepeating(std::equal_to<url::Origin>(),
                          impression.common_info().impression_origin()));
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), IsEmpty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();

  histograms.ExpectUniqueSample(
      "Conversions.ImpressionsDeletedInDataClearOperation", 1, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation", 2, 1);
}

// Same as the above test, but with a null filter.
TEST_F(AttributionStorageSqlTest, ClearAllDataWithVestigialConversion) {
  base::HistogramTester histograms;

  OpenDatabase();

  base::Time start = base::Time::Now();
  auto impression = SourceBuilder(start).SetExpiry(base::Days(30)).Build();
  storage()->StoreSource(impression);

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  // Use a time range that only intersects the last conversion.
  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(base::Time::Now(), base::Time::Now(), null_filter);
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), IsEmpty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();

  histograms.ExpectUniqueSample(
      "Conversions.ImpressionsDeletedInDataClearOperation", 1, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation", 2, 1);
}

// The max time range with a null filter should delete everything.
TEST_F(AttributionStorageSqlTest, DeleteEverything) {
  base::HistogramTester histograms;

  OpenDatabase();

  base::Time start = base::Time::Now();
  for (int i = 0; i < 10; i++) {
    auto impression = SourceBuilder(start).SetExpiry(base::Days(30)).Build();
    storage()->StoreSource(impression);
    task_environment_.FastForwardBy(base::Days(1));
  }

  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  auto null_filter = base::RepeatingCallback<bool(const url::Origin&)>();
  storage()->ClearData(base::Time::Min(), base::Time::Max(), null_filter);
  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Max()), IsEmpty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();

  histograms.ExpectUniqueSample(
      "Conversions.ImpressionsDeletedInDataClearOperation", 1, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation", 2, 1);
}

// Create a source with three aggregatable attributions and craft a query that
// will target all.
TEST_F(AttributionStorageSqlTest,
       ClearDataRangeMultipleAggregatableAttributions) {
  OpenDatabase();

  auto source = SourceBuilder().SetExpiry(base::Days(30)).Build();
  storage()->StoreSource(source);

  StoredSource::Id source_id(1);

  task_environment_.FastForwardBy(base::Days(1));
  AggregatableAttribution aggregatable_attribution_1(
      source_id, /*trigger_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now() + base::Hours(2),
      /*contributions=*/
      {HistogramContribution(/*bucket=*/"1", /*value=*/2)});
  EXPECT_TRUE(storage()->AddAggregatableAttributionForTesting(
      aggregatable_attribution_1));

  task_environment_.FastForwardBy(base::Days(1));
  AggregatableAttribution aggregatable_attribution_2(
      source_id, /*trigger_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now() + base::Hours(2),
      /*contributions=*/
      {HistogramContribution(/*bucket=*/"3", /*value=*/4),
       HistogramContribution(/*bucket=*/"5", /*value=*/6)});
  EXPECT_TRUE(storage()->AddAggregatableAttributionForTesting(
      aggregatable_attribution_2));

  task_environment_.FastForwardBy(base::Days(1));
  AggregatableAttribution aggregatable_attribution_3(
      source_id, /*trigger_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now() + base::Hours(2),
      /*contributions=*/
      {HistogramContribution(/*bucket=*/"7", /*value=*/8)});
  EXPECT_TRUE(storage()->AddAggregatableAttributionForTesting(
      aggregatable_attribution_3));

  // Use a time range that targets all aggregatable attributions.
  storage()->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindRepeating(std::equal_to<url::Origin>(),
                          source.common_info().impression_origin()));
  EXPECT_THAT(storage()->GetAggregatableContributionReportsForTesting(
                  base::Time::Max()),
              IsEmpty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();
}

// Create a source with two aggregatable attributions (A1 and A2). Craft a query
// that will target A2, which will in turn delete the source. We should ensure
// that A1 is properly deleted (aggregatable attributions should not be stored
// unattributed).
TEST_F(AttributionStorageSqlTest,
       ClearDataWithVestigialAggregatableAttribution) {
  OpenDatabase();

  auto source = SourceBuilder().SetExpiry(base::Days(30)).Build();
  storage()->StoreSource(source);

  StoredSource::Id source_id(1);

  task_environment_.FastForwardBy(base::Days(1));
  AggregatableAttribution aggregatable_attribution_1(
      source_id, /*trigger_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now() + base::Hours(2),
      /*contributions=*/
      {HistogramContribution(/*bucket=*/"1", /*value=*/2)});
  EXPECT_TRUE(storage()->AddAggregatableAttributionForTesting(
      aggregatable_attribution_1));

  task_environment_.FastForwardBy(base::Days(1));
  AggregatableAttribution aggregatable_attribution_2(
      source_id, /*trigger_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now() + base::Hours(2),
      /*contributions=*/
      {HistogramContribution(/*bucket=*/"3", /*value=*/4),
       HistogramContribution(/*bucket=*/"5", /*value=*/6)});
  EXPECT_TRUE(storage()->AddAggregatableAttributionForTesting(
      aggregatable_attribution_2));

  // Use a time range that only intersects the last aggregatable attribution.
  storage()->ClearData(
      base::Time::Now(), base::Time::Now(),
      base::BindRepeating(std::equal_to<url::Origin>(),
                          source.common_info().impression_origin()));
  EXPECT_THAT(storage()->GetAggregatableContributionReportsForTesting(
                  base::Time::Max()),
              IsEmpty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();
}

// Same as the above test, but with a null filter.
TEST_F(AttributionStorageSqlTest,
       ClearAllDataWithVestigialAggregatableAttribution) {
  OpenDatabase();

  storage()->StoreSource(SourceBuilder().SetExpiry(base::Days(30)).Build());

  StoredSource::Id source_id(1);

  task_environment_.FastForwardBy(base::Days(1));
  AggregatableAttribution aggregatable_attribution_1(
      source_id, /*trigger_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now() + base::Hours(2),
      /*contributions=*/
      {HistogramContribution(/*bucket=*/"1", /*value=*/2)});
  EXPECT_TRUE(storage()->AddAggregatableAttributionForTesting(
      aggregatable_attribution_1));

  task_environment_.FastForwardBy(base::Days(1));
  AggregatableAttribution aggregatable_attribution_2(
      source_id, /*trigger_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now() + base::Hours(2),
      /*contributions=*/
      {HistogramContribution(/*bucket=*/"3", /*value=*/4),
       HistogramContribution(/*bucket=*/"5", /*value=*/6)});
  EXPECT_TRUE(storage()->AddAggregatableAttributionForTesting(
      aggregatable_attribution_2));

  // Use a time range that only intersects the last aggregatable attribution.
  storage()->ClearData(base::Time::Now(), base::Time::Now(),
                       base::NullCallback());
  EXPECT_THAT(storage()->GetAggregatableContributionReportsForTesting(
                  base::Time::Max()),
              IsEmpty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();
}

// The max time range with a null filter should delete everything.
TEST_F(AttributionStorageSqlTest, DeleteEverythingWithAggregatableAttribution) {
  OpenDatabase();

  base::Time start = base::Time::Now();
  for (int i = 0; i < 5; i++) {
    storage()->StoreSource(
        SourceBuilder(start).SetExpiry(base::Days(30)).Build());
    task_environment_.FastForwardBy(base::Days(1));
  }

  StoredSource::Id source_id(1);

  AggregatableAttribution aggregatable_attribution_1(
      source_id, /*trigger_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now() + base::Hours(2),
      /*contributions=*/
      {HistogramContribution(/*bucket=*/"1", /*value=*/2),
       HistogramContribution(/*bucket=*/"3", /*value=*/4)});
  EXPECT_TRUE(storage()->AddAggregatableAttributionForTesting(
      aggregatable_attribution_1));

  task_environment_.FastForwardBy(base::Days(1));
  AggregatableAttribution aggregatable_attribution_2(
      source_id, /*trigger_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now() + base::Hours(2),
      /*contributions=*/
      {HistogramContribution(/*bucket=*/"5", /*value=*/6)});
  EXPECT_TRUE(storage()->AddAggregatableAttributionForTesting(
      aggregatable_attribution_2));

  storage()->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback());
  EXPECT_THAT(storage()->GetAggregatableContributionReportsForTesting(
                  base::Time::Max()),
              IsEmpty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();
}

TEST_F(AttributionStorageSqlTest, MaxSourcesPerOrigin) {
  OpenDatabase();
  delegate()->set_max_sources_per_origin(2);
  storage()->StoreSource(SourceBuilder().Build());
  storage()->StoreSource(SourceBuilder().Build());
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  CloseDatabase();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));
  size_t impression_rows;
  sql::test::CountTableRows(&raw_db, "impressions", &impression_rows);
  EXPECT_EQ(1u, impression_rows);
  size_t rate_limit_rows;
  sql::test::CountTableRows(&raw_db, "rate_limits", &rate_limit_rows);
  EXPECT_EQ(3u, rate_limit_rows);
}

TEST_F(AttributionStorageSqlTest, MaxAttributionsPerOrigin) {
  OpenDatabase();
  delegate()->set_max_attributions_per_origin(2);
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));
  EXPECT_EQ(AttributionTrigger::Result::kNoCapacityForConversionDestination,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  CloseDatabase();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));
  size_t conversion_rows;
  sql::test::CountTableRows(&raw_db, "conversions", &conversion_rows);
  EXPECT_EQ(2u, conversion_rows);
  size_t rate_limit_rows;
  sql::test::CountTableRows(&raw_db, "rate_limits", &rate_limit_rows);
  EXPECT_EQ(3u, rate_limit_rows);
}

TEST_F(AttributionStorageSqlTest,
       DeleteRateLimitRowsForSubdomainImpressionOrigin) {
  OpenDatabase();
  delegate()->set_max_attributions_per_source(1);
  delegate()->rate_limits().time_window = base::Days(7);
  const url::Origin impression_origin =
      url::Origin::Create(GURL("https://sub.impression.example/"));
  const url::Origin reporting_origin =
      url::Origin::Create(GURL("https://a.example/"));
  const url::Origin conversion_origin =
      url::Origin::Create(GURL("https://b.example/"));
  storage()->StoreSource(SourceBuilder()
                             .SetExpiry(base::Days(30))
                             .SetImpressionOrigin(impression_origin)
                             .SetReportingOrigin(reporting_origin)
                             .SetConversionOrigin(conversion_origin)
                             .Build());

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(
      AttributionTrigger::Result::kSuccess,
      MaybeCreateAndStoreReport(
          TriggerBuilder()
              .SetConversionDestination(net::SchemefulSite(conversion_origin))
              .SetReportingOrigin(reporting_origin)
              .Build()));
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  // Force the impression to be deactivated by ensuring that the next report is
  // in a different window.
  delegate()->set_report_delay(base::Milliseconds(1));
  EXPECT_EQ(
      AttributionTrigger::Result::kPriorityTooLow,
      MaybeCreateAndStoreReport(
          TriggerBuilder()
              .SetConversionDestination(net::SchemefulSite(conversion_origin))
              .SetReportingOrigin(reporting_origin)
              .Build()));
  EXPECT_THAT(storage()->GetActiveSources(), IsEmpty());

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(
      storage()->DeleteReport(AttributionReport::EventLevelData::Id(1)));
  storage()->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindRepeating(std::equal_to<url::Origin>(), impression_origin));

  CloseDatabase();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));
  size_t conversion_rows;
  sql::test::CountTableRows(&raw_db, "conversions", &conversion_rows);
  EXPECT_EQ(0u, conversion_rows);
  size_t rate_limit_rows;
  sql::test::CountTableRows(&raw_db, "rate_limits", &rate_limit_rows);
  EXPECT_EQ(0u, rate_limit_rows);
}

TEST_F(AttributionStorageSqlTest,
       DeleteRateLimitRowsForSubdomainConversionOrigin) {
  OpenDatabase();
  delegate()->set_max_attributions_per_source(1);
  delegate()->rate_limits().time_window = base::Days(7);
  const url::Origin impression_origin =
      url::Origin::Create(GURL("https://b.example/"));
  const url::Origin reporting_origin =
      url::Origin::Create(GURL("https://a.example/"));
  const url::Origin conversion_origin =
      url::Origin::Create(GURL("https://sub.impression.example/"));
  storage()->StoreSource(SourceBuilder()
                             .SetExpiry(base::Days(30))
                             .SetImpressionOrigin(impression_origin)
                             .SetReportingOrigin(reporting_origin)
                             .SetConversionOrigin(conversion_origin)
                             .Build());

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(
      AttributionTrigger::Result::kSuccess,
      MaybeCreateAndStoreReport(
          TriggerBuilder()
              .SetConversionDestination(net::SchemefulSite(conversion_origin))
              .SetReportingOrigin(reporting_origin)
              .Build()));
  EXPECT_THAT(storage()->GetActiveSources(), SizeIs(1));

  // Force the impression to be deactivated by ensuring that the next report is
  // in a different window.
  delegate()->set_report_delay(base::Milliseconds(1));
  EXPECT_EQ(
      AttributionTrigger::Result::kPriorityTooLow,
      MaybeCreateAndStoreReport(
          TriggerBuilder()
              .SetConversionDestination(net::SchemefulSite(conversion_origin))
              .SetReportingOrigin(reporting_origin)
              .Build()));
  EXPECT_THAT(storage()->GetActiveSources(), IsEmpty());

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(
      storage()->DeleteReport(AttributionReport::EventLevelData::Id(1)));
  storage()->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindRepeating(std::equal_to<url::Origin>(), conversion_origin));

  CloseDatabase();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));
  size_t conversion_rows;
  sql::test::CountTableRows(&raw_db, "conversions", &conversion_rows);
  EXPECT_EQ(0u, conversion_rows);
  size_t rate_limit_rows;
  sql::test::CountTableRows(&raw_db, "rate_limits", &rate_limit_rows);
  EXPECT_EQ(0u, rate_limit_rows);
}

TEST_F(AttributionStorageSqlTest, CantOpenDb_FailsSilentlyInRelease) {
  base::CreateDirectoryAndGetError(db_path(), nullptr);

  auto sql_storage = std::make_unique<AttributionStorageSql>(
      temp_directory_.GetPath(),
      std::make_unique<ConfigurableStorageDelegate>());
  sql_storage->set_ignore_errors_for_testing(true);

  std::unique_ptr<AttributionStorage> storage = std::move(sql_storage);

  // These calls should be no-ops.
  storage->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kNoMatchingImpressions,
            storage->MaybeCreateAndStoreReport(DefaultTrigger()).status());
}

TEST_F(AttributionStorageSqlTest, DatabaseDirDoesExist_CreateDirAndOpenDB) {
  // Give the storage layer a database directory that doesn't exist.
  std::unique_ptr<AttributionStorage> storage =
      std::make_unique<AttributionStorageSql>(
          temp_directory_.GetPath().Append(
              FILE_PATH_LITERAL("ConversionFolder/")),
          std::make_unique<ConfigurableStorageDelegate>());

  // The directory should be created, and the database opened.
  storage->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            storage->MaybeCreateAndStoreReport(DefaultTrigger()).status());
}

TEST_F(AttributionStorageSqlTest, DBinitializationSucceeds_HistogramRecorded) {
  base::HistogramTester histograms;

  OpenDatabase();
  storage()->StoreSource(SourceBuilder().Build());
  CloseDatabase();

  histograms.ExpectUniqueSample("Conversions.Storage.Sql.InitStatus2",
                                AttributionStorageSql::InitStatus::kSuccess, 1);
}

TEST_F(AttributionStorageSqlTest, MaxUint64StorageSucceeds) {
  constexpr uint64_t kMaxUint64 = std::numeric_limits<uint64_t>::max();

  OpenDatabase();

  // Ensure that reading and writing `uint64_t` fields via
  // `sql::Statement::ColumnInt64()` and `sql::Statement::BindInt64()` works
  // with the maximum value.

  const auto impression = SourceBuilder().SetSourceEventId(kMaxUint64).Build();
  storage()->StoreSource(impression);
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(CommonSourceInfoIs(impression.common_info())));

  EXPECT_EQ(
      AttributionTrigger::Result::kSuccess,
      MaybeCreateAndStoreReport(
          TriggerBuilder()
              .SetTriggerData(kMaxUint64)
              .SetConversionDestination(
                  impression.common_info().ConversionDestination())
              .SetReportingOrigin(impression.common_info().reporting_origin())
              .Build()));

  EXPECT_THAT(storage()->GetAttributionsToReport(base::Time::Now()),
              ElementsAre(EventLevelDataIs(TriggerDataIs(kMaxUint64))));
}

TEST_F(AttributionStorageSqlTest, ImpressionNotExpired_NotDeleted) {
  OpenDatabase();

  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest, ImpressionExpired_Deleted) {
  OpenDatabase();

  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());
  task_environment_.FastForwardBy(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(1u);
}

TEST_F(AttributionStorageSqlTest, ImpressionExpired_TooFrequent_NotDeleted) {
  OpenDatabase();

  delegate()->set_delete_expired_sources_frequency(base::Milliseconds(4));

  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());
  task_environment_.FastForwardBy(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest,
       ExpiredImpressionWithPendingConversion_NotDeleted) {
  OpenDatabase();

  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest, TwoImpressionsOneExpired_OneDeleted) {
  OpenDatabase();

  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(4)).Build());

  task_environment_.FastForwardBy(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest, ExpiredImpressionWithSentConversion_Deleted) {
  OpenDatabase();

  const base::TimeDelta kReportDelay = base::Milliseconds(5);
  delegate()->set_report_delay(kReportDelay);

  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());
  EXPECT_EQ(AttributionTrigger::Result::kSuccess,
            MaybeCreateAndStoreReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(3));
  // Advance past the default report time.
  task_environment_.FastForwardBy(kReportDelay);

  std::vector<AttributionReport> reports =
      storage()->GetAttributionsToReport(base::Time::Now());
  EXPECT_THAT(reports, SizeIs(1));
  EXPECT_TRUE(storage()->DeleteReport(
      *(absl::get<AttributionReport::EventLevelData>(reports[0].data()).id)));
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(1u);
}

TEST_F(AttributionStorageSqlTest, DeleteAggregatableContributionReport) {
  OpenDatabase();

  storage()->StoreSource(SourceBuilder().Build());

  AggregatableAttribution aggregatable_attribution(
      StoredSource::Id(1), /*trigger_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now() + base::Hours(2),
      /*contributions=*/
      {HistogramContribution(/*bucket=*/"1", /*value=*/2),
       HistogramContribution(/*bucket=*/"3", /*value=*/4)});
  EXPECT_TRUE(storage()->AddAggregatableAttributionForTesting(
      aggregatable_attribution));

  EXPECT_TRUE(storage()->DeleteReport(
      AttributionReport::AggregatableContributionData::Id(1)));

  const HistogramContribution& contribution =
      aggregatable_attribution.contributions[1];
  EXPECT_THAT(
      storage()->GetAggregatableContributionReportsForTesting(
          base::Time::Max()),
      ElementsAre(AttributionReport(
          AttributionInfo(SourceBuilder().BuildStored(),
                          aggregatable_attribution.trigger_time,
                          /*debug_key=*/absl::nullopt),
          aggregatable_attribution.report_time, DefaultExternalReportID(),
          AttributionReport::AggregatableContributionData(
              HistogramContribution(contribution.bucket(),
                                    contribution.value()),
              AttributionReport::AggregatableContributionData::Id(2)))));

  EXPECT_TRUE(storage()->DeleteReport(
      AttributionReport::AggregatableContributionData::Id(2)));
  EXPECT_THAT(storage()->GetAggregatableContributionReportsForTesting(
                  base::Time::Max()),
              IsEmpty());

  CloseDatabase();

  ExpectAggregatableReportMetadataRows(0u);
}

TEST_F(AttributionStorageSqlTest,
       ExpiredSourceWithPendingAggregatableAttribution_NotDeleted) {
  OpenDatabase();

  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  AggregatableAttribution aggregatable_attribution(
      StoredSource::Id(1), /*trigger_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now() + base::Hours(2),
      /*contributions=*/
      {HistogramContribution(/*bucket=*/"1", /*value=*/2)});
  EXPECT_TRUE(storage()->AddAggregatableAttributionForTesting(
      aggregatable_attribution));

  task_environment_.FastForwardBy(base::Milliseconds(3));
  // Store another source to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest,
       ExpiredSourceWithSentAggregatableAttribution_Deleted) {
  OpenDatabase();

  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  AggregatableAttribution aggregatable_attribution(
      StoredSource::Id(1), /*trigger_time=*/base::Time::Now(),
      /*report_time=*/base::Time::Now() + base::Hours(2),
      /*contributions=*/
      {HistogramContribution(/*bucket=*/"1", /*value=*/2)});
  EXPECT_TRUE(storage()->AddAggregatableAttributionForTesting(
      aggregatable_attribution));

  task_environment_.FastForwardBy(base::Milliseconds(3));

  EXPECT_TRUE(storage()->DeleteReport(
      AttributionReport::AggregatableContributionData::Id(1)));

  // Store another source to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(1u);
}

}  // namespace content
