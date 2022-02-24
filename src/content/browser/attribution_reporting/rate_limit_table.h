// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_RATE_LIMIT_TABLE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_RATE_LIMIT_TABLE_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/common/content_export.h"

namespace sql {
class Database;
}  // namespace sql

namespace url {
class Origin;
}  // namespace url

namespace content {

struct AttributionInfo;
class AttributionStorageDelegate;
class CommonSourceInfo;
class StorableSource;

// Manages storage for rate-limiting sources and attributions.
// This class may be constructed on any sequence but must be accessed and
// destroyed on the same sequence. The sequence must outlive |this|.
class CONTENT_EXPORT RateLimitTable {
 public:
  enum class Result {
    kAllowed,
    kNotAllowed,
    kError,
  };

  // We have separate reporting origin rate limits for sources and attributions.
  // This enum helps us differentiate between these two cases in the database.
  //
  // These values are persisted to the DB. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // The enum is only exposed here for use in unit tests.
  enum class Scope {
    kSource = 0,
    kAttribution = 1,
  };

  explicit RateLimitTable(const AttributionStorageDelegate* delegate);
  RateLimitTable(const RateLimitTable& other) = delete;
  RateLimitTable& operator=(const RateLimitTable& other) = delete;
  RateLimitTable(RateLimitTable&& other) = delete;
  RateLimitTable& operator=(RateLimitTable&& other) = delete;
  ~RateLimitTable();

  // Creates the table in |db| if it doesn't exist.
  // Returns false on failure.
  [[nodiscard]] bool CreateTable(sql::Database* db);

  // Returns false on failure.
  [[nodiscard]] bool AddRateLimitForSource(sql::Database* db,
                                           const StoredSource& source);

  // Returns false on failure.
  [[nodiscard]] bool AddRateLimitForAttribution(
      sql::Database* db,
      const AttributionInfo& attribution_info);

  [[nodiscard]] Result SourceAllowedForReportingOriginLimit(
      sql::Database* db,
      const StorableSource& source);

  [[nodiscard]] Result AttributionAllowedForReportingOriginLimit(
      sql::Database* db,
      const AttributionInfo& attribution_info);

  [[nodiscard]] Result AttributionAllowedForAttributionLimit(
      sql::Database* db,
      const AttributionInfo& attribution_info);

  // These should be 1:1 with |AttributionStorageSql|'s |ClearData| functions.
  // Returns false on failure.
  [[nodiscard]] bool ClearAllDataAllTime(sql::Database* db);
  // Returns false on failure.
  [[nodiscard]] bool ClearDataForOriginsInRange(
      sql::Database* db,
      base::Time delete_begin,
      base::Time delete_end,
      base::RepeatingCallback<bool(const url::Origin&)> filter);
  // Returns false on failure.
  [[nodiscard]] bool ClearDataForSourceIds(
      sql::Database* db,
      const std::vector<StoredSource::Id>& source_ids);

 private:
  [[nodiscard]] bool AddRateLimit(sql::Database* db,
                                  Scope scope,
                                  const StoredSource& source,
                                  base::Time time)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] Result AllowedForReportingOriginLimit(
      sql::Database* db,
      Scope scope,
      const CommonSourceInfo& common_info,
      base::Time time) VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns false on failure.
  [[nodiscard]] bool ClearAllDataInRange(sql::Database* db,
                                         base::Time delete_begin,
                                         base::Time delete_end)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Deletes data in the table older than the window determined by
  // |delegate_->GetRateLimits()|.
  // Returns false on failure.
  [[nodiscard]] bool DeleteExpiredRateLimits(sql::Database* db)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Must outlive |this|.
  raw_ptr<const AttributionStorageDelegate> delegate_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Time at which `DeleteExpiredRateLimits()` was last called. Initialized to
  // the NULL time.
  base::Time last_cleared_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_RATE_LIMIT_TABLE_H_
