// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing_hub/sharing_hub_model.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/share/core/share_targets.h"
#include "chrome/browser/share/proto/share_target.pb.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace sharing_hub {

namespace {

const char kUrlReplace[] = "%(escaped_url)";
const char kTitleReplace[] = "%(escaped_title)";

gfx::Image DecodeIcon(std::string str) {
  std::string icon_str;
  base::Base64Decode(str, &icon_str);
  return gfx::Image::CreateFrom1xPNGBytes(
      reinterpret_cast<const unsigned char*>(icon_str.data()), icon_str.size());
}

}  // namespace

SharingHubAction::SharingHubAction(int command_id,
                                   std::u16string title,
                                   const gfx::VectorIcon* icon,
                                   bool is_first_party,
                                   gfx::ImageSkia third_party_icon,
                                   std::string feature_name_for_metrics)
    : command_id(command_id),
      title(title),
      icon(icon),
      is_first_party(is_first_party),
      third_party_icon(third_party_icon),
      feature_name_for_metrics(feature_name_for_metrics) {}

SharingHubAction::SharingHubAction(const SharingHubAction& src) = default;
SharingHubAction& SharingHubAction::operator=(const SharingHubAction& src) =
    default;
SharingHubAction::SharingHubAction(SharingHubAction&& src) = default;
SharingHubAction& SharingHubAction::operator=(SharingHubAction&& src) = default;

SharingHubModel::SharingHubModel(content::BrowserContext* context)
    : context_(context) {
  PopulateFirstPartyActions();
  sharing::ShareTargets::GetInstance()->AddObserver(this);
}

SharingHubModel::~SharingHubModel() {
  sharing::ShareTargets::GetInstance()->RemoveObserver(this);
}

void SharingHubModel::GetFirstPartyActionList(
    content::WebContents* web_contents,
    std::vector<SharingHubAction>* list) {
  for (const auto& action : first_party_action_list_) {
    if (action.command_id == IDC_SEND_TAB_TO_SELF) {
      if (DoShowSendTabToSelfForWebContents(web_contents)) {
        list->push_back(action);
      }
    } else if (action.command_id == IDC_QRCODE_GENERATOR) {
      if (qrcode_generator::QRCodeGeneratorBubbleController::
              IsGeneratorAvailable(web_contents->GetLastCommittedURL())) {
        list->push_back(action);
      }
    } else if (action.command_id == IDC_SAVE_PAGE) {
      if (chrome::CanSavePage(
              chrome::FindBrowserWithWebContents(web_contents))) {
        list->push_back(action);
      }
    } else {
      list->push_back(action);
    }
  }
}

void SharingHubModel::GetThirdPartyActionList(
    std::vector<SharingHubAction>* list) {
  for (const auto& action : third_party_action_list_) {
    list->push_back(action);
  }
}

