// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_UTIL_DATA_TYPE_HISTOGRAM_H_
#define SYNC_UTIL_DATA_TYPE_HISTOGRAM_H_

#include "base/metrics/histogram.h"
#include "base/time.h"
#include "sync/internal_api/public/base/model_type.h"

// For now, this just implements UMA_HISTOGRAM_LONG_TIMES. This can be adjusted
// if we feel the min, max, or bucket count amount are not appropriate.
#define SYNC_FREQ_HISTOGRAM(name, time) UMA_HISTOGRAM_CUSTOM_TIMES( \
    name, time, base::TimeDelta::FromMilliseconds(1), \
    base::TimeDelta::FromHours(1), 50)

// Helper macro for datatype specific histograms. For each datatype, invokes
// a pre-defined PER_DATA_TYPE_MACRO(type_str), where |type_str| is the string
// version of the datatype.
//
// Example usage (ignoring newlines necessary for multiline macro):
// std::vector<syncer::ModelType> types = GetEntryTypes();
// for (int i = 0; i < types.size(); ++i) {
// #define PER_DATA_TYPE_MACRO(type_str)
//     UMA_HISTOGRAM_ENUMERATION("Sync." type_str "StartFailures",
//                               error, max_error);
//   SYNC_DATA_TYPE_HISTOGRAM(types[i]);
// #undef PER_DATA_TYPE_MACRO
// }
//
// TODO(zea): Once visual studio supports proper variadic argument replacement
// in macros, pass in the histogram method directly as a parameter.
// See http://connect.microsoft.com/VisualStudio/feedback/details/380090/
// variadic-macro-replacement#details
#define SYNC_DATA_TYPE_HISTOGRAM(datatype) \
  do { \
    switch (datatype) { \
      case ::syncer::BOOKMARKS: \
        PER_DATA_TYPE_MACRO("Bookmarks"); \
        break; \
      case ::syncer::PREFERENCES: \
        PER_DATA_TYPE_MACRO("Preferences"); \
        break; \
      case ::syncer::PASSWORDS: \
        PER_DATA_TYPE_MACRO("Passwords"); \
        break; \
      case ::syncer::AUTOFILL: \
        PER_DATA_TYPE_MACRO("Autofill"); \
        break; \
      case ::syncer::AUTOFILL_PROFILE: \
        PER_DATA_TYPE_MACRO("AutofillProfiles"); \
        break; \
      case ::syncer::THEMES: \
        PER_DATA_TYPE_MACRO("Themes"); \
        break; \
      case ::syncer::TYPED_URLS: \
        PER_DATA_TYPE_MACRO("TypedUrls"); \
        break; \
      case ::syncer::EXTENSIONS: \
        PER_DATA_TYPE_MACRO("Extensions"); \
        break; \
      case ::syncer::NIGORI: \
        PER_DATA_TYPE_MACRO("Nigori"); \
        break; \
      case ::syncer::SEARCH_ENGINES: \
        PER_DATA_TYPE_MACRO("SearchEngines"); \
        break; \
      case ::syncer::SESSIONS: \
        PER_DATA_TYPE_MACRO("Sessions"); \
        break; \
      case ::syncer::APPS: \
        PER_DATA_TYPE_MACRO("Apps"); \
        break; \
      case ::syncer::APP_SETTINGS: \
        PER_DATA_TYPE_MACRO("AppSettings"); \
        break; \
      case ::syncer::EXTENSION_SETTINGS: \
        PER_DATA_TYPE_MACRO("ExtensionSettings"); \
        break; \
      case ::syncer::APP_NOTIFICATIONS: \
        PER_DATA_TYPE_MACRO("AppNotifications"); \
        break; \
      case ::syncer::HISTORY_DELETE_DIRECTIVES: \
        PER_DATA_TYPE_MACRO("HistoryDeleteDirectives"); \
        break; \
      case ::syncer::SYNCED_NOTIFICATIONS: \
        PER_DATA_TYPE_MACRO("SyncedNotifications"); \
        break; \
      case ::syncer::DEVICE_INFO: \
        PER_DATA_TYPE_MACRO("DeviceInfo"); \
        break; \
      case ::syncer::EXPERIMENTS: \
        PER_DATA_TYPE_MACRO("Experiments"); \
        break; \
      default: \
        NOTREACHED() << "Unknown datatype " \
                     << ::syncer::ModelTypeToString(datatype);  \
    } \
  } while (0)

#endif  // SYNC_UTIL_DATA_TYPE_HISTOGRAM_H_
