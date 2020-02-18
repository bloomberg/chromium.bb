// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_email_confirmation_ui.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

SigninEmailConfirmationUI::SigninEmailConfirmationUI(content::WebUI* web_ui)
    : ConstrainedWebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUISigninEmailConfirmationHost);
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  source->SetDefaultResource(IDR_SIGNIN_EMAIL_CONFIRMATION_HTML);
  source->AddResourcePath("signin_email_confirmation_app.js",
                          IDR_SIGNIN_EMAIL_CONFIRMATION_APP_JS);
  source->AddResourcePath("signin_shared_css.js", IDR_SIGNIN_SHARED_CSS_JS);

  static constexpr webui::LocalizedString kStrings[] = {
      {"signinEmailConfirmationTitle", IDS_SIGNIN_EMAIL_CONFIRMATION_TITLE},
      {"signinEmailConfirmationCreateProfileButtonTitle",
       IDS_SIGNIN_EMAIL_CONFIRMATION_CREATE_PROFILE_RADIO_BUTTON_TITLE},
      {"signinEmailConfirmationCreateProfileButtonSubtitle",
       IDS_SIGNIN_EMAIL_CONFIRMATION_CREATE_PROFILE_RADIO_BUTTON_SUBTITLE},
      {"signinEmailConfirmationStartSyncButtonTitle",
       IDS_SIGNIN_EMAIL_CONFIRMATION_START_SYNC_RADIO_BUTTON_TITLE},
      {"signinEmailConfirmationStartSyncButtonSubtitle",
       IDS_SIGNIN_EMAIL_CONFIRMATION_START_SYNC_RADIO_BUTTON_SUBTITLE},
      {"signinEmailConfirmationConfirmLabel",
       IDS_SIGNIN_EMAIL_CONFIRMATION_CONFIRM_BUTTON_LABEL},
      {"signinEmailConfirmationCloseLabel",
       IDS_SIGNIN_EMAIL_CONFIRMATION_CLOSE_BUTTON_LABEL},
  };
  AddLocalizedStringsBulk(source, kStrings);

  base::DictionaryValue strings;
  webui::SetLoadTimeDataDefaults(g_browser_process->GetApplicationLocale(),
                                 &strings);
  source->AddLocalizedStrings(strings);

  content::WebUIDataSource::Add(profile, source);
}

SigninEmailConfirmationUI::~SigninEmailConfirmationUI() {}

void SigninEmailConfirmationUI::Close() {
  ConstrainedWebDialogDelegate* delegate = GetConstrainedDelegate();
  if (delegate) {
    delegate->GetWebDialogDelegate()->OnDialogClosed(std::string());
    delegate->OnDialogCloseFromWebUI();
  }
}
