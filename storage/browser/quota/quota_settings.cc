// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_settings.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

#define UMA_HISTOGRAM_MBYTES(name, sample)                                     \
  UMA_HISTOGRAM_CUSTOM_COUNTS((name), static_cast<int>((sample) / kMBytes), 1, \
                              10 * 1024 * 1024 /* 10TB */, 100)

namespace storage {

namespace {

// Skews |value| by +/- |percent|.
int64_t RandomizeByPercent(int64_t value, int percent) {
  double random_percent = (base::RandDouble() - 0.5) * percent * 2;
  return value + (value * (random_percent / 100.0));
}

base::Optional<storage::QuotaSettings> CalculateNominalDynamicSettings(
    const base::FilePath& partition_path,
    bool is_incognito) {
  base::AssertBlockingAllowed();
  const int64_t kMBytes = 1024 * 1024;
  const int kRandomizedPercentage = 10;

  if (is_incognito) {
    // The incognito pool size is a fraction of the amount of system memory,
    // and the amount is capped to a hard limit.
    const double kIncognitoPoolSizeRatio = 0.1;  // 10%
    const int64_t kMaxIncognitoPoolSize = 300 * kMBytes;

    storage::QuotaSettings settings;
    settings.pool_size = std::min(
        RandomizeByPercent(kMaxIncognitoPoolSize, kRandomizedPercentage),
        static_cast<int64_t>(base::SysInfo::AmountOfPhysicalMemory() *
                             kIncognitoPoolSizeRatio));
    settings.per_host_quota = settings.pool_size / 3;
    settings.session_only_per_host_quota = settings.per_host_quota;
    settings.refresh_interval = base::TimeDelta::Max();
    return settings;
  }

  // The fraction of the device's storage the browser is willing to
  // use for temporary storage, this is applied after adjusting the
  // total to take os_accomodation into account.
  const double kTemporaryPoolSizeRatio = 1.0 / 3.0;  // 33%

  // The fraction of the device's storage the browser attempts to
  // keep free.
  const double kShouldRemainAvailableRatio = 0.1;  // 10%

  // The fraction of the device's storage the browser attempts to
  // keep free at all costs.
  const double kMustRemainAvailableRatio = 0.01;  // 1%

  // Determines the portion of the temp pool that can be
  // utilized by a single host (ie. 5 for 20%).
  const int kPerHostTemporaryPortion = 5;

  // SessionOnly (or ephemeral) origins are allotted a fraction of what
  // normal origins are provided, and the amount is capped to a hard limit.
  const double kSessionOnlyHostQuotaRatio = 0.1;  // 10%
  const int64_t kMaxSessionOnlyHostQuota = 300 * kMBytes;

  // os_accomodation is an estimate of how much storage is needed for
  // the os and essential application code outside of the browser.
  const int64_t kDefaultOSAccomodation =
#if defined(OS_ANDROID)
      1000 * kMBytes;
#elif defined(OS_CHROMEOS)
      1000 * kMBytes;
#elif defined(OS_FUCHSIA)
      1000 * kMBytes;
#elif defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
      10000 * kMBytes;
#else
#error "Port: Need to define an OS accomodation value for unknown OS."
#endif

  storage::QuotaSettings settings;

  int64_t total = base::SysInfo::AmountOfTotalDiskSpace(partition_path);
  if (total == -1) {
    LOG(ERROR) << "Unable to compute QuotaSettings.";
    return base::nullopt;
  }

  // If our hardcoded OS accomodation is too large for the volume size, define
  // the value as a fraction of the total volume size instead.
  int64_t os_accomodation =
      std::min(kDefaultOSAccomodation, static_cast<int64_t>(total * 0.8));
  UMA_HISTOGRAM_MBYTES("Quota.OSAccomodationDelta",
                       kDefaultOSAccomodation - os_accomodation);

  int64_t adjusted_total = total - os_accomodation;
  int64_t pool_size = adjusted_total * kTemporaryPoolSizeRatio;

  settings.pool_size = pool_size;
  settings.should_remain_available = total * kShouldRemainAvailableRatio;
  settings.must_remain_available = total * kMustRemainAvailableRatio;
  settings.per_host_quota = pool_size / kPerHostTemporaryPortion;
  settings.session_only_per_host_quota = std::min(
      RandomizeByPercent(kMaxSessionOnlyHostQuota, kRandomizedPercentage),
      static_cast<int64_t>(settings.per_host_quota *
                           kSessionOnlyHostQuotaRatio));
  settings.refresh_interval = base::TimeDelta::FromSeconds(60);
  return settings;
}

}  // namespace

void GetNominalDynamicSettings(const base::FilePath& partition_path,
                               bool is_incognito,
                               OptionalQuotaSettingsCallback callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BACKGROUND,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CalculateNominalDynamicSettings, partition_path,
                     is_incognito),
      std::move(callback));
}

}  // namespace storage
