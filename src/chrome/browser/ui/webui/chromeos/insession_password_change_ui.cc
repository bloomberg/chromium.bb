// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/insession_password_change_ui.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/insession_password_change_handler_chromeos.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos {

InSessionPasswordChangeUI::InSessionPasswordChangeUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  CHECK(profile->GetPrefs()->GetBoolean(
      prefs::kSamlInSessionPasswordChangeEnabled));

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPasswordChangeHost);

  web_ui->AddMessageHandler(std::make_unique<InSessionPasswordChangeHandler>());

  source->SetJsonPath("strings.js");

  source->SetDefaultResource(IDR_PASSWORD_CHANGE_HTML);

  source->AddResourcePath("password_change.css", IDR_PASSWORD_CHANGE_CSS);
  source->AddResourcePath("authenticator.js",
                          IDR_PASSWORD_CHANGE_AUTHENTICATOR_JS);
  source->AddResourcePath("password_change.js", IDR_PASSWORD_CHANGE_JS);

  content::WebUIDataSource::Add(profile, source);
}

InSessionPasswordChangeUI::~InSessionPasswordChangeUI() = default;

}  // namespace chromeos
