// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/account_manager_welcome_ui.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos {

AccountManagerWelcomeUI::AccountManagerWelcomeUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui), weak_factory_(this) {
  content::WebUIDataSource* html_source = content::WebUIDataSource::Create(
      chrome::kChromeUIAccountManagerWelcomeHost);

  web_ui->RegisterMessageCallback(
      "closeDialog", base::BindRepeating(&WebDialogUI::CloseDialog,
                                         weak_factory_.GetWeakPtr()));

  html_source->SetJsonPath("strings.js");

  // Add localized strings.
  html_source->AddLocalizedString("welcomeTitle",
                                  IDS_ACCOUNT_MANAGER_WELCOME_TITLE);
  html_source->AddLocalizedString("welcomeMessage",
                                  IDS_ACCOUNT_MANAGER_WELCOME_TEXT);
  html_source->AddLocalizedString("okButton", IDS_APP_OK);

  // Add required resources.
  html_source->AddResourcePath("account_manager_welcome.css",
                               IDR_ACCOUNT_MANAGER_WELCOME_CSS);
  html_source->AddResourcePath("account_manager_welcome.js",
                               IDR_ACCOUNT_MANAGER_WELCOME_JS);
  html_source->AddResourcePath("account_manager_welcome_1x.png",
                               IDR_ACCOUNT_MANAGER_WELCOME_1X_PNG);
  html_source->AddResourcePath("account_manager_welcome_2x.png",
                               IDR_ACCOUNT_MANAGER_WELCOME_2X_PNG);
  html_source->AddResourcePath("googleg.svg",
                               IDR_ACCOUNT_MANAGER_WELCOME_GOOGLE_LOGO_SVG);
  html_source->SetDefaultResource(IDR_ACCOUNT_MANAGER_WELCOME_HTML);
  html_source->UseGzip();

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}

AccountManagerWelcomeUI::~AccountManagerWelcomeUI() = default;

}  // namespace chromeos
