// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux/email_handler.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/welcome/nux/show_promo_delegate.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/favicon/core/favicon_service.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"

namespace nux {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EmailProviders {
  kGmail = 0,
  kYahoo = 1,
  kOutlook = 2,
  kAol = 3,
  kiCloud = 4,
  kCount,
};

struct EmailProviderType {
  const EmailProviders id;
  const char* name;  // Icon in WebUI should use name to match CSS.
  const char* url;
  const int icon;  // Corresponds with resource ID, used for bookmark cache.
};

const char* kEmailInteractionHistogram =
    "FirstRun.NewUserExperience.EmailInteraction";

// Strings in costants not translated because this is an experiment.
// Translate before wide release.
// TODO(hcarmona): populate with icon ids.
const EmailProviderType kEmail[] = {
    {EmailProviders::kGmail, "Gmail",
     "https://accounts.google.com/b/0/AddMailService", IDR_NUX_EMAIL_GMAIL_1X},
    {EmailProviders::kYahoo, "Yahoo", "https://mail.yahoo.com",
     IDR_NUX_EMAIL_YAHOO_1X},
    {EmailProviders::kOutlook, "Outlook", "https://login.live.com/login.srf?",
     IDR_NUX_EMAIL_OUTLOOK_1X},
    {EmailProviders::kAol, "AOL", "https://mail.aol.com", IDR_NUX_EMAIL_AOL_1X},
    {EmailProviders::kiCloud, "iCloud", "https://www.icloud.com/mail",
     IDR_NUX_EMAIL_ICLOUD_1X},
};

constexpr const int kEmailIconSize = 48;  // Pixels.

static_assert(base::size(kEmail) == (size_t)EmailProviders::kCount,
              "names and histograms must match");

EmailHandler::EmailHandler(PrefService* prefs,
                           favicon::FaviconService* favicon_service)
    : prefs_(prefs), favicon_service_(favicon_service) {}

EmailHandler::~EmailHandler() {}

void EmailHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "cacheEmailIcon", base::BindRepeating(&EmailHandler::HandleCacheEmailIcon,
                                            base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "toggleBookmarkBar",
      base::BindRepeating(&EmailHandler::HandleToggleBookmarkBar,
                          base::Unretained(this)));
}

void EmailHandler::HandleCacheEmailIcon(const base::ListValue* args) {
  int emailId;
  args->GetInteger(0, &emailId);

  const EmailProviderType* selectedEmail = NULL;
  for (size_t i = 0; i < base::size(kEmail); i++) {
    if ((int)kEmail[i].id == emailId) {
      selectedEmail = &kEmail[i];
      break;
    }
  }
  CHECK(selectedEmail);  // WebUI should not be able to pass non-existent ID.

  // Preload the favicon cache with Chrome-bundled images. Otherwise, the
  // pre-populated bookmarks don't have favicons and look bad. Favicons are
  // updated automatically when a user visits a site.
  GURL app_url = GURL(selectedEmail->url);
  favicon_service_->MergeFavicon(
      app_url, app_url, favicon_base::IconType::kFavicon,
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
          selectedEmail->icon),
      gfx::Size(kEmailIconSize, kEmailIconSize));
}

void EmailHandler::HandleToggleBookmarkBar(const base::ListValue* args) {
  bool show = false;
  CHECK(args->GetBoolean(0, &show));
  prefs_->SetBoolean(bookmarks::prefs::kShowBookmarkBar, show);
}

void EmailHandler::AddSources(content::WebUIDataSource* html_source,
                              PrefService* prefs) {
  // Localized strings.
  html_source->AddLocalizedString("noThanks", IDS_NO_THANKS);
  html_source->AddLocalizedString("getStarted", IDS_NUX_EMAIL_GET_STARTED);
  html_source->AddLocalizedString("welcomeTitle", IDS_NUX_EMAIL_WELCOME_TITLE);
  html_source->AddLocalizedString("emailPrompt", IDS_NUX_EMAIL_PROMPT);
  html_source->AddLocalizedString("bookmarkAdded",
                                  IDS_NUX_EMAIL_BOOKMARK_ADDED);
  html_source->AddLocalizedString("bookmarkRemoved",
                                  IDS_NUX_EMAIL_BOOKMARK_REMOVED);
  html_source->AddLocalizedString("bookmarkReplaced",
                                  IDS_NUX_EMAIL_BOOKMARK_REPLACED);

  // Add required resources.
  html_source->AddResourcePath("email", IDR_NUX_EMAIL_HTML);
  html_source->AddResourcePath("email/nux_email.js", IDR_NUX_EMAIL_JS);

  html_source->AddResourcePath("email/nux_email_proxy.html",
                               IDR_NUX_EMAIL_PROXY_HTML);
  html_source->AddResourcePath("email/nux_email_proxy.js",
                               IDR_NUX_EMAIL_PROXY_JS);

  html_source->AddResourcePath("email/email_chooser.html",
                               IDR_NUX_EMAIL_CHOOSER_HTML);
  html_source->AddResourcePath("email/email_chooser.js",
                               IDR_NUX_EMAIL_CHOOSER_JS);

  // Add icons
  html_source->AddResourcePath("email/aol_1x.png", IDR_NUX_EMAIL_AOL_1X);
  html_source->AddResourcePath("email/aol_2x.png", IDR_NUX_EMAIL_AOL_2X);
  html_source->AddResourcePath("email/gmail_1x.png", IDR_NUX_EMAIL_GMAIL_1X);
  html_source->AddResourcePath("email/gmail_2x.png", IDR_NUX_EMAIL_GMAIL_2X);
  html_source->AddResourcePath("email/icloud_1x.png", IDR_NUX_EMAIL_ICLOUD_1X);
  html_source->AddResourcePath("email/icloud_2x.png", IDR_NUX_EMAIL_ICLOUD_2X);
  html_source->AddResourcePath("email/outlook_1x.png",
                               IDR_NUX_EMAIL_OUTLOOK_1X);
  html_source->AddResourcePath("email/outlook_2x.png",
                               IDR_NUX_EMAIL_OUTLOOK_2X);
  html_source->AddResourcePath("email/yahoo_1x.png", IDR_NUX_EMAIL_YAHOO_1X);
  html_source->AddResourcePath("email/yahoo_2x.png", IDR_NUX_EMAIL_YAHOO_2X);

  // Add constants to loadtime data
  for (size_t i = 0; i < (size_t)EmailProviders::kCount; ++i) {
    html_source->AddInteger("email_id_" + std::to_string(i), (int)kEmail[i].id);
    html_source->AddString("email_name_" + std::to_string(i), kEmail[i].name);
    html_source->AddString("email_url_" + std::to_string(i), kEmail[i].url);
  }
  html_source->AddInteger("email_count", (int)EmailProviders::kCount);
  html_source->AddBoolean(
      "bookmark_bar_shown",
      prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar));
  html_source->SetJsonPath("strings.js");
}

}  // namespace nux