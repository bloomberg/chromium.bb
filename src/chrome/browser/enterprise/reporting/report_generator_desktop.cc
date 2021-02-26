// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/report_generator_desktop.h"

#include <utility>

#if defined(OS_CHROMEOS)
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/enterprise/reporting/android_app_info_generator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#endif  // defined(OS_CHROMEOS)

namespace em = enterprise_management;

namespace enterprise_reporting {

// TODO(crbug.com/1102047): Split up Chrome OS reporting code into its own
// delegates, then move this method's implementation to ReportGeneratorChromeOS.
void ReportGeneratorDesktop::SetAndroidAppInfos(
    ReportGenerator::ReportRequest* basic_request) {
#if defined(OS_CHROMEOS)
  DCHECK(basic_request);
  basic_request->clear_android_app_infos();

  // Android application is only supported for primary profile.
  Profile* primary_profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();

  if (!arc::IsArcPlayStoreEnabledForProfile(primary_profile))
    return;

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(primary_profile);

  if (!prefs) {
    LOG(ERROR) << base::StringPrintf(
        "Failed to generate ArcAppListPrefs instance for primary user profile "
        "(debug name: %s).",
        primary_profile->GetDebugName().c_str());
    return;
  }

  AndroidAppInfoGenerator generator;
  for (std::string app_id : prefs->GetAppIds()) {
    basic_request->mutable_android_app_infos()->AddAllocated(
        generator.Generate(prefs, app_id).release());
  }
#endif  // defined(OS_CHROMEOS)
}

}  // namespace enterprise_reporting
