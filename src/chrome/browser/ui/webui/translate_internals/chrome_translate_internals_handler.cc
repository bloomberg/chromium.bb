// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/translate_internals/chrome_translate_internals_handler.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_error_details.h"
#include "components/translate/core/browser/translate_event_details.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

ChromeTranslateInternalsHandler::ChromeTranslateInternalsHandler() {
  detection_subscription_ =
      translate::TranslateManager::RegisterLanguageDetectedCallback(
          base::BindRepeating(
              &ChromeTranslateInternalsHandler::LanguageDetected,
              base::Unretained(this)));
}

ChromeTranslateInternalsHandler::~ChromeTranslateInternalsHandler() {}

translate::TranslateClient*
ChromeTranslateInternalsHandler::GetTranslateClient() {
  return ChromeTranslateClient::FromWebContents(web_ui()->GetWebContents());
}

variations::VariationsService*
ChromeTranslateInternalsHandler::GetVariationsService() {
  return g_browser_process->variations_service();
}

void ChromeTranslateInternalsHandler::RegisterMessageCallback(
    const std::string& message,
    MessageCallback callback) {
  web_ui()->RegisterMessageCallback(message, std::move(callback));
}

void ChromeTranslateInternalsHandler::RegisterDeprecatedMessageCallback(
    const std::string& message,
    const DeprecatedMessageCallback& callback) {
  web_ui()->RegisterDeprecatedMessageCallback(message, callback);
}

void ChromeTranslateInternalsHandler::CallJavascriptFunction(
    const std::string& function_name,
    const std::vector<const base::Value*>& args) {
  web_ui()->CallJavascriptFunctionUnsafe(function_name, args);
}

void ChromeTranslateInternalsHandler::RegisterMessages() {
  RegisterMessageCallbacks();
}

void ChromeTranslateInternalsHandler::LanguageDetected(
    const translate::LanguageDetectionDetails& details) {
  AddLanguageDetectionDetails(details);
}
