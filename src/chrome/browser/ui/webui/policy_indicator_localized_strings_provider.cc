// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_indicator_localized_strings_provider.h"

#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#endif

namespace policy_indicator {

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  int controlled_setting_policy_id = IDS_CONTROLLED_SETTING_POLICY;
#if defined(OS_CHROMEOS)
  if (chromeos::DemoSession::IsDeviceInDemoMode())
    controlled_setting_policy_id = IDS_CONTROLLED_SETTING_DEMO_SESSION;
#endif
  LocalizedString localized_strings[] = {
    {"controlledSettingPolicy", controlled_setting_policy_id},
    {"controlledSettingRecommendedMatches", IDS_CONTROLLED_SETTING_RECOMMENDED},
    {"controlledSettingRecommendedDiffers",
     IDS_CONTROLLED_SETTING_HAS_RECOMMENDATION},
    {"controlledSettingExtension", IDS_CONTROLLED_SETTING_EXTENSION},
    {"controlledSettingExtensionWithoutName",
     IDS_CONTROLLED_SETTING_EXTENSION_WITHOUT_NAME},
#if defined(OS_CHROMEOS)
    {"controlledSettingShared", IDS_CONTROLLED_SETTING_SHARED},
    {"controlledSettingOwner", IDS_CONTROLLED_SETTING_OWNER},
#endif
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          base::size(localized_strings));
}

}  // namespace policy_indicator
