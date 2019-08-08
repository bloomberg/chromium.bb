// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_deprecation_infobar_delegate.h"

#include "base/feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

// static
void FlashDeprecationInfoBarDelegate::Create(InfoBarService* infobar_service) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::make_unique<FlashDeprecationInfoBarDelegate>()));
}

// static
bool FlashDeprecationInfoBarDelegate::ShouldDisplayFlashDeprecation(
    Profile* profile) {
  DCHECK(profile);

  if (!base::FeatureList::IsEnabled(features::kFlashDeprecationWarning))
    return false;

  bool is_managed = false;
  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(profile);
  ContentSetting flash_setting =
      PluginUtils::UnsafeGetRawDefaultFlashContentSetting(settings_map,
                                                          &is_managed);

  // If the user can't do anything about their browser's Flash behavior,
  // there's no point to showing a Flash deprecation warning infobar.
  if (is_managed)
    return false;

  // Display the infobar if the Flash setting is anything other than BLOCK.
  return flash_setting != CONTENT_SETTING_BLOCK;
}

infobars::InfoBarDelegate::InfoBarIdentifier
FlashDeprecationInfoBarDelegate::GetIdentifier() const {
  return FLASH_DEPRECATION_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& FlashDeprecationInfoBarDelegate::GetVectorIcon() const {
  return kExtensionIcon;
}

base::string16 FlashDeprecationInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PLUGIN_FLASH_DEPRECATION_PROMPT);
}

int FlashDeprecationInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

base::string16 FlashDeprecationInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL FlashDeprecationInfoBarDelegate::GetLinkURL() const {
  return GURL(
      "https://www.blog.google/products/chrome/saying-goodbye-flash-chrome/");
}
