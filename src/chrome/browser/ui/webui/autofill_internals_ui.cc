// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/autofill_internals_ui.h"

#include <string>

#include "chrome/browser/autofill/autofill_internals_logging_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/autofill/core/browser/autofill_internals_logging.h"
#include "components/grit/components_resources.h"
#include "content/public/browser/web_ui_data_source.h"

#include "base/bind.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"

namespace {

content::WebUIDataSource* CreateAutofillInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIAutofillInternalsHost);
  source->AddResourcePath("autofill_and_password_manager_internals.js",
                          IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_JS);
  source->AddResourcePath("autofill_and_password_manager_internals.css",
                          IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_CSS);
  source->SetDefaultResource(IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_HTML);
  return source;
}

}  // namespace

AutofillInternalsUI::AutofillInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui), WebContentsObserver(web_ui->GetWebContents()) {
  // Set up the chrome://autofill-internals source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateAutofillInternalsHTMLSource());

  autofill::AutofillInternalsLoggingImpl* internals_logging_impl =
      new autofill::AutofillInternalsLoggingImpl();
  internals_logging_impl->set_web_ui(web_ui);
  autofill::AutofillInternalsLogging* internals_logging =
      static_cast<autofill::AutofillInternalsLogging*>(internals_logging_impl);
  autofill::AutofillInternalsLogging::SetAutofillInternalsLogger(
      std::unique_ptr<autofill::AutofillInternalsLogging>(internals_logging));
}

void AutofillInternalsUI::DidStartLoading() {}

void AutofillInternalsUI::DidStopLoading() {
  web_ui()->CallJavascriptFunctionUnsafe("setUpAutofillInternals");
  web_ui()->CallJavascriptFunctionUnsafe(
      "notifyAboutIncognito",
      base::Value(Profile::FromWebUI(web_ui())->IsIncognitoProfile()));
}