void SharingHubModel::ExecuteThirdPartyAction(Profile* profile,
                                              const GURL& gurl,
                                              const std::u16string& title,
                                              int id) {
  const std::string url = gurl.spec();
  auto url_it = third_party_action_urls_.find(id);
  if (url_it == third_party_action_urls_.end())
    return;
  std::string url_found = url_it->second.spec();
  size_t location_shared_url = url_found.find(kUrlReplace);
  if (location_shared_url != std::string::npos) {
    std::string escaped_url = net::EscapeUrlEncodedData(url, false);
    url_found.replace(location_shared_url, strlen(kUrlReplace), escaped_url);
  } else {
    LOG(ERROR) << "Third Party Share API did not contain URL param.";
  }

  size_t location_title = url_found.find(kTitleReplace);
  if (location_title != std::string::npos) {
    std::string escaped_title =
        net::EscapeQueryParamValue(base::UTF16ToUTF8(title), false);
    url_found.replace(location_title, strlen(kTitleReplace), escaped_title);
  }

  // TODO (crbug.com/1229421) support descriptions in third party targets.

  GURL share_url = GURL(url_found);
  if (share_url.is_valid()) {
    NavigateParams params(profile, share_url, ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
    base::RecordAction(
        base::UserMetricsAction("SharingHubDesktop.ThirdPartyAppSelected"));
  } else {
    LOG(ERROR) << "Third Party Share URL was invalid.";
  }
}

void SharingHubModel::ExecuteThirdPartyAction(content::WebContents* contents,
                                              int id) {
  ExecuteThirdPartyAction(
      Profile::FromBrowserContext(contents->GetBrowserContext()),
      contents->GetLastCommittedURL(), contents->GetTitle(), id);
}

void SharingHubModel::PopulateFirstPartyActions() {
  first_party_action_list_.push_back(
      {IDC_COPY_URL, l10n_util::GetStringUTF16(IDS_SHARING_HUB_COPY_LINK_LABEL),
       &kCopyIcon, true, gfx::ImageSkia(),
       "SharingHubDesktop.CopyURLSelected"});

  if (DesktopScreenshotsFeatureEnabled(context_)) {
    first_party_action_list_.push_back(
        {IDC_SHARING_HUB_SCREENSHOT,
         l10n_util::GetStringUTF16(IDS_SHARING_HUB_SCREENSHOT_LABEL),
         &kSharingHubScreenshotIcon, true, gfx::ImageSkia(),
         "SharingHubDesktop.ScreenshotSelected"});
  }

  first_party_action_list_.push_back(
      {IDC_SEND_TAB_TO_SELF,
       l10n_util::GetStringUTF16(IDS_CONTEXT_MENU_SEND_TAB_TO_SELF),
       &kSendTabToSelfIcon, true, gfx::ImageSkia(),
       "SharingHubDesktop.SendTabToSelfSelected"});

  first_party_action_list_.push_back(
      {IDC_QRCODE_GENERATOR,
       l10n_util::GetStringUTF16(IDS_OMNIBOX_QRCODE_GENERATOR_ICON_LABEL),
       &kQrcodeGeneratorIcon, true, gfx::ImageSkia(),
       "SharingHubDesktop.QRCodeSelected"});

  if (media_router::MediaRouterEnabled(context_)) {
    first_party_action_list_.push_back(
        {IDC_ROUTE_MEDIA,
         l10n_util::GetStringUTF16(IDS_SHARING_HUB_MEDIA_ROUTER_LABEL),
         &vector_icons::kMediaRouterIdleIcon, true, gfx::ImageSkia(),
         "SharingHubDesktop.CastSelected"});
  }

  first_party_action_list_.push_back(
      {IDC_SAVE_PAGE,
       l10n_util::GetStringUTF16(IDS_SHARING_HUB_SAVE_PAGE_LABEL),
       &kSavePageIcon, true, gfx::ImageSkia(),
       "SharingHubDesktop.SavePageSelected"});
}

void SharingHubModel::PopulateThirdPartyActions() {
  // Clear the action list in the case where the action list is repopulated.
  if (third_party_action_list_.size()) {
    third_party_action_list_.clear();
  }
  // Note: The third party action id must be greater than 0, otherwise the
  // action will be disabled in the app menu.
  int id = 1;
  if (third_party_targets_) {
    for (const sharing::mojom::ShareTarget& target :
         third_party_targets_->targets()) {
      if (!target.icon().empty()) {
        gfx::Image icon = DecodeIcon(target.icon());
        gfx::ImageSkia icon_skia = icon.AsImageSkia();
        gfx::Image icon_2x = DecodeIcon(target.icon_2x());
        if (!icon_2x.IsEmpty()) {
          const SkBitmap* skBitmap_2x = icon_2x.ToSkBitmap();
          gfx::ImageSkiaRep rep_2x(*skBitmap_2x, 2.0);
          icon_skia.AddRepresentation(rep_2x);
        }

        gfx::Image icon_3x = DecodeIcon(target.icon_3x());
        if (!icon_3x.IsEmpty()) {
          const SkBitmap* skBitmap_3x = icon_3x.ToSkBitmap();
          gfx::ImageSkiaRep rep_3x(*skBitmap_3x, 3.0);
          icon_skia.AddRepresentation(rep_3x);
        }

        icon_skia.MakeThreadSafe();
        third_party_action_list_.push_back(
            {id, base::ASCIIToUTF16(target.nickname()),
             &vector_icons::kEmailIcon, false, std::move(icon_skia),
             "SharingHubDesktop.ThirdPartyAppSelected"});
      } else {
        third_party_action_list_.push_back(
            {id, base::ASCIIToUTF16(target.nickname()),
             &vector_icons::kEmailIcon, false, gfx::ImageSkia(),
             "SharingHubDesktop.ThirdPartyAppSelected"});
      }

      third_party_action_urls_[id] = GURL(target.url());
      id++;
    }
  }
}

bool SharingHubModel::DoShowSendTabToSelfForWebContents(
    content::WebContents* web_contents) {
  return send_tab_to_self::ShouldOfferFeature(web_contents);
}

void SharingHubModel::OnShareTargetsUpdated(
    std::unique_ptr<sharing::mojom::ShareTargets> share_target) {
  third_party_targets_ = std::move(share_target);
  PopulateThirdPartyActions();
}

}  // namespace sharing_hub
