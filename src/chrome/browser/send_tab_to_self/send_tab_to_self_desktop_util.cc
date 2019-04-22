// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/theme_resources.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"

namespace send_tab_to_self {

void CreateNewEntry(content::WebContents* tab, const GURL& link_url) {
  content::NavigationEntry* navigation_entry =
      tab->GetController().GetLastCommittedEntry();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  GURL url = navigation_entry->GetURL();
  std::string title = base::UTF16ToUTF8(navigation_entry->GetTitle());
  base::Time navigation_time = navigation_entry->GetTimestamp();

  // TODO(crbug/946804) Add target device.
  std::string target_device;
  SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();

  UMA_HISTOGRAM_BOOLEAN("SendTabToSelf.Sync.ModelLoadedInTime",
                        model->IsReady());
  if (!model->IsReady()) {
    DesktopNotificationHandler(profile).DisplayFailureMessage(url);
    return;
  }

  const SendTabToSelfEntry* entry;
  if (link_url.is_valid()) {
    // When share a link.
    entry = model->AddEntry(link_url, "", base::Time(), target_device);
  } else {
    // When share a tab.
    entry = model->AddEntry(url, title, navigation_time, target_device);
  }
  if (entry) {
    DesktopNotificationHandler(profile).DisplaySendingConfirmation(*entry);
  } else {
    DesktopNotificationHandler(profile).DisplayFailureMessage(url);
  }
}

gfx::ImageSkia* GetImageSkia() {
  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  bool is_dark = native_theme && native_theme->SystemDarkModeEnabled();
  int resource_id = is_dark ? IDR_SEND_TAB_TO_SELF_ICON_DARK
                            : IDR_SEND_TAB_TO_SELF_ICON_LIGHT;
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
}

const gfx::Image GetImageForNotification() {
#if defined(OS_WIN)
  // Set image to be chrome logo on window.
  // Otherwise there will be a void area on the left side of the
  // notification on Windows.
  int resource_id;
  switch (chrome::GetChannel()) {
#if defined(GOOGLE_CHROME_BUILD)
    case version_info::Channel::CANARY:
      resource_id = IDR_PRODUCT_LOGO_32_CANARY;
      break;
    case version_info::Channel::DEV:
      resource_id = IDR_PRODUCT_LOGO_32_DEV;
      break;
    case version_info::Channel::BETA:
      resource_id = IDR_PRODUCT_LOGO_32_BETA;
      break;
    case version_info::Channel::STABLE:
      resource_id = IDR_PRODUCT_LOGO_32;
      break;
#else
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE:
      NOTREACHED();
      FALLTHROUGH;
#endif
    case version_info::Channel::UNKNOWN:
      resource_id = IDR_PRODUCT_LOGO_32;
      break;
  }
  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(resource_id);
#endif

  return gfx::Image();
}
void RecordSendTabToSelfClickResult(std::string context_menu,
                                    SendTabToSelfClickResult state) {
  base::UmaHistogramEnumeration(
      "SendTabToSelf." + context_menu + ".ClickResult", state);
}

}  // namespace send_tab_to_self
