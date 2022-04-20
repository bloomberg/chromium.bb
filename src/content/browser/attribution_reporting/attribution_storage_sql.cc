// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql.h"

#include <stdint.h>

#include <iterator>
#include <limits>
#include <string>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_source.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/rate_limit_result.h"
#include "content/browser/attribution_reporting/sql_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "sql/database.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/transaction.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/origin.h"

namespace content {

// Version number of the database.
const int AttributionStorageSql::kCurrentVersionNumber = 34;

// Earliest version which can use a |kCurrentVersionNumber| database
// without failing.
const int AttributionStorageSql::kCompatibleVersionNumber = 34;

// Latest version of the database that cannot be upgraded to
// |kCurrentVersionNumber| without razing the database.
//
// Note that all versions >=15 were introduced during the transitional state of
// the Attribution Reporting API and can be removed when done.
const int AttributionStorageSql::kDeprecatedVersionNumber = 32;

namespace {

using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;
using EventLevelResult = ::content::AttributionTrigger::EventLevelResult;

const base::FilePath::CharType kInMemoryPath[] = FILE_PATH_LITERAL(":memory");

const base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("Conversions");

#define ATTRIBUTION_CONVERSIONS_TABLE "event_level_reports"

#define ATTRIBUTION_AGGREGATABLE_REPORT_METADATA_TABLE \
  "aggregatable_report_metadata"

#define ATTRIBUTION_UPDATE_FAILED_REPORT_SQL(table, column) \
  "UPDATE " table                                           \
  " SET report_time=?,"                                     \
  "failed_send_attempts=failed_send_attempts+1 "            \
  "WHERE " column "=?"

#define ATTRIBUTION_NEXT_REPORT_TIME_SQL(table) \
  "SELECT MIN(report_time) FROM " table " WHERE report_time>?"

// Set the report time for all reports that should have been sent before now
// to now + a random number of microseconds between `min_delay` and
// `max_delay`, both inclusive. We use RANDOM, instead of a method on the
// delegate, to avoid having to pull all reports into memory and update them
// one by one. We use ABS because RANDOM may return a negative integer. We add
// 1 to the difference between `max_delay` and `min_delay` to ensure that the
// range of generated values is inclusive. If `max_delay == min_delay`, we
// take the remainder modulo 1, which is always 0.
#define ATTRIBUTION_SET_REPORT_TIME_SQL(table) \
  "UPDATE " table                              \
  " SET report_time=?+ABS(RANDOM()%?) "        \
  "WHERE report_time<?"

// clang-format off

#define ATTRIBUTION_SOURCE_COLUMNS_SQL(prefix) \
  prefix "source_id," \
  prefix "source_event_id," \
  prefix "source_origin," \
  prefix "destination_origin," \
  prefix "reporting_origin," \
  prefix "source_time," \
  prefix "expiry_time," \
  prefix "source_type," \
  prefix "attribution_logic," \
  prefix "priority," \
  prefix "debug_key," \
  prefix "num_attributions," \
  prefix "aggregatable_budget_consumed," \
  prefix "aggregatable_source," \
  prefix "filter_data," \
  prefix "event_level_active," \
  prefix "aggregatable_active"

#define ATTRIBUTION_SELECT_EVENT_LEVEL_REPORT_AND_SOURCE_COLUMNS_SQL \
  "SELECT "                                                                  \
  ATTRIBUTION_SOURCE_COLUMNS_SQL("I.") \
  ",C.trigger_data,C.trigger_time,C.report_time,C.report_id,"       \
  "C.priority,C.failed_send_attempts,C.external_report_id,C.debug_key "      \
  "FROM event_level_reports C "                                                      \
  "JOIN sources I ON C.source_id = I.source_id "

#define ATTRIBUTION_SELECT_AGGREGATABLE_REPORT_AND_SOURCE_COLUMNS_SQL  \
  "SELECT "                                                            \
  ATTRIBUTION_SOURCE_COLUMNS_SQL("I.") \
  ",A.aggregation_id,A.trigger_time,A.report_time,A.debug_key,"        \
  "A.external_report_id,A.failed_send_attempts,A.initial_report_time " \
  "FROM aggregatable_report_metadata A "                               \
  DCHECK_SQL_INDEXED_BY("aggregate_report_time_idx")                   \
  "JOIN sources I ON A.source_id = I.source_id "

// clang-format on

void RecordInitializationStatus(
    const AttributionStorageSql::InitStatus status) {
  base::UmaHistogramEnumeration("Conversions.Storage.Sql.InitStatus2", status);
}

void RecordSourcesDeleted(int count) {
  UMA_HISTOGRAM_COUNTS_1000(
      "Conversions.ImpressionsDeletedInDataClearOperation", count);
}

void RecordReportsDeleted(int count) {
  UMA_HISTOGRAM_COUNTS_1000("Conversions.ReportsDeletedInDataClearOperation",
                            count);
}

int SerializeAttributionLogic(StoredSource::AttributionLogic val) {
  return static_cast<int>(val);
}

absl::optional<StoredSource::AttributionLogic> DeserializeAttributionLogic(
    int val) {
  switch (val) {
    case static_cast<int>(StoredSource::AttributionLogic::kNever):
      return StoredSource::AttributionLogic::kNever;
    case static_cast<int>(StoredSource::AttributionLogic::kTruthfully):
      return StoredSource::AttributionLogic::kTruthfully;
    case static_cast<int>(StoredSource::AttributionLogic::kFalsely):
      return StoredSource::AttributionLogic::kFalsely;
    default:
      return absl::nullopt;
  }
}

int SerializeSourceType(AttributionSourceType val) {
  return static_cast<int>(val);
}

absl::optional<AttributionSourceType> DeserializeSourceType(int val) {
  switch (val) {
    case static_cast<int>(AttributionSourceType::kNavigation):
      return AttributionSourceType::kNavigation;
    case static_cast<int>(AttributionSourceType::kEvent):
      return AttributionSourceType::kEvent;
    default:
      return absl::nullopt;
  }
}

std::string SerializePotentiallyTrustworthyOrigin(const url::Origin& origin) {
  DCHECK(network::IsOriginPotentiallyTrustworthy(origin));
  return SerializeOrigin(origin);
}

url::Origin DeserializePotentiallyTrustworthyOrigin(const std::string& string) {
  url::Origin origin = DeserializeOrigin(string);

  if (!network::IsOriginPotentiallyTrustworthy(origin))
    return url::Origin();

  return origin;
}

absl::optional<StoredSource::ActiveState> GetSourceActiveState(
    bool event_level_active,
    bool aggregatable_active) {
  if (event_level_active && aggregatable_active)
    return StoredSource::ActiveState::kActive;

  if (!event_level_active && !aggregatable_active)
    return StoredSource::ActiveState::kInactive;

  if (!event_level_active)
    return StoredSource::ActiveState::kReachedEventLevelAttributionLimit;

  // We haven't enforced aggregatable attribution limit yet.
  return absl::nullopt;
}

void BindUint64OrNull(sql::Statement& statement,
                      int col,
                      absl::optional<uint64_t> value) {
  if (value.has_value())
    statement.BindInt64(col, SerializeUint64(*value));
  else
    statement.BindNull(col);
}

absl::optional<uint64_t> ColumnUint64OrNull(sql::Statement& statement,
                                            int col) {
  return statement.GetColumnType(col) == sql::ColumnType::kNull
             ? absl::nullopt
             : absl::make_optional(
                   DeserializeUint64(statement.ColumnInt64(col)));
}

absl::optional<AttributionAggregatableSource> ParseAggregatableSource(
    const std::string& str) {
  proto::AttributionAggregatableSource aggregatable_source;
  if (!aggregatable_source.ParseFromString(str))
    return absl::nullopt;

  return AttributionAggregatableSource::Create(std::move(aggregatable_source));
}

struct StoredSourceData {
  StoredSource source;
  int num_conversions;
  int64_t aggregatable_budget_consumed;
};

constexpr int kSourceColumnCount = 17;

// Helper to deserialize source rows. See `GetActiveSources()` for the
// expected ordering of columns used for the input to this function.
absl::optional<StoredSourceData> ReadSourceFromStatement(
    sql::Statement& statement) {
  DCHECK_GE(statement.ColumnCount(), kSourceColumnCount);

  int col = 0;

  StoredSource::Id source_id(statement.ColumnInt64(col++));
  uint64_t source_event_id = DeserializeUint64(statement.ColumnInt64(col++));
  url::Origin impression_origin =
      DeserializePotentiallyTrustworthyOrigin(statement.ColumnString(col++));
  url::Origin conversion_origin =
      DeserializePotentiallyTrustworthyOrigin(statement.ColumnString(col++));
  url::Origin reporting_origin =
      DeserializePotentiallyTrustworthyOrigin(statement.ColumnString(col++));
  base::Time impression_time = statement.ColumnTime(col++);
  base::Time expiry_time = statement.ColumnTime(col++);
  absl::optional<AttributionSourceType> source_type =
      DeserializeSourceType(statement.ColumnInt(col++));
  absl::optional<StoredSource::AttributionLogic> attribution_logic =
      DeserializeAttributionLogic(statement.ColumnInt(col++));
  int64_t priority = statement.ColumnInt64(col++);
  absl::optional<uint64_t> debug_key = ColumnUint64OrNull(statement, col++);
  int num_conversions = statement.ColumnInt(col++);
  int64_t aggregatable_budget_consumed = statement.ColumnInt64(col++);
  absl::optional<AttributionAggregatableSource> aggregatable_source =
      ParseAggregatableSource(statement.ColumnString(col++));

  if (impression_origin.opaque() || conversion_origin.opaque() ||
      reporting_origin.opaque() || !source_type.has_value() ||
      !attribution_logic.has_value() || num_conversions < 0 ||
      aggregatable_budget_consumed < 0 || !aggregatable_source.has_value()) {
    return absl::nullopt;
  }

  absl::optional<AttributionFilterData> filter_data =
      AttributionFilterData::DeserializeSourceFilterData(
          statement.ColumnString(col++), *source_type);
  if (!filter_data)
    return absl::nullopt;

  bool event_level_active = statement.ColumnBool(col++);
  bool aggregatable_active = statement.ColumnBool(col++);
  absl::optional<StoredSource::ActiveState> active_state =
      GetSourceActiveState(event_level_active, aggregatable_active);
  if (!active_state.has_value())
    return absl::nullopt;

  return StoredSourceData{
      .source = StoredSource(
          CommonSourceInfo(source_event_id, std::move(impression_origin),
                           std::move(conversion_origin),
                           std::move(reporting_origin), impression_time,
                           expiry_time, *source_type, priority,
                           std::move(*filter_data), debug_key,
                           std::move(*aggregatable_source)),
          *attribution_logic, *active_state, source_id),
      .num_conversions = num_conversions,
      .aggregatable_budget_consumed = aggregatable_budget_consumed};
}

absl::optional<StoredSourceData> ReadSourceToAttribute(
    sql::Database* db,
    StoredSource::Id source_id) {
  static constexpr char kReadSourceToAttributeSql[] =
      // clang-format off
      "SELECT " ATTRIBUTION_SOURCE_COLUMNS_SQL("")
      " FROM sources "
      "WHERE source_id = ?";  // clang-format on
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kReadSourceToAttributeSql));
  statement.BindInt64(0, *source_id);
  if (!statement.Step())
    return absl::nullopt;

  return ReadSourceFromStatement(statement);
}

absl::optional<base::Time> GetMinTime(absl::optional<base::Time> a,
                                      absl::optional<base::Time> b) {
  if (!a.has_value())
    return b;

  if (!b.has_value())
    return a;

  return std::min(*a, *b);
}

}  // namespace

// static
void AttributionStorageSql::RunInMemoryForTesting() {
  g_run_in_memory_ = true;
}

// static
bool AttributionStorageSql::g_run_in_memory_ = false;

