// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/network_ui/traffic_counters_resource_provider.h"

#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_generated_resources.h"

namespace chromeos {
namespace traffic_counters {

namespace {

constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"TrafficCountersUnknown", IDS_TRAFFIC_COUNTERS_UNKNOWN},
    {"TrafficCountersChrome", IDS_TRAFFIC_COUNTERS_CHROME},
    {"TrafficCountersUser", IDS_TRAFFIC_COUNTERS_USER},
    {"TrafficCountersArc", IDS_TRAFFIC_COUNTERS_ARC},
    {"TrafficCountersCrosvm", IDS_TRAFFIC_COUNTERS_CROSVM},
    {"TrafficCountersPluginvm", IDS_TRAFFIC_COUNTERS_PLUGINVM},
    {"TrafficCountersUpdateEngine", IDS_TRAFFIC_COUNTERS_UPDATE_ENGINE},
    {"TrafficCountersVpn", IDS_TRAFFIC_COUNTERS_VPN},
    {"TrafficCountersSystem", IDS_TRAFFIC_COUNTERS_SYSTEM},
    {"TrafficCountersGuid", IDS_TRAFFIC_COUNTERS_GUID},
    {"TrafficCountersName", IDS_TRAFFIC_COUNTERS_NAME},
    {"TrafficCountersTrafficCounters", IDS_TRAFFIC_COUNTERS_TRAFFIC_COUNTERS},
    {"TrafficCountersRequestTrafficCounters",
     IDS_TRAFFIC_COUNTERS_REQUEST_TRAFFIC_COUNTERS},
    {"TrafficCountersResetTrafficCounters",
     IDS_TRAFFIC_COUNTERS_RESET_TRAFFIC_COUNTERS},
    {"TrafficCountersLastResetTime", IDS_TRAFFIC_COUNTERS_LAST_RESET_TIME},
};

}  // namespace

void AddResources(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

}  // namespace traffic_counters
}  // namespace chromeos
