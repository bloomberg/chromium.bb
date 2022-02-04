// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace send_tab_to_self {

void CreateNewEntry(content::WebContents* tab,
                    const std::string& target_device_name,
                    const std::string& target_device_guid,
                    const GURL& link_url) {
  DCHECK(tab);

  GURL shared_url = link_url;
  std::string title;
  base::Time navigation_time = base::Time();

  content::NavigationEntry* navigation_entry =
      tab->GetController().GetLastCommittedEntry();

  // This should either be a valid link share or a valid tab share.
  DCHECK(link_url.is_valid() || navigation_entry);

  if (!link_url.is_valid()) {
    // This is not link share, get the values from the last navigation entry.
    shared_url = navigation_entry->GetURL();
    title = base::UTF16ToUTF8(navigation_entry->GetTitle());
    navigation_time = navigation_entry->GetTimestamp();
  }

  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  DCHECK(profile);
  SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  DCHECK(model);

  if (!model->IsReady()) {
    // TODO(https://crbug.com/1280681): Is this legit? In STTSv2, there may not
    // *be* a DesktopNotificationHandler for profile, and we're violating the
    // lifetime rules of DesktopNotificationHandler here I think.
    DesktopNotificationHandler(profile).DisplayFailureMessage(shared_url);
    return;
  }

  model->AddEntry(shared_url, title, navigation_time, target_device_guid);

  SendTabToSelfBubbleController* controller = send_tab_to_self::
      SendTabToSelfBubbleController::CreateOrGetFromWebContents(tab);
  controller->ShowConfirmationMessage();
}

void ShareToSingleTarget(content::WebContents* tab, const GURL& link_url) {
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  DCHECK_EQ(GetValidDeviceCount(profile), 1u);
  const std::vector<TargetDeviceInfo>& devices =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel()
          ->GetTargetDeviceInfoSortedList();
  CreateNewEntry(tab, devices.begin()->device_name, devices.begin()->cache_guid,
                 link_url);
}

size_t GetValidDeviceCount(Profile* profile) {
  SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  DCHECK(service);
  SendTabToSelfModel* model = service->GetSendTabToSelfModel();
  DCHECK(model);
  const std::vector<TargetDeviceInfo>& devices =
      model->GetTargetDeviceInfoSortedList();
  return devices.size();
}

std::u16string GetSingleTargetDeviceName(Profile* profile) {
  DCHECK_EQ(GetValidDeviceCount(profile), 1u);
  return base::UTF8ToUTF16(
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel()
          ->GetTargetDeviceInfoSortedList()
          .begin()
          ->device_name);
}

}  // namespace send_tab_to_self