AttributionStorageSql::AttributionStorageSql(
    const base::FilePath& path_to_database,
    std::unique_ptr<AttributionStorageDelegate> delegate)
    : path_to_database_(g_run_in_memory_
                            ? base::FilePath(kInMemoryPath)
                            : path_to_database.Append(kDatabasePath)),
      rate_limit_table_(delegate.get()),
      delegate_(std::move(delegate)) {
  DCHECK(delegate_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AttributionStorageSql::~AttributionStorageSql() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

absl::optional<std::vector<DeactivatedSource>>
AttributionStorageSql::DeactivateSources(
    const std::string& serialized_conversion_destination,
    const std::string& serialized_reporting_origin,
    int return_limit) {
  std::vector<DeactivatedSource> deactivated_sources;

  if (return_limit != 0) {
    // Get at most `return_limit` sources that will be deactivated. We do this
    // first, instead of using a RETURNING clause in the UPDATE, because we
    // cannot limit the number of returned results there, and we want to avoid
    // bringing all results into memory.
    static constexpr char kGetSourcesToReturnSql[] =
        // clang-format off
        "SELECT " ATTRIBUTION_SOURCE_COLUMNS_SQL("")
        " FROM sources "
        DCHECK_SQL_INDEXED_BY("sources_by_active_destination_site_reporting_origin")
        "WHERE destination_site = ? AND reporting_origin = ? AND "
        "((event_level_active = 1 AND num_attributions > 0) OR "
        "(aggregatable_active = 1 AND aggregatable_budget_consumed > 0)) "
        "LIMIT ?"; // clang-format on
    sql::Statement get_statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kGetSourcesToReturnSql));
    get_statement.BindString(0, serialized_conversion_destination);
    get_statement.BindString(1, serialized_reporting_origin);
    get_statement.BindInt(2, return_limit);

    while (get_statement.Step()) {
      absl::optional<StoredSourceData> source_data =
          ReadSourceFromStatement(get_statement);
      if (!source_data.has_value())
        return absl::nullopt;

      deactivated_sources.emplace_back(
          std::move(source_data->source),
          DeactivatedSource::Reason::kReplacedByNewerSource);
    }
    if (!get_statement.Succeeded())
      return absl::nullopt;

    // If nothing was returned, we know the UPDATE below will do nothing, so
    // just return early.
    if (deactivated_sources.empty())
      return deactivated_sources;
  }

  static constexpr char kDeactivateSourcesSql[] =
      "UPDATE sources "
      DCHECK_SQL_INDEXED_BY("sources_by_active_destination_site_reporting_origin")
      "SET event_level_active = 0,aggregatable_active = 0 "
      "WHERE destination_site = ? AND reporting_origin = ? AND "
      "((event_level_active = 1 AND num_attributions > 0) OR "
      "(aggregatable_active = 1 AND aggregatable_budget_consumed > 0))";
  sql::Statement deactivate_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeactivateSourcesSql));
  deactivate_statement.BindString(0, serialized_conversion_destination);
  deactivate_statement.BindString(1, serialized_reporting_origin);

  if (!deactivate_statement.Run())
    return absl::nullopt;

  for (auto& deactivated_source : deactivated_sources) {
    absl::optional<std::vector<uint64_t>> dedup_keys =
        ReadDedupKeys(deactivated_source.source.source_id());
    if (!dedup_keys.has_value())
      return absl::nullopt;
    deactivated_source.source.SetDedupKeys(std::move(*dedup_keys));
  }

  return deactivated_sources;
}

AttributionStorage::StoreSourceResult AttributionStorageSql::StoreSource(
    const StorableSource& source,
    int deactivated_source_return_limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Force the creation of the database if it doesn't exist, as we need to
  // persist the source.
  if (!LazyInit(DbCreationPolicy::kCreateIfAbsent))
    return StoreSourceResult(StorableSource::Result::kInternalError);

  // Only delete expired impressions periodically to avoid excessive DB
  // operations.
  const base::TimeDelta delete_frequency =
      delegate_->GetDeleteExpiredSourcesFrequency();
  DCHECK_GE(delete_frequency, base::TimeDelta());
  const base::Time now = base::Time::Now();
  if (now - last_deleted_expired_sources_ >= delete_frequency) {
    if (!DeleteExpiredSources())
      return StoreSourceResult(StorableSource::Result::kInternalError);
    last_deleted_expired_sources_ = now;
  }

  const CommonSourceInfo& common_info = source.common_info();

  const std::string serialized_impression_origin =
      SerializePotentiallyTrustworthyOrigin(common_info.impression_origin());
  if (!HasCapacityForStoringSource(serialized_impression_origin)) {
    return StoreSourceResult(
        StorableSource::Result::kInsufficientSourceCapacity);
  }

  if (!HasCapacityForUniqueDestinationLimitForPendingSource(source)) {
    return StoreSourceResult(
        StorableSource::Result::kInsufficientUniqueDestinationCapacity);
  }

  switch (rate_limit_table_.SourceAllowedForReportingOriginLimit(db_.get(),
                                                                 source)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      return StoreSourceResult(
          StorableSource::Result::kExcessiveReportingOrigins);
    case RateLimitResult::kError:
      return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  // Wrap the deactivation and insertion in the same transaction. If the
  // deactivation fails, we do not want to store the new source as we may
  // return the wrong set of sources for a trigger.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return StoreSourceResult(StorableSource::Result::kInternalError);

  const std::string serialized_conversion_destination =
      common_info.ConversionDestination().Serialize();
  const std::string serialized_reporting_origin =
      SerializePotentiallyTrustworthyOrigin(common_info.reporting_origin());

  // In the case where we get a new source for a given <reporting_origin,
  // conversion_destination> we should mark all active, converted impressions
  // with the matching <reporting_origin, conversion_destination> as not active.
  absl::optional<std::vector<DeactivatedSource>> deactivated_sources =
      DeactivateSources(serialized_conversion_destination,
                        serialized_reporting_origin,
                        deactivated_source_return_limit);
  if (!deactivated_sources.has_value())
    return StoreSourceResult(StorableSource::Result::kInternalError);

  AttributionStorageDelegate::RandomizedResponse randomized_response =
      delegate_->GetRandomizedResponse(common_info);

  int num_conversions = 0;
  auto attribution_logic = StoredSource::AttributionLogic::kTruthfully;
  bool event_level_active = true;
  if (randomized_response.has_value()) {
    num_conversions = randomized_response->size();
    attribution_logic = num_conversions == 0
                            ? StoredSource::AttributionLogic::kNever
                            : StoredSource::AttributionLogic::kFalsely;
    event_level_active = num_conversions == 0;
  }
  // Aggregatable reports are not subject to `attribution_logic`.
  const bool aggregatable_active = true;

  static constexpr char kInsertImpressionSql[] =
      "INSERT INTO sources"
      "(source_event_id,source_origin,destination_origin,"
      "destination_site,"
      "reporting_origin,source_time,expiry_time,source_type,"
      "attribution_logic,priority,source_site,"
      "num_attributions,event_level_active,aggregatable_active,debug_key,"
      "aggregatable_budget_consumed,aggregatable_source,filter_data)"
      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,0,?,?)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertImpressionSql));
  statement.BindInt64(0, SerializeUint64(common_info.source_event_id()));
  statement.BindString(1, serialized_impression_origin);
  statement.BindString(2, SerializePotentiallyTrustworthyOrigin(
                              common_info.conversion_origin()));
  statement.BindString(3, serialized_conversion_destination);
  statement.BindString(4, serialized_reporting_origin);
  statement.BindTime(5, common_info.impression_time());
  statement.BindTime(6, common_info.expiry_time());
  statement.BindInt(7, SerializeSourceType(common_info.source_type()));
  statement.BindInt(8, SerializeAttributionLogic(attribution_logic));
  statement.BindInt64(9, common_info.priority());
  statement.BindString(10, common_info.ImpressionSite().Serialize());
  statement.BindInt(11, num_conversions);
  statement.BindBool(12, event_level_active);
  statement.BindBool(13, aggregatable_active);

  BindUint64OrNull(statement, 14, common_info.debug_key());

  absl::optional<StoredSource::ActiveState> active_state =
      GetSourceActiveState(event_level_active, aggregatable_active);
  DCHECK(active_state.has_value());

  statement.BindBlob(
      15, common_info.aggregatable_source().proto().SerializeAsString());
  statement.BindBlob(16, common_info.filter_data().Serialize());

  if (!statement.Run())
    return StoreSourceResult(StorableSource::Result::kInternalError);

  const StoredSource::Id source_id(db_->GetLastInsertRowId());
  const StoredSource stored_source(source.common_info(), attribution_logic,
                                   *active_state, source_id);

  if (!rate_limit_table_.AddRateLimitForSource(db_.get(), stored_source))
    return StoreSourceResult(StorableSource::Result::kInternalError);

  absl::optional<base::Time> min_fake_report_time;

  if (attribution_logic == StoredSource::AttributionLogic::kFalsely) {
    const base::Time trigger_time = common_info.impression_time();

    for (const auto& fake_report : *randomized_response) {
      DCHECK_EQ(fake_report.trigger_data,
                delegate_->SanitizeTriggerData(fake_report.trigger_data,
                                               common_info.source_type()));

      if (!StoreEventLevelReport(source_id, fake_report.trigger_data,
                                 trigger_time, fake_report.report_time,
                                 /*priority=*/0, delegate_->NewReportID(),
                                 /*trigger_debug_key=*/absl::nullopt)) {
        return StoreSourceResult(StorableSource::Result::kInternalError);
      }

      if (!min_fake_report_time.has_value() ||
          fake_report.report_time < *min_fake_report_time) {
        min_fake_report_time = fake_report.report_time;
      }
    }
  }

  if (!transaction.Commit())
    return StoreSourceResult(StorableSource::Result::kInternalError);

  return StoreSourceResult(StorableSource::Result::kSuccess,
                           std::move(*deactivated_sources),
                           min_fake_report_time);
}

// Checks whether a new report is allowed to be stored for the given source
// based on `GetMaxAttributionsPerSource()`. If there's sufficient capacity,
// the new report should be stored. Otherwise, if all existing reports were from
// an earlier window, the corresponding source is deactivated and the new
// report should be dropped. Otherwise, If there's insufficient capacity, checks
// the new report's priority against all existing ones for the same source.
// If all existing ones have greater priority, the new report should be dropped;
// otherwise, the existing one with the lowest priority is deleted and the new
// one should be stored.
AttributionStorageSql::MaybeReplaceLowerPriorityEventLevelReportResult
AttributionStorageSql::MaybeReplaceLowerPriorityEventLevelReport(
    const AttributionReport& report,
    int num_conversions,
    int64_t conversion_priority,
    absl::optional<AttributionReport>& replaced_report) {
  DCHECK_GE(num_conversions, 0);

  const StoredSource& source = report.attribution_info().source;

  // If there's already capacity for the new report, there's nothing to do.
  if (num_conversions < delegate_->GetMaxAttributionsPerSource(
                            source.common_info().source_type())) {
    return MaybeReplaceLowerPriorityEventLevelReportResult::kAddNewReport;
  }

  // Prioritization is scoped within report windows.
  // This is reasonably optimized as is because we only store a ~small number
  // of reports per source_id. Selects the report with lowest priority,
  // and uses the greatest trigger_time to break ties. This favors sending
  // reports for report closer to the source time.
  static constexpr char kMinPrioritySql[] =
      "SELECT priority,report_id "
      "FROM event_level_reports "
      "WHERE source_id = ? AND report_time = ? "
      "ORDER BY priority ASC, trigger_time DESC "
      "LIMIT 1";
  sql::Statement min_priority_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kMinPrioritySql));
  min_priority_statement.BindInt64(0, *source.source_id());
  min_priority_statement.BindTime(1, report.report_time());

  const bool has_matching_report = min_priority_statement.Step();
  if (!min_priority_statement.Succeeded())
    return MaybeReplaceLowerPriorityEventLevelReportResult::kError;

  // Deactivate the source at event-level as a new report will never be
  // generated in the future.
  if (!has_matching_report) {
    static constexpr char kDeactivateSql[] =
        "UPDATE sources SET event_level_active = 0 WHERE source_id = ?";
    sql::Statement deactivate_statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kDeactivateSql));
    deactivate_statement.BindInt64(0, *source.source_id());
    return deactivate_statement.Run()
               ? MaybeReplaceLowerPriorityEventLevelReportResult::
                     kDropNewReportSourceDeactivated
               : MaybeReplaceLowerPriorityEventLevelReportResult::kError;
  }

  int64_t min_priority = min_priority_statement.ColumnInt64(0);
  AttributionReport::EventLevelData::Id conversion_id_with_min_priority(
      min_priority_statement.ColumnInt64(1));

  // If the new report's priority is less than all existing ones, or if its
  // priority is equal to the minimum existing one and it is more recent, drop
  // it. We could explicitly check the trigger time here, but it would only
  // be relevant in the case of an ill-behaved clock, in which case the rest of
  // the attribution functionality would probably also break.
  if (conversion_priority <= min_priority) {
    return MaybeReplaceLowerPriorityEventLevelReportResult::kDropNewReport;
  }

  absl::optional<AttributionReport> replaced =
      GetReport(conversion_id_with_min_priority);
  if (!replaced.has_value()) {
    return MaybeReplaceLowerPriorityEventLevelReportResult::kError;
  }

  // Otherwise, delete the existing report with the lowest priority.
  if (!DeleteReportInternal(conversion_id_with_min_priority)) {
    return MaybeReplaceLowerPriorityEventLevelReportResult::kError;
  }

  replaced_report = std::move(replaced);
  return MaybeReplaceLowerPriorityEventLevelReportResult::kReplaceOldReport;
}

