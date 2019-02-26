// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux/email_handler.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/welcome/nux/bookmark_item.h"
#include "chrome/browser/ui/webui/welcome/nux/email_providers_list.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/onboarding_welcome_resources.h"
#include "components/country_codes/country_codes.h"
#include "components/favicon/core/favicon_service.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"

namespace nux {

const char* kEmailInteractionHistogram =
    "FirstRun.NewUserExperience.EmailInteraction";

constexpr const int kEmailIconSize = 48;  // Pixels.

EmailHandler::EmailHandler()
    : email_providers_(GetCurrentCountryEmailProviders()) {}

EmailHandler::~EmailHandler() {}

void EmailHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "cacheEmailIcon", base::BindRepeating(&EmailHandler::HandleCacheEmailIcon,
                                            base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getEmailList", base::BindRepeating(&EmailHandler::HandleGetEmailList,
                                          base::Unretained(this)));
}

void EmailHandler::HandleCacheEmailIcon(const base::ListValue* args) {
  int emailId;
  args->GetInteger(0, &emailId);

  const BookmarkItem* selectedEmail = NULL;
  for (const auto& provider : email_providers_) {
    if (provider.id == emailId) {
      selectedEmail = &provider;
      break;
    }
  }
  CHECK(selectedEmail);  // WebUI should not be able to pass non-existent ID.

  // Preload the favicon cache with Chrome-bundled images. Otherwise, the
  // pre-populated bookmarks don't have favicons and look bad. Favicons are
  // updated automatically when a user visits a site.
  GURL app_url = GURL(selectedEmail->url);
  FaviconServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()),
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->MergeFavicon(
          app_url, app_url, favicon_base::IconType::kFavicon,
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              selectedEmail->icon),
          gfx::Size(kEmailIconSize, kEmailIconSize));
}

void EmailHandler::HandleGetEmailList(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  ResolveJavascriptCallback(*callback_id,
                            bookmarkItemsToListValue(email_providers_));
}

void EmailHandler::AddSources(content::WebUIDataSource* html_source) {
  // Add constants to loadtime data
  html_source->AddInteger("email_providers_enum_count", EmailProviders::kCount);
  html_source->SetJsonPath("strings.js");
}

}  // namespace nux
