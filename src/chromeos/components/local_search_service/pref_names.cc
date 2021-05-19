// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/pref_names.h"

namespace chromeos {
namespace local_search_service {
namespace prefs {

// TODO(thanhdng): clean this up after LSS is sandboxed.
const char kLocalSearchServiceSyncMetricsDailySample[] =
    "local_search_service_sync.metrics.daily_sample";
const char kLocalSearchServiceSyncMetricsCrosSettingsCount[] =
    "local_search_service_sync.metrics.cros_settings_count";
const char kLocalSearchServiceSyncMetricsHelpAppCount[] =
    "local_search_service_sync.metrics.help_app_count";

const char kLocalSearchServiceMetricsDailySample[] =
    "local_search_service.metrics.daily_sample";
const char kLocalSearchServiceMetricsCrosSettingsCount[] =
    "local_search_service.metrics.cros_settings_count";
const char kLocalSearchServiceMetricsHelpAppCount[] =
    "local_search_service.metrics.help_app_count";
const char kLocalSearchServiceMetricsHelpAppLauncherCount[] =
    "local_search_service.metrics.help_app_launcher_count";

}  // namespace prefs
}  // namespace local_search_service
}  // namespace chromeos