namespace {

bool IsSuccessResult(absl::optional<EventLevelResult> result) {
  return result == EventLevelResult::kSuccess ||
         result == EventLevelResult::kSuccessDroppedLowerPriority;
}

bool IsSuccessResult(absl::optional<AggregatableResult> result) {
  return result == AggregatableResult::kSuccess;
}

CreateReportResult AssembleReportResult(
    base::Time trigger_time,
    EventLevelResult event_level_status,
    AggregatableResult aggregatable_status,
    absl::optional<AttributionReport> new_event_level_report,
    absl::optional<AttributionReport> new_aggregatable_report,
    absl::optional<AttributionReport> replaced_event_level_report) {
  std::vector<AttributionReport> new_reports;

  if (IsSuccessResult(event_level_status)) {
    DCHECK(new_event_level_report.has_value());
    new_reports.push_back(std::move(*new_event_level_report));
  }

  if (IsSuccessResult(aggregatable_status)) {
    DCHECK(new_aggregatable_report.has_value());
    new_reports.push_back(std::move(*new_aggregatable_report));
  }

  return CreateReportResult(
      trigger_time, event_level_status, aggregatable_status,
      std::move(replaced_event_level_report), std::move(new_reports));
}

}  // namespace

CreateReportResult AttributionStorageSql::MaybeCreateAndStoreReport(
    const AttributionTrigger& trigger) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Time trigger_time = base::Time::Now();

  // Declarations for all of the various pieces of information which may be
  // collected and/or returned as a result of computing new reports in order to
  // produce a `CreateReportResult`.
  absl::optional<EventLevelResult> event_level_status;
  absl::optional<AttributionReport> new_event_level_report;

  absl::optional<AggregatableResult> aggregatable_status;
  absl::optional<AttributionReport> new_aggregatable_report;

  absl::optional<AttributionReport> replaced_event_level_report;

  auto assemble_report_result =
      [&](absl::optional<EventLevelResult> new_event_level_status,
          absl::optional<AggregatableResult> new_aggregatable_status) {
        event_level_status = event_level_status.has_value()
                                 ? event_level_status
                                 : new_event_level_status;
        DCHECK(event_level_status.has_value());

        aggregatable_status = aggregatable_status.has_value()
                                  ? aggregatable_status
                                  : new_aggregatable_status;
        DCHECK(aggregatable_status.has_value());

        return AssembleReportResult(trigger_time, *event_level_status,
                                    *aggregatable_status,
                                    std::move(new_event_level_report),
                                    std::move(new_aggregatable_report),
                                    std::move(replaced_event_level_report));
      };

  if (trigger.aggregatable_trigger().trigger_data().empty() &&
      trigger.aggregatable_trigger().values().empty()) {
    aggregatable_status = AggregatableResult::kNotRegistered;
  }

  // We don't bother creating the DB here if it doesn't exist, because it's not
  // possible for there to be a matching source if there's no DB.
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return assemble_report_result(EventLevelResult::kNoMatchingImpressions,
                                  AggregatableResult::kNoMatchingImpressions);

  absl::optional<StoredSource::Id> source_id_to_attribute;
  std::vector<StoredSource::Id> source_ids_to_delete;
  if (!FindMatchingSourceForTrigger(trigger, source_id_to_attribute,
                                    source_ids_to_delete)) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }
  if (!source_id_to_attribute.has_value())
    return assemble_report_result(EventLevelResult::kNoMatchingImpressions,
                                  AggregatableResult::kNoMatchingImpressions);

  absl::optional<StoredSourceData> source_to_attribute =
      ReadSourceToAttribute(db_.get(), *source_id_to_attribute);
  // This is only possible if there is a corrupt DB.
  if (!source_to_attribute.has_value())
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);

  if (source_to_attribute->source.common_info()
          .aggregatable_source()
          .proto()
          .keys()
          .empty()) {
    aggregatable_status = AggregatableResult::kNotRegistered;
  }

  const bool top_level_filters_match = AttributionFiltersMatch(
      source_to_attribute->source.common_info().filter_data(),
      trigger.filters(),
      /*trigger_not_filters=*/AttributionFilterData());

  AttributionInfo attribution_info(std::move(source_to_attribute->source),
                                   trigger_time, trigger.debug_key());

  absl::optional<uint64_t> dedup_key;
  if (EventLevelResult create_event_level_status = MaybeCreateEventLevelReport(
          attribution_info, trigger, top_level_filters_match,
          new_event_level_report, dedup_key);
      create_event_level_status != EventLevelResult::kSuccess) {
    event_level_status = create_event_level_status;
  }

  if (!aggregatable_status.has_value()) {
    if (AggregatableResult create_aggregatable_status =
            MaybeCreateAggregatableAttributionReport(attribution_info, trigger,
                                                     top_level_filters_match,
                                                     new_aggregatable_report);
        create_aggregatable_status != AggregatableResult::kSuccess) {
      aggregatable_status = create_aggregatable_status;
    }
  }

  if (event_level_status.has_value() && aggregatable_status.has_value())
    return assemble_report_result(/*new_event_level_status=*/absl::nullopt,
                                  /*new_aggregaable_status=*/absl::nullopt);

  switch (CapacityForStoringReport(trigger)) {
    case ConversionCapacityStatus::kHasCapacity:
      break;
    case ConversionCapacityStatus::kNoCapacity:
      return assemble_report_result(
          EventLevelResult::kNoCapacityForConversionDestination,
          AggregatableResult::kNoCapacityForConversionDestination);
    case ConversionCapacityStatus::kError:
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
  }

  switch (rate_limit_table_.AttributionAllowedForAttributionLimit(
      db_.get(), attribution_info)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      return assemble_report_result(EventLevelResult::kExcessiveAttributions,
                                    AggregatableResult::kExcessiveAttributions);
    case RateLimitResult::kError:
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
  }

  switch (rate_limit_table_.AttributionAllowedForReportingOriginLimit(
      db_.get(), attribution_info)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      return assemble_report_result(
          EventLevelResult::kExcessiveReportingOrigins,
          AggregatableResult::kExcessiveReportingOrigins);
    case RateLimitResult::kError:
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);

  absl::optional<EventLevelResult> store_event_level_status;
  if (!event_level_status.has_value()) {
    DCHECK(new_event_level_report.has_value());
    store_event_level_status = MaybeStoreEventLevelReport(
        *new_event_level_report, dedup_key,
        source_to_attribute->num_conversions, replaced_event_level_report);
  }

  absl::optional<AggregatableResult> store_aggregatable_status;
  if (!aggregatable_status.has_value()) {
    DCHECK(new_aggregatable_report.has_value());
    store_aggregatable_status = MaybeStoreAggregatableAttributionReport(
        *new_aggregatable_report,
        source_to_attribute->aggregatable_budget_consumed);
  }

  if (store_event_level_status == EventLevelResult::kInternalError ||
      store_aggregatable_status == AggregatableResult::kInternalError) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  // Early exit if done modifying the storage. Dropped reports still need to
  // clean sources.
  if (!IsSuccessResult(store_event_level_status) &&
      !IsSuccessResult(store_aggregatable_status) &&
      store_event_level_status != EventLevelResult::kDroppedForNoise) {
    if (!transaction.Commit())
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);

    return assemble_report_result(store_event_level_status,
                                  store_aggregatable_status);
  }

  // Delete all unattributed sources.
  if (!DeleteSources(source_ids_to_delete))
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);

  // Based on the deletion logic here and the fact that we delete sources
  // with |num_conversions > 0| or |aggregatable_budget_consumed > 0| when
  // there is a new matching source in |StoreSource()|, we should be
  // guaranteed that these sources all have |num_conversions == 0| and
  // |aggregatable_budget_consumed == 0|, and that they never contributed to a
  // rate limit. Therefore, we don't need to call
  // |RateLimitTable::ClearDataForSourceIds()| here.

  // Reports which are dropped do not need to make any further changes.
  if (store_event_level_status == EventLevelResult::kDroppedForNoise &&
      !IsSuccessResult(store_aggregatable_status)) {
    if (!transaction.Commit())
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);

    return assemble_report_result(store_event_level_status,
                                  store_aggregatable_status);
  }

  if (!rate_limit_table_.AddRateLimitForAttribution(db_.get(),
                                                    attribution_info)) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  if (!transaction.Commit())
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);

  return assemble_report_result(store_event_level_status,
                                store_aggregatable_status);
}

bool AttributionStorageSql::FindMatchingSourceForTrigger(
    const AttributionTrigger& trigger,
    absl::optional<StoredSource::Id>& source_id_to_attribute,
    std::vector<StoredSource::Id>& source_ids_to_delete) {
  const url::Origin& destination_origin = trigger.destination_origin();
  DCHECK(!destination_origin.opaque());

  const url::Origin& reporting_origin = trigger.reporting_origin();
  DCHECK(!reporting_origin.opaque());

  // Get all sources that match this <reporting_origin,
  // conversion_destination> pair. Only get sources that are active and not
  // past their expiry time. The sources are fetched in order so that the
  // first one is the one that will be attributed; the others will be deleted.
  static constexpr char kGetMatchingSourcesSql[] =
      "SELECT source_id FROM sources "
      DCHECK_SQL_INDEXED_BY("sources_by_active_destination_site_reporting_origin")
      "WHERE destination_site = ? AND reporting_origin = ? "
      "AND (event_level_active = 1 OR aggregatable_active = 1) "
      "AND expiry_time > ? "
      "ORDER BY priority DESC,source_time DESC";

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetMatchingSourcesSql));
  statement.BindString(0, net::SchemefulSite(destination_origin).Serialize());
  statement.BindString(1,
                       SerializePotentiallyTrustworthyOrigin(reporting_origin));
  statement.BindTime(2, base::Time::Now());

  // If there are no matching sources, return early.
  if (!statement.Step())
    return statement.Succeeded();

  // The first one returned will be attributed; it has the highest priority.
  source_id_to_attribute = StoredSource::Id(statement.ColumnInt64(0));

  // Any others will be deleted.
  while (statement.Step()) {
    StoredSource::Id source_id(statement.ColumnInt64(0));
    source_ids_to_delete.push_back(source_id);
  }
  return statement.Succeeded();
}

EventLevelResult AttributionStorageSql::MaybeCreateEventLevelReport(
    const AttributionInfo& attribution_info,
    const AttributionTrigger& trigger,
    const bool top_level_filters_match,
    absl::optional<AttributionReport>& report,
    absl::optional<uint64_t>& dedup_key) {
  if (attribution_info.source.active_state() ==
      StoredSource::ActiveState::kReachedEventLevelAttributionLimit) {
    return EventLevelResult::kNoMatchingImpressions;
  }

  if (!top_level_filters_match)
    return EventLevelResult::kNoMatchingSourceFilterData;

  const CommonSourceInfo& common_info = attribution_info.source.common_info();

  const AttributionSourceType source_type = common_info.source_type();

  uint64_t trigger_data = 0;
  int64_t priority = 0;

  auto event_trigger = base::ranges::find_if(
      trigger.event_triggers(),
      [&](const AttributionTrigger::EventTriggerData& event_trigger) {
        return AttributionFiltersMatch(common_info.filter_data(),
                                       event_trigger.filters,
                                       event_trigger.not_filters);
      });

  // If there's a match, use its data. Otherwise use default values instead of
  // returning an error so that a report is still sent.
  // TODO(apaseltiner): Consider recording a metric for no match.
  if (event_trigger != trigger.event_triggers().end()) {
    trigger_data = event_trigger->data;
    priority = event_trigger->priority;
    dedup_key = event_trigger->dedup_key;
  }

  switch (ReportAlreadyStored(attribution_info.source.source_id(), dedup_key)) {
    case ReportAlreadyStoredStatus::kNotStored:
      break;
    case ReportAlreadyStoredStatus::kStored:
      return EventLevelResult::kDeduplicated;
    case ReportAlreadyStoredStatus::kError:
      return EventLevelResult::kInternalError;
  }

  const base::Time report_time =
      delegate_->GetEventLevelReportTime(common_info, attribution_info.time);

  // TODO(apaseltiner): When the real values returned by
  // `GetRandomizedResponseRate()` are changed for the first time, we must
  // remove the call to that function here and instead associate each newly
  // stored source and report with the current configuration. One way to do that
  // is to permanently store the configuration history in the binary with each
  // version having a unique ID, and storing that ID in a new column in the
  // sources and event_level_reports DB tables. This code would then look up the
  // values for the particular IDs. Because such an approach would entail
  // complicating the DB schema, we hardcode the values for now and will wait
  // for the first time the values are changed before complicating the codebase.
  const double randomized_response_rate =
      delegate_->GetRandomizedResponseRate(source_type);

  // TODO(apaseltiner): Consider informing the manager if the trigger
  // data was out of range for DevTools issue reporting.
  report = AttributionReport(
      attribution_info, report_time, delegate_->NewReportID(),
      AttributionReport::EventLevelData(
          delegate_->SanitizeTriggerData(trigger_data, source_type), priority,
          randomized_response_rate,
          /*id=*/absl::nullopt));

  return EventLevelResult::kSuccess;
}

