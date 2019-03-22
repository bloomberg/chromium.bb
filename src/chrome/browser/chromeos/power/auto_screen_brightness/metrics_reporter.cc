// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/metrics_reporter.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

// Interval for asking metrics::DailyEvent to check whether a day has passed.
constexpr base::TimeDelta kCheckDailyEventInternal =
    base::TimeDelta::FromSeconds(60);

// Prefs corresponding to UserAdjustment values.
constexpr std::array<const char*, 3> kDailyCountPrefs = {
    prefs::kAutoScreenBrightnessMetricsNoAlsUserAdjustmentCount,
    prefs::kAutoScreenBrightnessMetricsSupportedAlsUserAdjustmentCount,
    prefs::kAutoScreenBrightnessMetricsUnsupportedAlsUserAdjustmentCount,
};

// Histograms corresponding to UserAdjustment values.
constexpr std::array<const char*, 3> kDailyCountHistograms = {
    MetricsReporter::kNoAlsUserAdjustmentName,
    MetricsReporter::kSupportedAlsUserAdjustmentName,
    MetricsReporter::kUnsupportedAlsUserAdjustmentName,
};

}  // namespace

constexpr char MetricsReporter::kDailyEventIntervalName[];
constexpr char MetricsReporter::kNoAlsUserAdjustmentName[];
constexpr char MetricsReporter::kSupportedAlsUserAdjustmentName[];
constexpr char MetricsReporter::kUnsupportedAlsUserAdjustmentName[];

// This class is needed since metrics::DailyEvent requires taking ownership
// of its observers. It just forwards events to MetricsReporter.
class MetricsReporter::DailyEventObserver
    : public metrics::DailyEvent::Observer {
 public:
  explicit DailyEventObserver(MetricsReporter* reporter)
      : reporter_(reporter) {}
  ~DailyEventObserver() override = default;

  // metrics::DailyEvent::Observer:
  void OnDailyEvent(metrics::DailyEvent::IntervalType type) override {
    reporter_->ReportDailyMetrics(type);
  }

 private:
  MetricsReporter* reporter_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(DailyEventObserver);
};

void MetricsReporter::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  metrics::DailyEvent::RegisterPref(
      registry, prefs::kAutoScreenBrightnessMetricsDailySample);
  for (const char* daily_count_pref : kDailyCountPrefs) {
    registry->RegisterIntegerPref(daily_count_pref, 0);
  }
}

MetricsReporter::MetricsReporter(
    chromeos::PowerManagerClient* power_manager_client,
    PrefService* local_state_pref_service)
    : power_manager_client_observer_(this),
      pref_service_(local_state_pref_service),
      daily_event_(std::make_unique<metrics::DailyEvent>(
          pref_service_,
          prefs::kAutoScreenBrightnessMetricsDailySample,
          kDailyEventIntervalName)) {
  for (size_t i = 0; i < kDailyCountPrefs.size(); ++i) {
    daily_counts_[i] = pref_service_->GetInteger(kDailyCountPrefs[i]);
  }

  power_manager_client_observer_.Add(power_manager_client);

  daily_event_->AddObserver(std::make_unique<DailyEventObserver>(this));
  daily_event_->CheckInterval();
  timer_.Start(FROM_HERE, kCheckDailyEventInternal, daily_event_.get(),
               &metrics::DailyEvent::CheckInterval);
}

MetricsReporter::~MetricsReporter() = default;

void MetricsReporter::SuspendDone(const base::TimeDelta& duration) {
  daily_event_->CheckInterval();
}

void MetricsReporter::OnUserBrightnessChangeRequested(
    UserAdjustment user_adjustment) {
  const size_t user_adjustment_uint = static_cast<size_t>(user_adjustment);
  DCHECK_LT(user_adjustment_uint, kDailyCountPrefs.size());
  const char* daily_count_pref = kDailyCountPrefs[user_adjustment_uint];
  ++daily_counts_[user_adjustment_uint];
  pref_service_->SetInteger(daily_count_pref,
                            daily_counts_[user_adjustment_uint]);
}

void MetricsReporter::ReportDailyMetricsForTesting(
    metrics::DailyEvent::IntervalType type) {
  ReportDailyMetrics(type);
}

void MetricsReporter::ReportDailyMetrics(
    metrics::DailyEvent::IntervalType type) {
  // Don't send metrics on first run or if the clock is changed.
  if (type == metrics::DailyEvent::IntervalType::DAY_ELAPSED) {
    for (size_t i = 0; i < kDailyCountHistograms.size(); ++i) {
      base::UmaHistogramCounts100(kDailyCountHistograms[i], daily_counts_[i]);
    }
  }

  for (size_t i = 0; i < kDailyCountPrefs.size(); ++i) {
    daily_counts_[i] = 0;
    pref_service_->SetInteger(kDailyCountPrefs[i], 0);
  }
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
