// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_PREFS_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace extensions {
namespace enterprise_reporting {

// Controls reporting of OS/Chrome version information.
extern const char kReportVersionData[];

// Controls reporting of Chrome policy data and policy fetch timestamps.
extern const char kReportPolicyData[];

// Controls reporting of information that can identify machines.
extern const char kReportMachineIDData[];

// Controls reporting of information that can identify users.
extern const char kReportUserIDData[];

// Controls reporting of Chrome extensions and plugins data.
extern const char kReportExtensionsAndPluginsData[];

// Controls reporting of Safe browsing data.
extern const char kReportSafeBrowsingData[];

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace enterprise_reporting
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_PREFS_H_