EventLevelResult AttributionStorageSql::MaybeStoreEventLevelReport(
    const AttributionReport& report,
    absl::optional<uint64_t> dedup_key,
    int num_conversions,
    absl::optional<AttributionReport>& replaced_report) {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return EventLevelResult::kInternalError;

  const auto* event_level_data =
      absl::get_if<AttributionReport::EventLevelData>(&report.data());
  DCHECK(event_level_data);
  const auto maybe_replace_lower_priority_report_result =
      MaybeReplaceLowerPriorityEventLevelReport(
          report, num_conversions, event_level_data->priority, replaced_report);
  if (maybe_replace_lower_priority_report_result ==
      MaybeReplaceLowerPriorityEventLevelReportResult::kError) {
    return EventLevelResult::kInternalError;
  }

  if (maybe_replace_lower_priority_report_result ==
          MaybeReplaceLowerPriorityEventLevelReportResult::kDropNewReport ||
      maybe_replace_lower_priority_report_result ==
          MaybeReplaceLowerPriorityEventLevelReportResult::
              kDropNewReportSourceDeactivated) {
    if (!transaction.Commit())
      return EventLevelResult::kInternalError;

    return EventLevelResult::kPriorityTooLow;
  }

  const AttributionInfo& attribution_info = report.attribution_info();

  // Reports with `AttributionLogic::kNever` should be included in all
  // attribution operations and matching, but only `kTruthfully` should generate
  // reports that get sent.
  const bool create_report = attribution_info.source.attribution_logic() ==
                             StoredSource::AttributionLogic::kTruthfully;

  if (create_report) {
    if (!StoreEventLevelReport(
            attribution_info.source.source_id(), event_level_data->trigger_data,
            attribution_info.time, report.report_time(),
            event_level_data->priority, report.external_report_id(),
            attribution_info.debug_key)) {
      return EventLevelResult::kInternalError;
    }
  }

  // If a dedup key is present, store it. We do this regardless of whether
  // `create_report` is true to avoid leaking whether the report was actually
  // stored.
  if (dedup_key.has_value()) {
    static constexpr char kInsertDedupKeySql[] =
        "INSERT INTO dedup_keys(source_id,dedup_key)VALUES(?,?)";
    sql::Statement insert_dedup_key_statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kInsertDedupKeySql));
    insert_dedup_key_statement.BindInt64(0,
                                         *attribution_info.source.source_id());
    insert_dedup_key_statement.BindInt64(1, SerializeUint64(*dedup_key));
    if (!insert_dedup_key_statement.Run())
      return EventLevelResult::kInternalError;
  }

  // Only increment the number of conversions associated with the source if
  // we are adding a new one, rather than replacing a dropped one.
  if (maybe_replace_lower_priority_report_result ==
      MaybeReplaceLowerPriorityEventLevelReportResult::kAddNewReport) {
    static constexpr char kUpdateImpressionForConversionSql[] =
        "UPDATE sources SET num_attributions = num_attributions + 1 "
        "WHERE source_id = ?";
    sql::Statement impression_update_statement(db_->GetCachedStatement(
        SQL_FROM_HERE, kUpdateImpressionForConversionSql));

    // Update the attributed source.
    impression_update_statement.BindInt64(0,
                                          *attribution_info.source.source_id());
    if (!impression_update_statement.Run())
      return EventLevelResult::kInternalError;
  }

  if (!transaction.Commit())
    return EventLevelResult::kInternalError;

  if (!create_report)
    return EventLevelResult::kDroppedForNoise;

  return maybe_replace_lower_priority_report_result ==
                 MaybeReplaceLowerPriorityEventLevelReportResult::
                     kReplaceOldReport
             ? EventLevelResult::kSuccessDroppedLowerPriority
             : EventLevelResult::kSuccess;
}

bool AttributionStorageSql::StoreEventLevelReport(
    StoredSource::Id source_id,
    uint64_t trigger_data,
    base::Time trigger_time,
    base::Time report_time,
    int64_t priority,
    const base::GUID& external_report_id,
    absl::optional<uint64_t> trigger_debug_key) {
  DCHECK(external_report_id.is_valid());

  static constexpr char kStoreReportSql[] =
      "INSERT INTO event_level_reports"
      "(source_id,trigger_data,trigger_time,report_time,"
      "priority,failed_send_attempts,external_report_id,debug_key)"
      "VALUES(?,?,?,?,?,0,?,?)";
  sql::Statement store_report_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kStoreReportSql));
  store_report_statement.BindInt64(0, *source_id);
  store_report_statement.BindInt64(1, SerializeUint64(trigger_data));
  store_report_statement.BindTime(2, trigger_time);
  store_report_statement.BindTime(3, report_time);
  store_report_statement.BindInt64(4, priority);
  store_report_statement.BindString(5, external_report_id.AsLowercaseString());
  BindUint64OrNull(store_report_statement, 6, trigger_debug_key);
  return store_report_statement.Run();
}

// Helper to deserialize report rows. See `GetReport()` for the expected
// ordering of columns used for the input to this function.
absl::optional<AttributionReport>
AttributionStorageSql::ReadReportFromStatement(sql::Statement& statement) {
  DCHECK_EQ(statement.ColumnCount(), kSourceColumnCount + 8);

  absl::optional<StoredSourceData> source_data =
      ReadSourceFromStatement(statement);

  int col = kSourceColumnCount;
  uint64_t trigger_data = DeserializeUint64(statement.ColumnInt64(col++));
  base::Time trigger_time = statement.ColumnTime(col++);
  base::Time report_time = statement.ColumnTime(col++);
  AttributionReport::EventLevelData::Id report_id(statement.ColumnInt64(col++));
  int64_t conversion_priority = statement.ColumnInt64(col++);
  int failed_send_attempts = statement.ColumnInt(col++);
  base::GUID external_report_id =
      base::GUID::ParseLowercase(statement.ColumnString(col++));
  absl::optional<uint64_t> trigger_debug_key =
      ColumnUint64OrNull(statement, col++);

  // Ensure data is valid before continuing. This could happen if there is
  // database corruption.
  // TODO(apaseltiner): Should we raze the DB if we've detected corruption?
  if (failed_send_attempts < 0 || !external_report_id.is_valid() ||
      !source_data.has_value()) {
    return absl::nullopt;
  }

  double randomized_response_rate = delegate_->GetRandomizedResponseRate(
      source_data->source.common_info().source_type());

  AttributionReport report(
      AttributionInfo(std::move(source_data->source), trigger_time,
                      trigger_debug_key),
      report_time, std::move(external_report_id),
      AttributionReport::EventLevelData(trigger_data, conversion_priority,
                                        randomized_response_rate, report_id));
  report.set_failed_send_attempts(failed_send_attempts);
  return report;
}

std::vector<AttributionReport> AttributionStorageSql::GetAttributionReports(
    base::Time max_report_time,
    int limit,
    AttributionReport::ReportTypes report_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!report_types.Empty());

  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return {};

  std::vector<AttributionReport> reports;

  for (AttributionReport::ReportType report_type : report_types) {
    switch (report_type) {
      case AttributionReport::ReportType::kEventLevel: {
        std::vector<AttributionReport> event_level_reports =
            GetEventLevelReportsInternal(max_report_time, limit);
        reports.insert(reports.end(),
                       std::make_move_iterator(event_level_reports.begin()),
                       std::make_move_iterator(event_level_reports.end()));
        break;
      }
      case AttributionReport::ReportType::kAggregatableAttribution: {
        std::vector<AttributionReport> aggregatable_reports =
            GetAggregatableAttributionReportsInternal(max_report_time, limit);
        reports.insert(reports.end(),
                       std::make_move_iterator(aggregatable_reports.begin()),
                       std::make_move_iterator(aggregatable_reports.end()));
        break;
      }
    }
  }

  if (limit >= 0 && reports.size() > static_cast<size_t>(limit)) {
    base::ranges::partial_sort(reports, reports.begin() + limit, /*comp=*/{},
                               &AttributionReport::report_time);
    reports.erase(reports.begin() + limit);
  }

  delegate_->ShuffleReports(reports);
  return reports;
}

std::vector<AttributionReport>
AttributionStorageSql::GetEventLevelReportsInternal(base::Time max_report_time,
                                                    int limit) {
  // Get at most |limit| entries in the event_level_reports table with a
  // |report_time| no greater than |max_report_time| and their matching
  // information from the impression table. Negatives are treated as no limit
  // (https://sqlite.org/lang_select.html#limitoffset).
  static constexpr char kGetReportsSql[] =
      ATTRIBUTION_SELECT_EVENT_LEVEL_REPORT_AND_SOURCE_COLUMNS_SQL
      "WHERE C.report_time <= ? LIMIT ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetReportsSql));
  statement.BindTime(0, max_report_time);
  statement.BindInt(1, limit);

  std::vector<AttributionReport> reports;
  while (statement.Step()) {
    absl::optional<AttributionReport> report =
        ReadReportFromStatement(statement);
    if (report.has_value())
      reports.push_back(std::move(*report));
  }

  if (!statement.Succeeded())
    return {};

  return reports;
}

absl::optional<base::Time> AttributionStorageSql::GetNextReportTime(
    base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return absl::nullopt;

  absl::optional<base::Time> next_event_level_report_time =
      GetNextEventLevelReportTime(time);
  absl::optional<base::Time> next_aggregatable_report_time =
      GetNextAggregatableAttributionReportTime(time);

  return GetMinTime(next_event_level_report_time,
                    next_aggregatable_report_time);
}

absl::optional<base::Time> AttributionStorageSql::GetNextReportTime(
    sql::StatementID id,
    const char* sql,
    base::Time time) {
  sql::Statement statement(db_->GetCachedStatement(id, sql));
  statement.BindTime(0, time);

  if (statement.Step() &&
      statement.GetColumnType(0) != sql::ColumnType::kNull) {
    return statement.ColumnTime(0);
  }

  return absl::nullopt;
}

absl::optional<base::Time> AttributionStorageSql::GetNextEventLevelReportTime(
    base::Time time) {
  static constexpr char kNextReportTimeSql[] =
      ATTRIBUTION_NEXT_REPORT_TIME_SQL(ATTRIBUTION_CONVERSIONS_TABLE);
  return GetNextReportTime(SQL_FROM_HERE, kNextReportTimeSql, time);
}

std::vector<AttributionReport> AttributionStorageSql::GetReports(
    const std::vector<AttributionReport::Id>& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return {};

  std::vector<AttributionReport> reports;
  for (AttributionReport::Id id : ids) {
    absl::optional<AttributionReport> report = absl::visit(
        [&](auto id) {
          DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
          return GetReport(id);
        },
        id);

    if (report.has_value())
      reports.push_back(std::move(*report));
  }
  return reports;
}

absl::optional<AttributionReport> AttributionStorageSql::GetReport(
    AttributionReport::EventLevelData::Id conversion_id) {
  static constexpr char kGetReportSql[] =
      ATTRIBUTION_SELECT_EVENT_LEVEL_REPORT_AND_SOURCE_COLUMNS_SQL
      "WHERE C.report_id = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetReportSql));
  statement.BindInt64(0, *conversion_id);

  if (!statement.Step())
    return absl::nullopt;

  return ReadReportFromStatement(statement);
}

