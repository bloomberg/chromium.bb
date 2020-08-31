// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_SQL_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_SQL_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_storage.h"
#include "content/common/content_export.h"
#include "sql/database.h"

namespace base {
class Clock;
}  // namespace base

namespace content {

// Provides an implementation of ConversionStorage that is backed by SQLite.
// This class may be constructed on any sequence but must be accessed and
// destroyed on the same sequence. The sequence must outlive |this|.
class CONTENT_EXPORT ConversionStorageSql : public ConversionStorage {
 public:
  ConversionStorageSql(const base::FilePath& path_to_database_dir,
                       std::unique_ptr<Delegate> delegate,
                       const base::Clock* clock);
  ConversionStorageSql(const ConversionStorageSql& other) = delete;
  ConversionStorageSql& operator=(const ConversionStorageSql& other) = delete;
  ~ConversionStorageSql() override;

 private:
  // ConversionStorage
  bool Initialize() override;
  void StoreImpression(const StorableImpression& impression) override;
  int MaybeCreateAndStoreConversionReports(
      const StorableConversion& conversion) override;
  std::vector<ConversionReport> GetConversionsToReport(
      base::Time expiry_time) override;
  std::vector<StorableImpression> GetActiveImpressions() override;
  int DeleteExpiredImpressions() override;
  bool DeleteConversion(int64_t conversion_id) override;
  void ClearData(
      base::Time delete_begin,
      base::Time delete_end,
      base::RepeatingCallback<bool(const url::Origin&)> filter) override;

  // Variants of ClearData that assume all Origins match the filter.
  void ClearAllDataInRange(base::Time delete_begin, base::Time delete_end);
  void ClearAllDataAllTime();

  bool HasCapacityForStoringImpression(const std::string& serialized_origin);
  bool HasCapacityForStoringConversion(const std::string& serialized_origin);

  bool InitializeSchema();

  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  const base::FilePath path_to_database_;
  sql::Database db_;

  // Must outlive |this|.
  const base::Clock* clock_;

  std::unique_ptr<Delegate> delegate_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ConversionStorageSql> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_SQL_H_
