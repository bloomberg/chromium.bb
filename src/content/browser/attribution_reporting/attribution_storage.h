// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

struct AggregatableAttribution;

// This class provides an interface for persisting attribution data to
// disk, and performing queries on it. AttributionStorage should initialize
// itself. Calls to a AttributionStorage instance that failed to initialize
// properly should result in no-ops.
class AttributionStorage {
 public:
  struct CONTENT_EXPORT DeactivatedSource {
    enum class Reason {
      kReplacedByNewerSource,
      kReachedAttributionLimit,
    };

    DeactivatedSource(StoredSource source, Reason reason);
    ~DeactivatedSource();

    DeactivatedSource(const DeactivatedSource&);
    DeactivatedSource(DeactivatedSource&&);

    DeactivatedSource& operator=(const DeactivatedSource&);
    DeactivatedSource& operator=(DeactivatedSource&&);

    StoredSource source;
    Reason reason;
  };

  struct CONTENT_EXPORT StoreSourceResult {
    explicit StoreSourceResult(
        StorableSource::Result status,
        std::vector<DeactivatedSource> deactivated_sources = {},
        absl::optional<base::Time> min_fake_report_time = absl::nullopt);

    ~StoreSourceResult();

    StoreSourceResult(const StoreSourceResult&);
    StoreSourceResult(StoreSourceResult&&);

    StoreSourceResult& operator=(const StoreSourceResult&);
    StoreSourceResult& operator=(StoreSourceResult&&);

    StorableSource::Result status;
    std::vector<DeactivatedSource> deactivated_sources;
    // The earliest report time for any fake reports stored alongside the
    // source, if any.
    absl::optional<base::Time> min_fake_report_time;
  };

  virtual ~AttributionStorage() = default;

  // When adding a new method, also add it to
  // AttributionStorageTest.StorageUsedAfterFailedInitilization_FailsSilently.

  // Add |source| to storage. Two sources are considered
  // matching when they share a <reporting origin, attribution destination>
  // pair. When a source is stored, all matching sources that have already
  // converted are marked as inactive, and are no longer eligible for reporting.
  // Unconverted matching sources are not modified.
  // Returns at most `deactivated_source_return_limit` deactivated sources, to
  // put an upper bound on memory usage; use a negative number for no limit.
  virtual StoreSourceResult StoreSource(
      const StorableSource& source,
      int deactivated_source_return_limit = -1) = 0;

  class CONTENT_EXPORT CreateReportResult {
   public:
    explicit CreateReportResult(
        AttributionTrigger::Result status,
        absl::optional<AttributionReport> dropped_report = absl::nullopt,
        absl::optional<DeactivatedSource::Reason>
            dropped_report_source_deactivation_reason = absl::nullopt,
        absl::optional<base::Time> report_time = absl::nullopt);
    ~CreateReportResult();

    CreateReportResult(const CreateReportResult&);
    CreateReportResult(CreateReportResult&&);

    CreateReportResult& operator=(const CreateReportResult&);
    CreateReportResult& operator=(CreateReportResult&&);

    AttributionTrigger::Result status() const;

    const absl::optional<AttributionReport>& dropped_report() const;

    absl::optional<base::Time> report_time() const;

    absl::optional<DeactivatedSource> GetDeactivatedSource() const;

   private:
    AttributionTrigger::Result status_;

    // `AttributionTrigger::Result::kInternalError` is only associated with a
    // dropped report if the browser succeeded in running the
    // source-to-attribute logic.
    absl::optional<AttributionReport> dropped_report_;

    // Null unless `dropped_report_`'s source was deactivated.
    absl::optional<DeactivatedSource::Reason>
        dropped_report_source_deactivation_reason_;

    // Null unless `status` is `kSuccess` or `kSuccessDroppedLowerPriority`.
    absl::optional<base::Time> report_time_;
  };

  // Finds all stored sources matching a given `trigger`, and stores the
  // new associated report. Only active sources will receive new attributions.
  // Returns whether a new report has been scheduled/added to storage.
  virtual CreateReportResult MaybeCreateAndStoreReport(
      const AttributionTrigger& trigger) = 0;

  // Returns all of the reports that should be sent before
  // |max_report_time|. This call is logically const, and does not modify the
  // underlying storage. |limit| limits the number of reports to return; use
  // a negative number for no limit. Reports are shuffled before being returned.
  virtual std::vector<AttributionReport> GetAttributionsToReport(
      base::Time max_report_time,
      int limit = -1) = 0;

  // Returns the first report time strictly after `time`.
  virtual absl::optional<base::Time> GetNextReportTime(base::Time time) = 0;

  // Returns the reports with the given IDs. This call is logically const, and
  // does not modify the underlying storage.
  virtual std::vector<AttributionReport> GetReports(
      const std::vector<AttributionReport::EventLevelData::Id>& ids) = 0;

  // Returns all active sources in storage. Active sources are all
  // sources that can still convert. Sources that: are past expiry,
  // reached the attribution limit, or was marked inactive due to having
  // trigger and then superceded by a matching source should not be
  // returned. |limit| limits the number of sources to return; use
  // a negative number for no limit.
  virtual std::vector<StoredSource> GetActiveSources(int limit = -1) = 0;

  // Deletes the report with the given |report_id|. Returns
  // false if an error occurred.
  [[nodiscard]] virtual bool DeleteReport(AttributionReport::Id report_id) = 0;

  // Updates the number of failures associated with the given report, and sets
  // its report time to the given value. Should be called after a transient
  // failure to send the report so that it is retried later.
  [[nodiscard]] virtual bool UpdateReportForSendFailure(
      AttributionReport::EventLevelData::Id report_id,
      base::Time new_report_time) = 0;

  // Adjusts the report time of all reports that should have been sent while the
  // browser was offline, according to
  // `AttributionStorageDelegate::GetOfflineReportDelayConfig()`. If that
  // method returns null, no delay is applied. Otherwise, applies a random value
  // between `min_delay` and `max_delay`, both inclusive. Returns the new first
  // report time in storage, if any.
  virtual absl::optional<base::Time> AdjustOfflineReportTimes() = 0;

  // Deletes all data in storage for URLs matching |filter|, between
  // |delete_begin| and |delete_end| time. More specifically, this:
  // 1. Deletes all sources within the time range. If any report is
  //    attributed to this source it is also deleted.
  // 2. Deletes all reports within the time range. All sources
  //    attributed to the report are also deleted.
  //
  // Note: if |filter| is null, it means that all Origins should match.
  virtual void ClearData(
      base::Time delete_begin,
      base::Time delete_end,
      base::RepeatingCallback<bool(const url::Origin& origin)> filter) = 0;

  // Aggregate Attribution:
  [[nodiscard]] virtual bool AddAggregatableAttributionForTesting(
      const AggregatableAttribution& aggregatable_attribution) = 0;

  // Returns all of the aggregatable reports that should be sent before
  // `max_report_time`. This call is logically const, and does not modify the
  // underlying storage. `limit` limits the number of reports to return; use a
  // negative number for no limit.
  virtual std::vector<AttributionReport>
  GetAggregatableContributionReportsForTesting(base::Time max_report_time,
                                               int limit = -1) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_H_