bool AttributionStorageSql::DeleteExpiredSources() {
  const int kMaxDeletesPerBatch = 100;

  auto delete_sources_from_paged_select =
      [this](sql::Statement& statement)
          VALID_CONTEXT_REQUIRED(sequence_checker_) -> bool {
    DCHECK_EQ(statement.ColumnCount(), 1);

    while (true) {
      std::vector<StoredSource::Id> source_ids;
      while (statement.Step()) {
        StoredSource::Id source_id(statement.ColumnInt64(0));
        source_ids.push_back(source_id);
      }
      if (!statement.Succeeded())
        return false;
      if (source_ids.empty())
        return true;
      if (!DeleteSources(source_ids))
        return false;
      // Deliberately retain the existing bound vars so that the limit, etc are
      // the same.
      statement.Reset(/*clear_bound_vars=*/false);
    }
  };

  // Delete all sources that have no associated reports and are past
  // their expiry time. Optimized by |kImpressionExpiryIndexSql|.
  static constexpr char kSelectExpiredSourcesSql[] =
      "SELECT source_id FROM sources "
      DCHECK_SQL_INDEXED_BY("sources_by_expiry_time")
      "WHERE expiry_time <= ? AND "
      "source_id NOT IN("
      "SELECT source_id FROM event_level_reports"
      DCHECK_SQL_INDEXED_BY("event_level_reports_by_source_id")
      ")AND source_id NOT IN("
      "SELECT source_id FROM aggregatable_report_metadata"
      DCHECK_SQL_INDEXED_BY("aggregate_source_id_idx")
      ")LIMIT ?";
  sql::Statement select_expired_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectExpiredSourcesSql));
  select_expired_statement.BindTime(0, base::Time::Now());
  select_expired_statement.BindInt(1, kMaxDeletesPerBatch);
  if (!delete_sources_from_paged_select(select_expired_statement))
    return false;

  // Delete all sources that have no associated reports and are
  // inactive. This is done in a separate statement from
  // |kSelectExpiredSourcesSql| so that each query is optimized by an index.
  // Optimized by |kConversionDestinationIndexSql|.
  static constexpr char kSelectInactiveSourcesSql[] =
      "SELECT source_id FROM sources "
      DCHECK_SQL_INDEXED_BY("sources_by_active_destination_site_reporting_origin")
      "WHERE event_level_active = 0 AND aggregatable_active = 0 AND "
      "source_id NOT IN("
      "SELECT source_id FROM event_level_reports"
      DCHECK_SQL_INDEXED_BY("event_level_reports_by_source_id")
      ")AND source_id NOT IN("
      "SELECT source_id FROM aggregatable_report_metadata"
      DCHECK_SQL_INDEXED_BY("aggregate_source_id_idx")
      ")LIMIT ?";
  sql::Statement select_inactive_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectInactiveSourcesSql));
  select_inactive_statement.BindInt(0, kMaxDeletesPerBatch);
  return delete_sources_from_paged_select(select_inactive_statement);
}

bool AttributionStorageSql::DeleteReport(AttributionReport::Id report_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return true;

  return absl::visit(
      [&](auto id) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
        return DeleteReportInternal(id);
      },
      report_id);
}

bool AttributionStorageSql::DeleteReportInternal(
    AttributionReport::EventLevelData::Id report_id) {
  static constexpr char kDeleteReportSql[] =
      "DELETE FROM event_level_reports WHERE report_id = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteReportSql));
  statement.BindInt64(0, *report_id);
  return statement.Run();
}

bool AttributionStorageSql::UpdateReportForSendFailure(
    AttributionReport::Id report_id,
    base::Time new_report_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return false;

  struct Visitor {
    raw_ptr<AttributionStorageSql> storage;
    base::Time new_report_time;

    bool operator()(AttributionReport::EventLevelData::Id id) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(storage->sequence_checker_);
      static constexpr char kUpdateFailedReportSql[] =
          ATTRIBUTION_UPDATE_FAILED_REPORT_SQL(ATTRIBUTION_CONVERSIONS_TABLE,
                                               "report_id");
      return storage->UpdateReportForSendFailure(
          SQL_FROM_HERE, kUpdateFailedReportSql, *id, new_report_time);
    }

    bool operator()(AttributionReport::AggregatableAttributionData::Id id) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(storage->sequence_checker_);
      static constexpr char kUpdateFailedReportSql[] =
          ATTRIBUTION_UPDATE_FAILED_REPORT_SQL(
              ATTRIBUTION_AGGREGATABLE_REPORT_METADATA_TABLE, "aggregation_id");
      return storage->UpdateReportForSendFailure(
          SQL_FROM_HERE, kUpdateFailedReportSql, *id, new_report_time);
    }
  };

  return absl::visit(
      Visitor{.storage = this, .new_report_time = new_report_time}, report_id);
}

bool AttributionStorageSql::UpdateReportForSendFailure(
    sql::StatementID id,
    const char* sql,
    int64_t report_id,
    base::Time new_report_time) {
  sql::Statement statement(db_->GetCachedStatement(id, sql));
  statement.BindTime(0, new_report_time);
  statement.BindInt64(1, report_id);
  return statement.Run() && db_->GetLastChangeCount() == 1;
}

absl::optional<base::Time> AttributionStorageSql::AdjustOfflineReportTimes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto delay = delegate_->GetOfflineReportDelayConfig();
  if (!delay.has_value())
    return absl::nullopt;

  DCHECK_GE(delay->min, base::TimeDelta());
  DCHECK_GE(delay->max, base::TimeDelta());
  DCHECK_LE(delay->min, delay->max);

  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return absl::nullopt;

  base::Time now = base::Time::Now();

  absl::optional<base::Time> next_event_level_report_time =
      AdjustOfflineEventLevelReportTimes(delay->min, delay->max, now);
  absl::optional<base::Time> next_aggregatable_report_time =
      AdjustOfflineAggregatableAttributionReportTimes(delay->min, delay->max,
                                                      now);
  return GetMinTime(next_event_level_report_time,
                    next_aggregatable_report_time);
}

bool AttributionStorageSql::AdjustOfflineReportTimes(sql::StatementID id,
                                                     const char* sql,
                                                     base::TimeDelta min_delay,
                                                     base::TimeDelta max_delay,
                                                     base::Time now) {
  sql::Statement statement(db_->GetCachedStatement(id, sql));
  statement.BindTime(0, now + min_delay);
  statement.BindInt64(1, 1 + (max_delay - min_delay).InMicroseconds());
  statement.BindTime(2, now);
  return statement.Run();
}

absl::optional<base::Time>
AttributionStorageSql::AdjustOfflineEventLevelReportTimes(
    base::TimeDelta min_delay,
    base::TimeDelta max_delay,
    base::Time now) {
  static constexpr char kSetReportTimeSql[] =
      ATTRIBUTION_SET_REPORT_TIME_SQL(ATTRIBUTION_CONVERSIONS_TABLE);
  if (!AdjustOfflineReportTimes(SQL_FROM_HERE, kSetReportTimeSql, min_delay,
                                max_delay, now)) {
    return absl::nullopt;
  }

  return GetNextEventLevelReportTime(base::Time::Min());
}

void AttributionStorageSql::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    base::RepeatingCallback<bool(const url::Origin&)> filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return;

  SCOPED_UMA_HISTOGRAM_TIMER("Conversions.ClearDataTime");
  if (filter.is_null() && (delete_begin.is_null() || delete_begin.is_min()) &&
      delete_end.is_max()) {
    ClearAllDataAllTime();
    return;
  }

  // Measure the time it takes to perform a clear with a filter separately from
  // the above histogram.
  SCOPED_UMA_HISTOGRAM_TIMER("Conversions.Storage.ClearDataWithFilterDuration");

  // TODO(csharrison, johnidel): This query can be split up and optimized by
  // adding indexes on the time and trigger_time columns.
  // See this comment for more information:
  // crrev.com/c/2150071/4/content/browser/conversions/conversion_storage_sql.cc#342
  //
  // TODO(crbug.com/1290377): Look into optimizing origin filter callback.
  static constexpr char kScanCandidateData[] =
      "SELECT I.source_origin,I.destination_origin,I.reporting_origin,"
      "I.source_id,C.report_id "
      "FROM sources I LEFT JOIN event_level_reports C ON "
      "C.source_id = I.source_id WHERE"
      "(I.source_time BETWEEN ?1 AND ?2)OR"
      "(C.trigger_time BETWEEN ?1 AND ?2)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kScanCandidateData));
  statement.BindTime(0, delete_begin);
  statement.BindTime(1, delete_end);

  // TODO(apaseltiner): Consider wrapping `filter` such that it deletes
  // opaque/untrustworthy origins.

  std::vector<StoredSource::Id> source_ids_to_delete;
  int num_reports_deleted = 0;
  while (statement.Step()) {
    if (filter.is_null() ||
        filter.Run(DeserializeOrigin(statement.ColumnString(0))) ||
        filter.Run(DeserializeOrigin(statement.ColumnString(1))) ||
        filter.Run(DeserializeOrigin(statement.ColumnString(2)))) {
      source_ids_to_delete.emplace_back(statement.ColumnInt64(3));
      if (statement.GetColumnType(4) != sql::ColumnType::kNull) {
        if (!DeleteReportInternal(AttributionReport::EventLevelData::Id(
                statement.ColumnInt64(4)))) {
          return;
        }

        ++num_reports_deleted;
      }
    }
  }

  // TODO(csharrison, johnidel): Should we consider poisoning the DB if some of
  // the delete operations fail?
  if (!statement.Succeeded())
    return;

  // Delete the data in a transaction to avoid cases where the source part
  // of a report is deleted without deleting the associated report, or
  // vice versa.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  if (!ClearAggregatableAttributionsForOriginsInRange(
          delete_begin, delete_end, filter, source_ids_to_delete)) {
    return;
  }

  // Since multiple reports can be associated with a single source,
  // deduplicate source IDs using a set to avoid redundant DB operations
  // below.
  source_ids_to_delete =
      base::flat_set<StoredSource::Id>(std::move(source_ids_to_delete))
          .extract();

  if (!DeleteSources(source_ids_to_delete))
    return;

  // Careful! At this point we can still have some vestigial entries in the DB.
  // For example, if a source has two reports, and one report is
  // deleted, the above logic will delete the source as well, leaving the
  // second report in limbo (it was not in the deletion time range).
  // Delete all unattributed reports here to ensure everything is cleaned
  // up.
  static constexpr char kDeleteVestigialConversionSql[] =
      "DELETE FROM event_level_reports WHERE source_id = ?";
  sql::Statement delete_vestigial_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteVestigialConversionSql));
  for (StoredSource::Id source_id : source_ids_to_delete) {
    delete_vestigial_statement.Reset(/*clear_bound_vars=*/true);
    delete_vestigial_statement.BindInt64(0, *source_id);
    if (!delete_vestigial_statement.Run())
      return;

    num_reports_deleted += db_->GetLastChangeCount();
  }

  // Careful! At this point we can still have some vestigial entries in the DB.
  // See comments above for event-level reports.
  if (!ClearAggregatableAttributionsForSourceIds(source_ids_to_delete))
    return;

  if (!rate_limit_table_.ClearDataForSourceIds(db_.get(),
                                               source_ids_to_delete)) {
    return;
  }

  if (!rate_limit_table_.ClearDataForOriginsInRange(db_.get(), delete_begin,
                                                    delete_end, filter)) {
    return;
  }

  if (!transaction.Commit())
    return;

  RecordSourcesDeleted(static_cast<int>(source_ids_to_delete.size()));
  RecordReportsDeleted(num_reports_deleted);
}

void AttributionStorageSql::ClearAllDataAllTime() {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  static constexpr char kDeleteAllReportsSql[] =
      "DELETE FROM event_level_reports";
  sql::Statement delete_all_reports_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllReportsSql));
  if (!delete_all_reports_statement.Run())
    return;
  int num_reports_deleted = db_->GetLastChangeCount();

  static constexpr char kDeleteAllSourcesSql[] = "DELETE FROM sources";
  sql::Statement delete_all_sources_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllSourcesSql));
  if (!delete_all_sources_statement.Run())
    return;
  int num_sources_deleted = db_->GetLastChangeCount();

  static constexpr char kDeleteAllDedupKeysSql[] = "DELETE FROM dedup_keys";
  sql::Statement delete_all_dedup_keys_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllDedupKeysSql));
  if (!delete_all_dedup_keys_statement.Run())
    return;

  static constexpr char kDeleteAllAggregationsSql[] =
      "DELETE FROM aggregatable_report_metadata";
  sql::Statement delete_all_aggregations_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllAggregationsSql));
  if (!delete_all_aggregations_statement.Run())
    return;

  static constexpr char kDeleteAllContributionsSql[] =
      "DELETE FROM aggregatable_contributions";
  sql::Statement delete_all_contributions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllContributionsSql));
  if (!delete_all_contributions_statement.Run())
    return;

  if (!rate_limit_table_.ClearAllDataAllTime(db_.get()))
    return;

  if (!transaction.Commit())
    return;

  RecordSourcesDeleted(num_sources_deleted);
  RecordReportsDeleted(num_reports_deleted);
}

bool AttributionStorageSql::HasCapacityForStoringSource(
    const std::string& serialized_origin) {
  static constexpr char kCountSourcesSql[] =
      // clang-format off
      "SELECT COUNT(source_origin)FROM sources "
      DCHECK_SQL_INDEXED_BY("sources_by_origin")
      "WHERE source_origin = ?";  // clang-format on

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kCountSourcesSql));
  statement.BindString(0, serialized_origin);
  if (!statement.Step())
    return false;
  int64_t count = statement.ColumnInt64(0);
  return count < delegate_->GetMaxSourcesPerOrigin();
}

