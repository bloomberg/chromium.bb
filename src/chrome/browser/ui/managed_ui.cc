// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/managed_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/management_ui_handler.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "ui/chromeos/devicetype_utils.h"
#endif

namespace chrome {

bool ShouldDisplayManagedUi(Profile* profile) {
#if defined(OS_CHROMEOS)
  // Don't show the UI in demo mode.
  if (chromeos::DemoSession::IsDeviceInDemoMode())
    return false;

  // Don't show the UI for Unicorn accounts.
  if (profile->IsSupervised())
    return false;
#endif

  return enterprise_util::HasBrowserPoliciesApplied(profile);
}

base::string16 GetManagedUiMenuItemLabel(Profile* profile) {
  std::string management_domain =
      ManagementUIHandler::GetAccountDomain(profile);

  int string_id = IDS_MANAGED;
  std::vector<base::string16> replacements;
  if (!management_domain.empty()) {
    string_id = IDS_MANAGED_BY;
    replacements.push_back(base::UTF8ToUTF16(management_domain));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}

base::string16 GetManagedUiWebUILabel(Profile* profile) {
  std::string management_domain =
      ManagementUIHandler::GetAccountDomain(profile);

  int string_id = IDS_MANAGED_WITH_HYPERLINK;

  std::vector<base::string16> replacements;
  replacements.push_back(base::UTF8ToUTF16(chrome::kChromeUIManagementURL));
  if (!management_domain.empty()) {
    string_id = IDS_MANAGED_BY_WITH_HYPERLINK;
    replacements.push_back(base::UTF8ToUTF16(management_domain));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}

#if defined(OS_CHROMEOS)
base::string16 GetDeviceManagedUiWebUILabel(Profile* profile) {
  std::string management_domain =
      ManagementUIHandler::GetAccountDomain(profile);

  int string_id = IDS_DEVICE_MANAGED_WITH_HYPERLINK;

  std::vector<base::string16> replacements;
  replacements.push_back(base::UTF8ToUTF16(chrome::kChromeUIManagementURL));
  replacements.push_back(ui::GetChromeOSDeviceName());
  if (!management_domain.empty()) {
    string_id = IDS_DEVICE_MANAGED_BY_WITH_HYPERLINK;
    replacements.push_back(base::UTF8ToUTF16(management_domain));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}
#endif

}  // namespace chrome
