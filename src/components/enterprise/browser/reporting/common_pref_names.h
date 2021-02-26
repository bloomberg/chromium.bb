// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_COMMON_PREF_NAMES_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_COMMON_PREF_NAMES_H_

#include "build/build_config.h"

namespace enterprise_reporting {

#if !defined(OS_ANDROID)
extern const char kCloudReportingEnabled[];
#endif

extern const char kLastUploadTimestamp[];

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_COMMON_PREF_NAMES_H_