AttributionStorageSql::ReportAlreadyStoredStatus
AttributionStorageSql::ReportAlreadyStored(StoredSource::Id source_id,
                                           absl::optional<uint64_t> dedup_key) {
  if (!dedup_key.has_value())
    return ReportAlreadyStoredStatus::kNotStored;

  static constexpr char kCountReportsSql[] =
      "SELECT COUNT(*)FROM dedup_keys "
      "WHERE source_id = ? AND dedup_key = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kCountReportsSql));
  statement.BindInt64(0, *source_id);
  statement.BindInt64(1, SerializeUint64(*dedup_key));

  // If there's an error, return true so `MaybeCreateAndStoreReport()`
  // returns early.
  if (!statement.Step())
    return ReportAlreadyStoredStatus::kError;

  int64_t count = statement.ColumnInt64(0);
  return count > 0 ? ReportAlreadyStoredStatus::kStored
                   : ReportAlreadyStoredStatus::kNotStored;
}

AttributionStorageSql::ConversionCapacityStatus
AttributionStorageSql::CapacityForStoringReport(
    const AttributionTrigger& trigger) {
  // This query should be reasonably optimized via
  // `kConversionDestinationIndexSql`. The conversion origin is the second
  // column in a multi-column index where the first column is just a boolean.
  // Therefore the second column in the index should be very well-sorted.
  //
  // Note: to take advantage of this, we need to hint to the query planner that
  // |event_level_active| and |aggregatable_active| are booleans, so include
  // them in the conditional.
  static constexpr char kCountReportsSql[] =
      "SELECT COUNT(*)FROM event_level_reports C "
      "JOIN sources I "
      DCHECK_SQL_INDEXED_BY("sources_by_active_destination_site_reporting_origin")
      "ON I.source_id = C.source_id "
      "WHERE I.destination_site = ? AND "
      "(event_level_active BETWEEN 0 AND 1) AND "
      "(aggregatable_active BETWEEN 0 AND 1)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kCountReportsSql));
  statement.BindString(
      0, net::SchemefulSite(trigger.destination_origin()).Serialize());
  if (!statement.Step())
    return ConversionCapacityStatus::kError;
  int64_t count = statement.ColumnInt64(0);
  return count < delegate_->GetMaxAttributionsPerOrigin()
             ? ConversionCapacityStatus::kHasCapacity
             : ConversionCapacityStatus::kNoCapacity;
}

std::vector<StoredSource> AttributionStorageSql::GetActiveSources(int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return {};

  // Negatives are treated as no limit
  // (https://sqlite.org/lang_select.html#limitoffset).
  static constexpr char kGetActiveSourcesSql[] =
      // clang-format off
      "SELECT " ATTRIBUTION_SOURCE_COLUMNS_SQL("")
      " FROM sources "
      "WHERE (event_level_active = 1 OR aggregatable_active = 1) AND "
      "expiry_time > ? LIMIT ?";  // clang-format on

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetActiveSourcesSql));
  statement.BindTime(0, base::Time::Now());
  statement.BindInt(1, limit);

  std::vector<StoredSource> sources;
  while (statement.Step()) {
    absl::optional<StoredSourceData> source_data =
        ReadSourceFromStatement(statement);
    if (source_data.has_value())
      sources.push_back(std::move(source_data->source));
  }
  if (!statement.Succeeded())
    return {};

  for (auto& source : sources) {
    absl::optional<std::vector<uint64_t>> dedup_keys =
        ReadDedupKeys(source.source_id());
    if (!dedup_keys.has_value())
      return {};
    source.SetDedupKeys(std::move(*dedup_keys));
  }

  return sources;
}

absl::optional<std::vector<uint64_t>> AttributionStorageSql::ReadDedupKeys(
    StoredSource::Id source_id) {
  static constexpr char kDedupKeySql[] =
      "SELECT dedup_key FROM dedup_keys WHERE source_id = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDedupKeySql));
  statement.BindInt64(0, *source_id);

  std::vector<uint64_t> dedup_keys;
  while (statement.Step()) {
    dedup_keys.push_back(DeserializeUint64(statement.ColumnInt64(0)));
  }
  if (!statement.Succeeded())
    return absl ::nullopt;

  return dedup_keys;
}

void AttributionStorageSql::HandleInitializationFailure(
    const InitStatus status) {
  RecordInitializationStatus(status);
  db_.reset();
  db_init_status_ = DbStatus::kClosed;
}

bool AttributionStorageSql::LazyInit(DbCreationPolicy creation_policy) {
  if (!db_init_status_) {
    if (g_run_in_memory_) {
      db_init_status_ = DbStatus::kDeferringCreation;
    } else {
      db_init_status_ = base::PathExists(path_to_database_)
                            ? DbStatus::kDeferringOpen
                            : DbStatus::kDeferringCreation;
    }
  }

  switch (*db_init_status_) {
    // If the database file has not been created, we defer creation until
    // storage needs to be used for an operation which needs to operate even on
    // an empty database.
    case DbStatus::kDeferringCreation:
      if (creation_policy == DbCreationPolicy::kIgnoreIfAbsent)
        return false;
      break;
    case DbStatus::kDeferringOpen:
      break;
    case DbStatus::kClosed:
      return false;
    case DbStatus::kOpen:
      return true;
  }

  db_ = std::make_unique<sql::Database>(sql::DatabaseOptions{
      .exclusive_locking = true, .page_size = 4096, .cache_size = 32});
  db_->set_histogram_tag("Conversions");

  // Supply this callback with a weak_ptr to avoid calling the error callback
  // after |this| has been deleted.
  db_->set_error_callback(
      base::BindRepeating(&AttributionStorageSql::DatabaseErrorCallback,
                          weak_factory_.GetWeakPtr()));

  if (path_to_database_.value() == kInMemoryPath) {
    if (!db_->OpenInMemory()) {
      HandleInitializationFailure(InitStatus::kFailedToOpenDbInMemory);
      return false;
    }
  } else {
    const base::FilePath& dir = path_to_database_.DirName();
    const bool dir_exists_or_was_created =
        base::DirectoryExists(dir) || base::CreateDirectory(dir);
    if (dir_exists_or_was_created == false) {
      DLOG(ERROR) << "Failed to create directory for Conversion database";
      HandleInitializationFailure(InitStatus::kFailedToCreateDir);
      return false;
    }
    if (db_->Open(path_to_database_) == false) {
      HandleInitializationFailure(InitStatus::kFailedToOpenDbFile);
      return false;
    }
  }

  if (!InitializeSchema(db_init_status_ == DbStatus::kDeferringCreation)) {
    HandleInitializationFailure(InitStatus::kFailedToInitializeSchema);
    return false;
  }

  db_init_status_ = DbStatus::kOpen;
  RecordInitializationStatus(InitStatus::kSuccess);
  return true;
}

bool AttributionStorageSql::InitializeSchema(bool db_empty) {
  if (db_empty)
    return CreateSchema();

  // Create the meta table if it doesn't already exist. The only version for
  // which this is the case is version 1.
  if (!meta_table_.Init(db_.get(), /*version=*/1, kCompatibleVersionNumber))
    return false;

  int version = meta_table_.GetVersionNumber();
  if (version == kCurrentVersionNumber)
    return true;

  // Recreate the DB if the version is deprecated or too new. In the latter
  // case, the DB will never work until Chrome is re-upgraded. Assume the user
  // will continue using this Chrome version and raze the DB to get attribution
  // reporting working.
  if (version <= kDeprecatedVersionNumber ||
      meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    // Note that this also razes the meta table, so it will need to be
    // initialized again.
    db_->Raze();
    meta_table_.Reset();
    return CreateSchema();
  }

  return UpgradeAttributionStorageSqlSchema(db_.get(), &meta_table_);
}

