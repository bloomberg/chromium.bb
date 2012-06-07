// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_UTIL_DATA_TYPE_HISTOGRAM_H_
#define SYNC_UTIL_DATA_TYPE_HISTOGRAM_H_
#pragma once

#include "base/metrics/histogram.h"
#include "base/time.h"
#include "sync/internal_api/public/syncable/model_type.h"

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
// std::vector<syncable::ModelType> types = GetEntryTypes();
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
      case syncable::BOOKMARKS: \
        PER_DATA_TYPE_MACRO("Bookmarks"); \
        break; \
      case syncable::PREFERENCES: \
        PER_DATA_TYPE_MACRO("Preferences"); \
        break; \
      case syncable::PASSWORDS: \
        PER_DATA_TYPE_MACRO("Passwords"); \
        break; \
      case syncable::AUTOFILL: \
        PER_DATA_TYPE_MACRO("Autofill"); \
        break; \
      case syncable::AUTOFILL_PROFILE: \
        PER_DATA_TYPE_MACRO("AutofillProfiles"); \
        break; \
      case syncable::THEMES: \
        PER_DATA_TYPE_MACRO("Themes"); \
        break; \
      case syncable::TYPED_URLS: \
        PER_DATA_TYPE_MACRO("TypedUrls"); \
        break; \
      case syncable::EXTENSIONS: \
        PER_DATA_TYPE_MACRO("Extensions"); \
        break; \
      case syncable::NIGORI: \
        PER_DATA_TYPE_MACRO("Nigori"); \
        break; \
      case syncable::SEARCH_ENGINES: \
        PER_DATA_TYPE_MACRO("SearchEngines"); \
        break; \
      case syncable::SESSIONS: \
        PER_DATA_TYPE_MACRO("Sessions"); \
        break; \
      case syncable::APPS: \
        PER_DATA_TYPE_MACRO("Apps"); \
        break; \
      case syncable::APP_SETTINGS: \
        PER_DATA_TYPE_MACRO("AppSettings"); \
        break; \
      case syncable::EXTENSION_SETTINGS: \
        PER_DATA_TYPE_MACRO("ExtensionSettings"); \
        break; \
      case syncable::APP_NOTIFICATIONS: \
        PER_DATA_TYPE_MACRO("AppNotifications"); \
        break; \
      default: \
        NOTREACHED() << "Unknown datatype " \
                     << syncable::ModelTypeToString(datatype); \
    } \
  } while (0)

#endif  // SYNC_UTIL_DATA_TYPE_HISTOGRAM_H_
