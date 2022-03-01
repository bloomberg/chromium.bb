// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/translate_internals/translate_internals_ui.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/translate_internals/chrome_translate_internals_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/translate_internals/translate_internals_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

content::WebUIDataSource* CreateTranslateInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUITranslateInternalsHost);

  source->SetDefaultResource(IDR_TRANSLATE_INTERNALS_HTML);
  source->UseStringsJs();
  source->AddResourcePath("translate_internals.css",
                          IDR_TRANSLATE_INTERNALS_CSS);
  source->AddResourcePath("translate_internals.js", IDR_TRANSLATE_INTERNALS_JS);

  base::Value langs = translate::TranslateInternalsHandler::GetLanguages();
  for (const auto key_value_pair : langs.DictItems()) {
    DCHECK(key_value_pair.second.is_string());
    std::string key = "language-" + key_value_pair.first;
    const std::string& value = key_value_pair.second.GetString();
    source->AddString(key, value);
  }

  if (translate::IsTFLiteLanguageDetectionEnabled()) {
    source->AddString("model-version", "TFLite_v1");
    return source;
  }
  // The default language detection model is "CLD3".
  source->AddString("model-version", "CLD3");

  return source;
}

}  // namespace

TranslateInternalsUI::TranslateInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<ChromeTranslateInternalsHandler>());

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateTranslateInternalsHTMLSource());
}