bool AttributionStorageSql::CreateSchema() {
  base::ThreadTicks start_timestamp = base::ThreadTicks::Now();

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  // TODO(johnidel, csharrison): Many sources will share a target origin and
  // a reporting origin, so it makes sense to make a "shared string" table for
  // these to save disk / memory. However, this complicates the schema a lot, so
  // probably best to only do it if there's performance problems here.
  //
  // Origins usually aren't _that_ big compared to a 64 bit integer(8 bytes).
  //
  // All of the columns in this table are designed to be "const" except for
  // |num_attributions|, |aggregatable_budget_consumed|, |event_level_active|
  // and |aggregatable_active| which are updated when a new trigger is
  // received. |num_attributions| is the number of times an event-level report
  // has been created for a given source. |aggregatable_budget_consumed| is the
  // aggregatable budget that has been consumed for a given source. |delegate_|
  // can choose to enforce a maximum limit on them. |event_level_active| and
  // |aggregatable_active| indicate whether a source is able to create new
  // associated event-level and aggregatable reports. |event_level_active| and
  // |aggregatable_active| can be unset on a number of conditions:
  //   - A source converted too many times.
  //   - A new source was stored after a source converted, making it
  //     ineligible for new sources due to the attribution model documented
  //     in `StoreSource()`.
  //   - A source has expired but still has unsent reports in the
  //     event_level_reports table meaning it cannot be deleted yet.
  // |source_type| is the type of the source of the source, currently always
  // |kNavigation|.
  // |attribution_logic| corresponds to the
  // |StoredSource::AttributionLogic| enum.
  // |source_site| is used to optimize the lookup of sources;
  // |CommonSourceInfo::ImpressionSite| is always derived from the origin.
  // |filter_data| is a serialized `AttributionFilterData` used for source
  // matching.
  //
  // |source_id| uses AUTOINCREMENT to ensure that IDs aren't reused over
  // the lifetime of the DB.
  static constexpr char kImpressionTableSql[] =
      "CREATE TABLE IF NOT EXISTS sources"
      "(source_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "source_event_id INTEGER NOT NULL,"
      "source_origin TEXT NOT NULL,"
      "destination_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "source_time INTEGER NOT NULL,"
      "expiry_time INTEGER NOT NULL,"
      "num_attributions INTEGER NOT NULL,"
      "event_level_active INTEGER NOT NULL,"
      "aggregatable_active INTEGER NOT NULL,"
      "destination_site TEXT NOT NULL,"
      "source_type INTEGER NOT NULL,"
      "attribution_logic INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "source_site TEXT NOT NULL,"
      "debug_key INTEGER,"
      "aggregatable_budget_consumed INTEGER NOT NULL,"
      "aggregatable_source BLOB NOT NULL,"
      "filter_data BLOB NOT NULL)";
  if (!db_->Execute(kImpressionTableSql))
    return false;

  // Optimizes source lookup by conversion destination/reporting origin
  // during calls to `MaybeCreateAndStoreReport()`,
  // `StoreSource()`, `DeleteExpiredSources()`. Sources and
  // triggers are considered matching if they share this pair. These calls
  // need to distinguish between active and inactive reports, so include
  // |event_level_active| and |aggregatable_active| in the index.
  static constexpr char kConversionDestinationIndexSql[] =
      "CREATE INDEX IF NOT EXISTS "
      "sources_by_active_destination_site_reporting_origin "
      "ON sources(event_level_active,aggregatable_active,"
      "destination_site,reporting_origin)";
  if (!db_->Execute(kConversionDestinationIndexSql))
    return false;

  // Optimizes calls to `DeleteExpiredSources()` and
  // `MaybeCreateAndStoreReport()` by indexing sources by expiry
  // time. Both calls require only returning sources that expire after a
  // given time.
  static constexpr char kImpressionExpiryIndexSql[] =
      "CREATE INDEX IF NOT EXISTS sources_by_expiry_time "
      "ON sources(expiry_time)";
  if (!db_->Execute(kImpressionExpiryIndexSql))
    return false;

  // Optimizes counting sources by source origin.
  static constexpr char kImpressionOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS sources_by_origin "
      "ON sources(source_origin)";
  if (!db_->Execute(kImpressionOriginIndexSql))
    return false;

  // Optimizes `HasCapacityForUniqueDestinationLimitForPendingSource()`, which
  // only needs to examine active, unconverted sources.
  static constexpr char kImpressionSiteReportingOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS "
      "active_unattributed_sources_by_site_reporting_origin "
      "ON sources(source_site,reporting_origin)"
      "WHERE event_level_active=1 AND num_attributions=0 AND "
      "aggregatable_active=1 AND aggregatable_budget_consumed=0";
  if (!db_->Execute(kImpressionSiteReportingOriginIndexSql))
    return false;

  // All columns in this table are const except |report_time| and
  // |failed_send_attempts|,
  // which are updated when a report fails to send, as part of retries.
  // |source_id| is the primary key of a row in the [sources] table,
  // [sources.source_id]. |trigger_time| is the time at which the
  // trigger was registered, and should be used for clearing site data.
  // |report_time| is the time a <report, source> pair should be
  // reported, and is specified by |delegate_|.
  //
  // |id| uses AUTOINCREMENT to ensure that IDs aren't reused over
  // the lifetime of the DB.
  static constexpr char kConversionTableSql[] =
      "CREATE TABLE IF NOT EXISTS event_level_reports"
      "(report_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "source_id INTEGER NOT NULL,"
      "trigger_data INTEGER NOT NULL,"
      "trigger_time INTEGER NOT NULL,"
      "report_time INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "failed_send_attempts INTEGER NOT NULL,"
      "external_report_id TEXT NOT NULL,"
      "debug_key INTEGER)";
  if (!db_->Execute(kConversionTableSql))
    return false;

  // Optimize sorting reports by report time for calls to
  // `GetAttributionReports()`. The reports with the earliest report times are
  // periodically fetched from storage to be sent.
  static constexpr char kConversionReportTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS event_level_reports_by_report_time "
      "ON event_level_reports(report_time)";
  if (!db_->Execute(kConversionReportTimeIndexSql))
    return false;

  // Want to optimize report look up by source id. This allows us to
  // quickly know if an expired source can be deleted safely if it has no
  // corresponding pending reports during calls to
  // `DeleteExpiredSources()`.
  static constexpr char kConversionImpressionIdIndexSql[] =
      "CREATE INDEX IF NOT EXISTS event_level_reports_by_source_id "
      "ON event_level_reports(source_id)";
  if (!db_->Execute(kConversionImpressionIdIndexSql))
    return false;

  if (!rate_limit_table_.CreateTable(db_.get()))
    return false;

  static constexpr char kDedupKeyTableSql[] =
      "CREATE TABLE IF NOT EXISTS dedup_keys"
      "(source_id INTEGER NOT NULL,"
      "dedup_key INTEGER NOT NULL,"
      "PRIMARY KEY(source_id,dedup_key))WITHOUT ROWID";
  if (!db_->Execute(kDedupKeyTableSql))
    return false;

  // ============================
  // AGGREGATE ATTRIBUTION SCHEMA
  // ============================

  // An attribution might make multiple histogram contributions. Therefore
  // multiple rows in |aggregatable_contributions| table might correspond to the
  // same row in |aggregatable_report_metadata| table.

  // All columns in this table are const except `report_time` and
  // `failed_send_attempts`, which are updated when a report fails to send, as
  // part of retries.
  // `source_id` is the primary key of a row in the [sources] table,
  // [sources.source_id].
  // `trigger_time` is the time at which the trigger was registered, and
  // should be used for clearing site data.
  // `external_report_id` is used for deduplicating reports received by the
  // reporting origin.
  // `report_time` is the time the aggregatable report should be reported.
  // `initial_report_time` is the report time initially scheduled by the
  // browser.
  static constexpr char kAggregatableReportMetadataTableSql[] =
      "CREATE TABLE IF NOT EXISTS aggregatable_report_metadata("
      "aggregation_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "source_id INTEGER NOT NULL,"
      "trigger_time INTEGER NOT NULL,"
      "debug_key INTEGER,"
      "external_report_id TEXT NOT NULL,"
      "report_time INTEGER NOT NULL,"
      "failed_send_attempts INTEGER NOT NULL,"
      "initial_report_time INTEGER NOT NULL)";
  if (!db_->Execute(kAggregatableReportMetadataTableSql))
    return false;

  // Optimizes aggregatable report look up by source id during calls to
  // `DeleteExpiredSources()`, `ClearAggregatableAttributionsForSourceIds()`.
  static constexpr char kAggregateSourceIdIndexSql[] =
      "CREATE INDEX IF NOT EXISTS aggregate_source_id_idx "
      "ON aggregatable_report_metadata(source_id)";
  if (!db_->Execute(kAggregateSourceIdIndexSql))
    return false;

  // Optimizes aggregatable report look up by trigger time for clearing site
  // data during calls to
  // `ClearAggregatableAttributionsForOriginsInRange()`.
  static constexpr char kAggregateTriggerTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS aggregate_trigger_time_idx "
      "ON aggregatable_report_metadata(trigger_time)";
  if (!db_->Execute(kAggregateTriggerTimeIndexSql))
    return false;

  // Optimizes aggregatable report look up by report time to get reports in a
  // time range during calls to
  // `GetAggregatableAttributionReportsInternal()`.
  static constexpr char kAggregateReportTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS aggregate_report_time_idx "
      "ON aggregatable_report_metadata(report_time)";
  if (!db_->Execute(kAggregateReportTimeIndexSql))
    return false;

  // All columns in this table are const.
  // `aggregation_id` is the primary key of a row in the
  // [aggregatable_report_metadata] table.
  // `key_high_bits` and `key_low_bits` represent the histogram bucket key that
  // is a 128-bit unsigned integer.
  // `value` is the histogram value.
  static constexpr char kAggregatableContributionsTableSql[] =
      "CREATE TABLE IF NOT EXISTS aggregatable_contributions("
      "contribution_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "aggregation_id INTEGER NOT NULL,"
      "key_high_bits INTEGER NOT NULL,"
      "key_low_bits INTEGER NOT NULL,"
      "value INTEGER NOT NULL)";
  if (!db_->Execute(kAggregatableContributionsTableSql))
    return false;

  // Optimizes contribution look up by aggregation id during calls to
  // `DeleteAggregatableContributions()`.
  static constexpr char kContributionAggregationIdIndexSql[] =
      "CREATE INDEX IF NOT EXISTS contribution_aggregation_id_idx "
      "ON aggregatable_contributions(aggregation_id)";
  if (!db_->Execute(kContributionAggregationIdIndexSql))
    return false;

  if (!meta_table_.Init(db_.get(), kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    return false;
  }

  if (!transaction.Commit())
    return false;

  base::UmaHistogramMediumTimes("Conversions.Storage.CreationTime",
                                base::ThreadTicks::Now() - start_timestamp);
  return true;
}

void AttributionStorageSql::DatabaseErrorCallback(int extended_error,
                                                  sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Attempt to recover a corrupt database, unless it is setup in memory.
  if (sql::Recovery::ShouldRecover(extended_error) &&
      (path_to_database_.value() != kInMemoryPath)) {
    // Prevent reentrant calls.
    db_->reset_error_callback();

    // After this call, the |db_| handle is poisoned so that future calls will
    // return errors until the handle is re-opened.
    sql::Recovery::RecoverDatabaseWithMetaVersion(db_.get(), path_to_database_);

    // The DLOG(FATAL) below is intended to draw immediate attention to errors
    // in newly-written code.  Database corruption is generally a result of OS
    // or hardware issues, not coding errors at the client level, so displaying
    // the error would probably lead to confusion.  The ignored call signals the
    // test-expectation framework that the error was handled.
    std::ignore = sql::Database::IsExpectedSqliteError(extended_error);
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error) &&
      !ignore_errors_for_testing_)
    DLOG(FATAL) << db_->GetErrorMessage();

  // Consider the  database closed if we did not attempt to recover so we did
  // not produce further errors.
  db_init_status_ = DbStatus::kClosed;
}

bool AttributionStorageSql::
    HasCapacityForUniqueDestinationLimitForPendingSource(
        const StorableSource& source) {
  const int max = delegate_->GetMaxDestinationsPerSourceSiteReportingOrigin();
  // TODO(apaseltiner): We could just make
  // `GetMaxDestinationsPerSourceSiteReportingOrigin()` return `size_t`, but it
  // would be inconsistent with the other `AttributionStorageDelegate`
  // methods.
  DCHECK_GT(max, 0);

  const std::string serialized_conversion_destination =
      source.common_info().ConversionDestination().Serialize();

  // Optimized by `kImpressionSiteReportingOriginIndexSql`.
  static constexpr char kSelectSourcesSql[] =
      "SELECT destination_site FROM sources "
      DCHECK_SQL_INDEXED_BY("active_unattributed_sources_by_site_reporting_origin")
      "WHERE source_site=? AND reporting_origin=? "
      "AND event_level_active=1 AND num_attributions=0 AND "
      "aggregatable_active=1 AND aggregatable_budget_consumed=0";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectSourcesSql));
  statement.BindString(0, source.common_info().ImpressionSite().Serialize());
  statement.BindString(1, SerializePotentiallyTrustworthyOrigin(
                              source.common_info().reporting_origin()));

  base::flat_set<std::string> destinations;
  while (statement.Step()) {
    std::string destination = statement.ColumnString(0);

    // The destination isn't new, so it doesn't change the count.
    if (destination == serialized_conversion_destination)
      return true;

    destinations.insert(std::move(destination));

    if (destinations.size() == static_cast<size_t>(max))
      return false;
  }

  return statement.Succeeded();
}

bool AttributionStorageSql::DeleteSources(
    const std::vector<StoredSource::Id>& source_ids) {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteSourcesSql[] =
      "DELETE FROM sources WHERE source_id = ?";
  sql::Statement delete_impression_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteSourcesSql));

  for (StoredSource::Id source_id : source_ids) {
    delete_impression_statement.Reset(/*clear_bound_vars=*/true);
    delete_impression_statement.BindInt64(0, *source_id);
    if (!delete_impression_statement.Run())
      return false;
  }

  static constexpr char kDeleteDedupKeySql[] =
      "DELETE FROM dedup_keys WHERE source_id = ?";
  sql::Statement delete_dedup_key_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteDedupKeySql));

  for (StoredSource::Id source_id : source_ids) {
    delete_dedup_key_statement.Reset(/*clear_bound_vars=*/true);
    delete_dedup_key_statement.BindInt64(0, *source_id);
    if (!delete_dedup_key_statement.Run())
      return false;
  }

  return transaction.Commit();
}

bool AttributionStorageSql::AddAggregatableAttributionForTesting(
    const AttributionReport& report) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto* aggregatable_attribution =
      absl::get_if<AttributionReport::AggregatableAttributionData>(
          &report.data());
  DCHECK(aggregatable_attribution);

  if (!LazyInit(DbCreationPolicy::kCreateIfAbsent))
    return false;

  StoredSource::Id source_id = report.attribution_info().source.source_id();

  absl::optional<StoredSourceData> source_to_attribute =
      ReadSourceToAttribute(db_.get(), source_id);
  // This is only possible if there is a corrupt DB.
  if (!source_to_attribute.has_value())
    return false;

  return MaybeStoreAggregatableAttributionReport(
             report, source_to_attribute->aggregatable_budget_consumed) ==
         AggregatableResult::kSuccess;
}

bool AttributionStorageSql::ClearAggregatableAttributionsForOriginsInRange(
    base::Time delete_begin,
    base::Time delete_end,
    base::RepeatingCallback<bool(const url::Origin&)> filter,
    std::vector<StoredSource::Id>& source_ids_to_delete) {
  DCHECK_LE(delete_begin, delete_end);

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  // TODO(linnan): Considering optimizing SQL query by moving some logic to C++.
  // See the comment in crrev.com/c/3379484 for more information.
  static constexpr char kScanCandidateData[] =
      "SELECT I.source_origin,I.destination_origin,I.reporting_origin,"
      "I.source_id,A.aggregation_id "
      "FROM sources I LEFT JOIN aggregatable_report_metadata A "
      DCHECK_SQL_INDEXED_BY("aggregate_trigger_time_idx")
      "ON A.source_id=I.source_id WHERE"
      "(I.source_time BETWEEN ?1 AND ?2)OR"
      "(A.trigger_time BETWEEN ?1 AND ?2)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kScanCandidateData));
  statement.BindTime(0, delete_begin);
  statement.BindTime(1, delete_end);

  while (statement.Step()) {
    if (filter.is_null() ||
        filter.Run(DeserializeOrigin(statement.ColumnString(0))) ||
        filter.Run(DeserializeOrigin(statement.ColumnString(1))) ||
        filter.Run(DeserializeOrigin(statement.ColumnString(2)))) {
      source_ids_to_delete.emplace_back(statement.ColumnInt64(3));
      if (statement.GetColumnType(4) != sql::ColumnType::kNull &&
          !DeleteReportInternal(
              AttributionReport::AggregatableAttributionData::Id(
                  statement.ColumnInt64(4)))) {
        return false;
      }
    }
  }

  if (!statement.Succeeded())
    return false;

  return transaction.Commit();
}

