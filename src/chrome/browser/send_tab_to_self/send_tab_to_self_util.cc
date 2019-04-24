// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/device_info/device_info.h"
#include "components/sync/device_info/device_info_sync_service.h"
#include "components/sync/device_info/device_info_tracker.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace send_tab_to_self {

bool IsFlagEnabled() {
  return base::FeatureList::IsEnabled(switches::kSyncSendTabToSelf);
}

bool IsUserSyncTypeEnabled(Profile* profile) {
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  return sync_service &&
         sync_service->GetPreferredDataTypes().Has(syncer::SEND_TAB_TO_SELF);
}

bool IsSyncingOnMultipleDevices(Profile* profile) {
  syncer::DeviceInfoSyncService* device_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  return device_sync_service &&
         device_sync_service->GetDeviceInfoTracker()->CountActiveDevices() > 1;
}

bool IsContentRequirementsMet(GURL& url, Profile* profile) {
  bool is_http_or_https = url.SchemeIsHTTPOrHTTPS();
  bool is_native_page = url.SchemeIs(content::kChromeUIScheme);
  bool is_incognito_mode =
      profile->GetProfileType() == Profile::INCOGNITO_PROFILE;
  return is_http_or_https && !is_native_page && !is_incognito_mode;
}

bool ShouldOfferFeature(Browser* browser) {
  if (!browser) {
    return false;
  }
  Profile* profile = browser->profile();
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!profile || !web_contents) {
    return false;
  }

  GURL url = web_contents->GetURL();
  return IsFlagEnabled() && IsUserSyncTypeEnabled(profile) &&
         IsSyncingOnMultipleDevices(profile) &&
         IsContentRequirementsMet(url, profile);
}

void CreateNewEntry(content::WebContents* tab, Profile* profile) {
  GURL url = tab->GetURL();
  std::string title = base::UTF16ToUTF8(tab->GetTitle());
  SendTabToSelfSyncServiceFactory::GetForProfile(profile)
      ->GetSendTabToSelfModel()
      ->AddEntry(url, title);
}

}  // namespace send_tab_to_self
