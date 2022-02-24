// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/page_info/about_this_site_service_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/page_info/core/about_this_site_service.h"
#include "components/permissions/permission_manager.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_ui_utils.h"
#endif

ChromePageInfoUiDelegate::ChromePageInfoUiDelegate(
    content::WebContents* web_contents,
    const GURL& site_url)
    : web_contents_(web_contents), site_url_(site_url) {}

bool ChromePageInfoUiDelegate::ShouldShowAllow(ContentSettingsType type) {
  switch (type) {
    // Notifications and idle detection do not support CONTENT_SETTING_ALLOW in
    // incognito.
    case ContentSettingsType::NOTIFICATIONS:
    case ContentSettingsType::IDLE_DETECTION:
      return !GetProfile()->IsOffTheRecord();
    // Media only supports CONTENT_SETTING_ALLOW for secure origins.
    case ContentSettingsType::MEDIASTREAM_MIC:
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return network::IsUrlPotentiallyTrustworthy(site_url_);
    // Chooser permissions do not support CONTENT_SETTING_ALLOW.
    case ContentSettingsType::SERIAL_GUARD:
    case ContentSettingsType::USB_GUARD:
    case ContentSettingsType::BLUETOOTH_GUARD:
    case ContentSettingsType::HID_GUARD:
    // Bluetooth scanning does not support CONTENT_SETTING_ALLOW.
    case ContentSettingsType::BLUETOOTH_SCANNING:
    // File system write does not support CONTENT_SETTING_ALLOW.
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      return false;
    default:
      return true;
  }
}

std::u16string ChromePageInfoUiDelegate::GetAutomaticallyBlockedReason(
    ContentSettingsType type) {
  switch (type) {
    // Notifications and idle detection do not support CONTENT_SETTING_ALLOW in
    // incognito.
    case ContentSettingsType::NOTIFICATIONS:
    case ContentSettingsType::IDLE_DETECTION: {
      if (GetProfile()->IsOffTheRecord()) {
        return l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_STATE_TEXT_NOT_ALLOWED_IN_INCOGNITO);
      }
      break;
    }
    // Media only supports CONTENT_SETTING_ALLOW for secure origins.
    // TODO(crbug.com/1227679): This string can probably be removed.
    case ContentSettingsType::MEDIASTREAM_MIC:
    case ContentSettingsType::MEDIASTREAM_CAMERA: {
      if (!network::IsUrlPotentiallyTrustworthy(site_url_)) {
        return l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_STATE_TEXT_NOT_ALLOWED_INSECURE);
      }
      break;
    }
    default:
      break;
  }

  return std::u16string();
}

absl::optional<page_info::proto::SiteInfo>
ChromePageInfoUiDelegate::GetAboutThisSiteInfo() {
  if (auto* service =
          AboutThisSiteServiceFactory::GetForProfile(GetProfile())) {
    return service->GetAboutThisSiteInfo(
        site_url_, ukm::GetSourceIdForWebContentsDocument(web_contents_));
  }
  return absl::nullopt;
}

void ChromePageInfoUiDelegate::AboutThisSiteSourceClicked(
    GURL url,
    const ui::Event& event) {
  // TODO(crbug.com/1250653): Consider moving this to presenter as other methods
  // that open web pages.
  web_contents_->OpenURL(content::OpenURLParams(
      url, content::Referrer(),
      ui::DispositionFromEventFlags(event.flags(),
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB),
      ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false));
}

bool ChromePageInfoUiDelegate::ShouldShowAsk(ContentSettingsType type) {
  return permissions::PermissionUtil::IsGuardContentSetting(type);
}

#if !BUILDFLAG(IS_ANDROID)
bool ChromePageInfoUiDelegate::ShouldShowSiteSettings(int* link_text_id,
                                                      int* tooltip_text_id) {
  if (GetProfile()->IsGuestSession())
    return false;

  if (web_app::GetLabelIdsForAppManagementLinkInPageInfo(
          web_contents_, link_text_id, tooltip_text_id)) {
    return true;
  }

  *link_text_id = IDS_PAGE_INFO_SITE_SETTINGS_LINK;
  *tooltip_text_id = IDS_PAGE_INFO_SITE_SETTINGS_TOOLTIP;

  return true;
}

// TODO(crbug.com/1227074): Reconcile with LastTabStandingTracker.
bool ChromePageInfoUiDelegate::IsMultipleTabsOpen() {
  const extensions::WindowControllerList::ControllerList& windows =
      extensions::WindowControllerList::GetInstance()->windows();
  int count = 0;
  auto site_origin = site_url_.DeprecatedGetOriginAsURL();
  for (auto* window : windows) {
    const Browser* const browser = window->GetBrowser();
    if (!browser)
      continue;
    const TabStripModel* const tabs = browser->tab_strip_model();
    DCHECK(tabs);
    for (int i = 0; i < tabs->count(); ++i) {
      content::WebContents* const web_contents = tabs->GetWebContentsAt(i);
      if (web_contents->GetLastCommittedURL().DeprecatedGetOriginAsURL() ==
          site_origin) {
        count++;
      }
    }
  }
  return count > 1;
}

void ChromePageInfoUiDelegate::ShowPrivacySandboxSettings() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  chrome::ShowPrivacySandboxSettings(browser);
}

std::u16string ChromePageInfoUiDelegate::GetPermissionDetail(
    ContentSettingsType type) {
  switch (type) {
    // TODO(crbug.com/1228243): Reconcile with SiteDetailsPermissionElement.
    case ContentSettingsType::ADS:
      return l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_ADS_SUBTITLE);
    default:
      return {};
  }
}

bool ChromePageInfoUiDelegate::IsBlockAutoPlayEnabled() {
  return GetProfile()->GetPrefs()->GetBoolean(prefs::kBlockAutoplayEnabled);
}
#endif

permissions::PermissionResult ChromePageInfoUiDelegate::GetPermissionStatus(
    ContentSettingsType type) {
  return PermissionManagerFactory::GetForProfile(GetProfile())
      ->GetPermissionStatus(type, site_url_, site_url_);
}

Profile* ChromePageInfoUiDelegate::GetProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}