bool AttributionStorageSql::DeleteReportInternal(
    AttributionReport::AggregatableAttributionData::Id aggregation_id) {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteAggregationSql[] =
      "DELETE FROM aggregatable_report_metadata WHERE aggregation_id=?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAggregationSql));
  statement.BindInt64(0, *aggregation_id);
  if (!statement.Run())
    return false;

  if (!DeleteAggregatableContributions(aggregation_id))
    return false;

  return transaction.Commit();
}

bool AttributionStorageSql::DeleteAggregatableContributions(
    AttributionReport::AggregatableAttributionData::Id aggregation_id) {
  static constexpr char kDeleteContributionsSql[] =
      // clang-format off
      "DELETE FROM aggregatable_contributions "
      DCHECK_SQL_INDEXED_BY("contribution_aggregation_id_idx")
      "WHERE aggregation_id=?";  // clang-format on
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteContributionsSql));
  statement.BindInt64(0, *aggregation_id);
  return statement.Run();
}

bool AttributionStorageSql::ClearAggregatableAttributionsForSourceIds(
    const std::vector<StoredSource::Id>& source_ids) {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteAggregationsSql[] =
      "DELETE FROM aggregatable_report_metadata "
      DCHECK_SQL_INDEXED_BY("aggregate_source_id_idx")
      "WHERE source_id=? "
      "RETURNING aggregation_id";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAggregationsSql));

  for (StoredSource::Id id : source_ids) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt64(0, *id);

    while (statement.Step()) {
      if (!DeleteAggregatableContributions(
              AttributionReport::AggregatableAttributionData::Id(
                  statement.ColumnInt64(0)))) {
        return false;
      }
    }

    if (!statement.Succeeded())
      return false;
  }

  return transaction.Commit();
}

std::vector<AttributionReport>
AttributionStorageSql::GetAggregatableAttributionReportsInternal(
    base::Time max_report_time,
    int limit) {
  static constexpr char kGetReportsSql[] =
      ATTRIBUTION_SELECT_AGGREGATABLE_REPORT_AND_SOURCE_COLUMNS_SQL
      "WHERE A.report_time<=? LIMIT ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetReportsSql));
  statement.BindTime(0, max_report_time);
  statement.BindInt(1, limit);

  std::vector<AttributionReport> reports;
  while (statement.Step()) {
    absl::optional<AttributionReport> report =
        ReadAggregatableAttributionReportFromStatement(statement);
    if (report.has_value())
      reports.push_back(std::move(*report));
  }

  if (!statement.Succeeded())
    return {};

  return reports;
}

std::vector<AggregatableHistogramContribution>
AttributionStorageSql::GetAggregatableContributions(
    AttributionReport::AggregatableAttributionData::Id aggregation_id) {
  static constexpr char kGetContributionsSql[] =
      "SELECT key_high_bits,key_low_bits,value "
      "FROM aggregatable_contributions "
      "WHERE aggregation_id=?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetContributionsSql));
  statement.BindInt64(0, *aggregation_id);

  std::vector<AggregatableHistogramContribution> contributions;
  while (statement.Step()) {
    absl::uint128 bucket_key =
        absl::MakeUint128(DeserializeUint64(statement.ColumnInt64(0)),
                          DeserializeUint64(statement.ColumnInt64(1)));
    int64_t value = statement.ColumnInt64(2);
    if (value <= 0 || value > std::numeric_limits<uint32_t>::max())
      return {};

    contributions.emplace_back(bucket_key, static_cast<uint32_t>(value));
  }

  return contributions;
}

RateLimitResult
AttributionStorageSql::AggregatableAttributionAllowedForBudgetLimit(
    const AttributionReport::AggregatableAttributionData&
        aggregatable_attribution,
    int64_t aggregatable_budget_consumed) {
  const int64_t budget = delegate_->GetAggregatableBudgetPerSource();
  DCHECK_GT(budget, 0);

  const int64_t capacity = budget > aggregatable_budget_consumed
                               ? budget - aggregatable_budget_consumed
                               : 0;

  if (capacity == 0)
    return RateLimitResult::kNotAllowed;

  const base::CheckedNumeric<int64_t> budget_required =
      aggregatable_attribution.BudgetRequired();
  if (!budget_required.IsValid() || budget_required.ValueOrDie() > capacity)
    return RateLimitResult::kNotAllowed;

  return RateLimitResult::kAllowed;
}

bool AttributionStorageSql::AdjustBudgetConsumedForSource(
    StoredSource::Id source_id,
    int64_t additional_budget_consumed) {
  DCHECK_GE(additional_budget_consumed, 0);

  static constexpr char kAdjustBudgetConsumedForSourceSql[] =
      "UPDATE sources "
      "SET aggregatable_budget_consumed=aggregatable_budget_consumed+? "
      "WHERE source_id=?";
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, kAdjustBudgetConsumedForSourceSql));
  statement.BindInt64(0, additional_budget_consumed);
  statement.BindInt64(1, *source_id);
  return statement.Run() && db_->GetLastChangeCount() == 1;
}

absl::optional<base::Time>
AttributionStorageSql::GetNextAggregatableAttributionReportTime(
    base::Time time) {
  static constexpr char kNextReportTimeSql[] = ATTRIBUTION_NEXT_REPORT_TIME_SQL(
      ATTRIBUTION_AGGREGATABLE_REPORT_METADATA_TABLE);
  return GetNextReportTime(SQL_FROM_HERE, kNextReportTimeSql, time);
}

absl::optional<base::Time>
AttributionStorageSql::AdjustOfflineAggregatableAttributionReportTimes(
    base::TimeDelta min_delay,
    base::TimeDelta max_delay,
    base::Time now) {
  static constexpr char kSetReportTimeSql[] = ATTRIBUTION_SET_REPORT_TIME_SQL(
      ATTRIBUTION_AGGREGATABLE_REPORT_METADATA_TABLE);
  if (!AdjustOfflineReportTimes(SQL_FROM_HERE, kSetReportTimeSql, min_delay,
                                max_delay, now)) {
    return absl::nullopt;
  }

  return GetNextAggregatableAttributionReportTime(base::Time::Min());
}

AggregatableResult
AttributionStorageSql::MaybeCreateAggregatableAttributionReport(
    const AttributionInfo& attribution_info,
    const AttributionTrigger& trigger,
    bool top_level_filters_match,
    absl::optional<AttributionReport>& report) {
  std::vector<AggregatableHistogramContribution> contributions =
      CreateAggregatableHistogram(
          attribution_info.source.common_info().filter_data(),
          attribution_info.source.common_info().aggregatable_source(),
          trigger.aggregatable_trigger());
  if (contributions.empty())
    return AggregatableResult::kNoHistograms;

  if (!top_level_filters_match)
    return AggregatableResult::kNoMatchingSourceFilterData;

  base::Time report_time =
      delegate_->GetAggregatableReportTime(attribution_info.time);

  report =
      AttributionReport(attribution_info, report_time, delegate_->NewReportID(),
                        AttributionReport::AggregatableAttributionData(
                            std::move(contributions),
                            /*id=*/absl::nullopt, report_time));

  return AggregatableResult::kSuccess;
}

bool AttributionStorageSql::StoreAggregatableAttributionReport(
    const AttributionReport& report) {
  const auto* aggregatable_attribution =
      absl::get_if<AttributionReport::AggregatableAttributionData>(
          &report.data());
  DCHECK(aggregatable_attribution);

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  const AttributionInfo& attribution_info = report.attribution_info();

  static constexpr char kInsertMetadataSql[] =
      "INSERT INTO aggregatable_report_metadata"
      "(source_id,trigger_time,debug_key,external_report_id,report_time,"
      "failed_send_attempts,initial_report_time)"
      "VALUES(?,?,?,?,?,0,?)";
  sql::Statement insert_metadata_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertMetadataSql));
  insert_metadata_statement.BindInt64(0, *attribution_info.source.source_id());
  insert_metadata_statement.BindTime(1, attribution_info.time);
  BindUint64OrNull(insert_metadata_statement, 2, attribution_info.debug_key);
  insert_metadata_statement.BindString(
      3, report.external_report_id().AsLowercaseString());
  insert_metadata_statement.BindTime(4, report.report_time());
  insert_metadata_statement.BindTime(
      5, aggregatable_attribution->initial_report_time);
  if (!insert_metadata_statement.Run())
    return false;

  AttributionReport::AggregatableAttributionData::Id aggregation_id(
      db_->GetLastInsertRowId());

  static constexpr char kInsertContributionsSql[] =
      "INSERT INTO aggregatable_contributions"
      "(aggregation_id,key_high_bits,key_low_bits,value)"
      "VALUES(?,?,?,?)";
  sql::Statement insert_contributions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertContributionsSql));

  for (const auto& contribution : aggregatable_attribution->contributions) {
    insert_contributions_statement.Reset(/*clear_bound_vars=*/true);
    insert_contributions_statement.BindInt64(0, *aggregation_id);
    insert_contributions_statement.BindInt64(
        1, SerializeUint64(absl::Uint128High64(contribution.key())));
    insert_contributions_statement.BindInt64(
        2, SerializeUint64(absl::Uint128Low64(contribution.key())));
    insert_contributions_statement.BindInt64(
        3, static_cast<int64_t>(contribution.value()));
    if (!insert_contributions_statement.Run())
      return false;
  }

  return transaction.Commit();
}

AggregatableResult
AttributionStorageSql::MaybeStoreAggregatableAttributionReport(
    const AttributionReport& report,
    int64_t aggregatable_budget_consumed) {
  const auto* aggregatable_attribution =
      absl::get_if<AttributionReport::AggregatableAttributionData>(
          &report.data());
  DCHECK(aggregatable_attribution);

  switch (AggregatableAttributionAllowedForBudgetLimit(
      *aggregatable_attribution, aggregatable_budget_consumed)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      return AggregatableResult::kInsufficientBudget;
    case RateLimitResult::kError:
      return AggregatableResult::kInternalError;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return AggregatableResult::kInternalError;

  if (!StoreAggregatableAttributionReport(report))
    return AggregatableResult::kInternalError;

  base::CheckedNumeric<int64_t> budget_required =
      aggregatable_attribution->BudgetRequired();
  // The value was already validated by
  // `AggregatableAttributionAllowedForBudgetLimit()` above.
  DCHECK(budget_required.IsValid());
  if (!AdjustBudgetConsumedForSource(
          report.attribution_info().source.source_id(),
          budget_required.ValueOrDie())) {
    return AggregatableResult::kInternalError;
  }

  if (!transaction.Commit())
    return AggregatableResult::kInternalError;

  return AggregatableResult::kSuccess;
}

// Helper to deserialize report rows. See `GetReport()` for the expected
// ordering of columns used for the input to this function.
absl::optional<AttributionReport>
AttributionStorageSql::ReadAggregatableAttributionReportFromStatement(
    sql::Statement& statement) {
  DCHECK_EQ(statement.ColumnCount(), kSourceColumnCount + 7);

  absl::optional<StoredSourceData> source_data =
      ReadSourceFromStatement(statement);
  if (!source_data.has_value())
    return absl::nullopt;

  int col = kSourceColumnCount;
  AttributionReport::AggregatableAttributionData::Id report_id(
      statement.ColumnInt64(col++));
  base::Time trigger_time = statement.ColumnTime(col++);
  base::Time report_time = statement.ColumnTime(col++);
  absl::optional<uint64_t> trigger_debug_key =
      ColumnUint64OrNull(statement, col++);
  base::GUID external_report_id =
      base::GUID::ParseLowercase(statement.ColumnString(col++));
  int failed_send_attempts = statement.ColumnInt(col++);
  base::Time initial_report_time = statement.ColumnTime(col++);

  // Ensure data is valid before continuing. This could happen if there is
  // database corruption.
  if (!external_report_id.is_valid() || failed_send_attempts < 0) {
    return absl::nullopt;
  }

  std::vector<AggregatableHistogramContribution> contributions =
      GetAggregatableContributions(report_id);
  if (contributions.empty())
    return absl::nullopt;

  AttributionReport report(
      AttributionInfo(std::move(source_data->source), trigger_time,
                      trigger_debug_key),
      report_time, std::move(external_report_id),
      AttributionReport::AggregatableAttributionData(
          std::move(contributions), report_id, initial_report_time));
  report.set_failed_send_attempts(failed_send_attempts);
  return report;
}

absl::optional<AttributionReport> AttributionStorageSql::GetReport(
    AttributionReport::AggregatableAttributionData::Id report_id) {
  static constexpr char kGetReportSql[] =
      ATTRIBUTION_SELECT_AGGREGATABLE_REPORT_AND_SOURCE_COLUMNS_SQL
      "WHERE A.aggregation_id = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetReportSql));
  statement.BindInt64(0, *report_id);

  if (!statement.Step())
    return absl::nullopt;

  return ReadAggregatableAttributionReportFromStatement(statement);
}

}  // namespace content
