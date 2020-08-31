// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_CONVERTER_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_CONVERTER_H_

#include "base/values.h"
#include "chrome/services/app_service/public/cpp/preferred_apps_list.h"

namespace apps {

extern const char kConditionTypeKey[];
extern const char kConditionValuesKey[];
extern const char kValueKey[];
extern const char kMatchTypeKey[];
extern const char kAppIdKey[];
extern const char kIntentFilterKey[];

// Convert the PreferredAppsList struct to base::Value to write to JSON file.
// e.g. for preferred app with |app_id| "abcdefg", and |intent_filter| for url
// https://www.google.com/abc.
// The converted base::Value format will be:
//[ {"app_id": "abcdefg",
//    "intent_filter": [ {
//       "condition_type": 0,
//       "condition_values": [ {
//          "match_type": 0,
//          "value": "https"
//       } ]
//    }, {
//       "condition_type": 1,
//       "condition_values": [ {
//          "match_type": 0,
//          "value": "www.google.com"
//       } ]
//    }, {
//       "condition_type": 2,
//       "condition_values": [ {
//          "match_type": 2,
//          "value": "/abc"
//       } ]
//    } ]
// } ]
base::Value ConvertPreferredAppsToValue(
    const PreferredAppsList::PreferredApps& preferred_apps);

// Parse the base::Value read from JSON file back to preferred apps struct.
PreferredAppsList::PreferredApps ParseValueToPreferredApps(
    const base::Value& preferred_apps_value);

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_CONVERTER_H_
