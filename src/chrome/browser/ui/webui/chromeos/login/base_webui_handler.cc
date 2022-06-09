// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"

#include <memory>

#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "components/login/localized_values_builder.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

BaseWebUIHandler::BaseWebUIHandler(JSCallsContainer* js_calls_container)
    : js_calls_container_(js_calls_container) {}

BaseWebUIHandler::~BaseWebUIHandler() = default;

void BaseWebUIHandler::InitializeBase() {
  page_is_ready_ = true;
  AllowJavascript();
  Initialize();
}

void BaseWebUIHandler::GetLocalizedStrings(base::DictionaryValue* dict) {
  auto builder = std::make_unique<::login::LocalizedValuesBuilder>(dict);
  DeclareLocalizedValues(builder.get());
  GetAdditionalParameters(dict);
}

void BaseWebUIHandler::RegisterMessages() {
  DeclareJSCallbacks();
}

void BaseWebUIHandler::GetAdditionalParameters(base::DictionaryValue* dict) {}

void BaseWebUIHandler::ShowScreen(OobeScreenId screen) {
  ShowScreenWithData(screen, nullptr);
}

void BaseWebUIHandler::ShowScreenWithData(OobeScreenId screen,
                                          const base::DictionaryValue* data) {
  base::DictionaryValue screen_params;
  screen_params.SetString("id", screen.name);
  if (data) {
    screen_params.SetKey("data", data->Clone());
  }
  CallJS("cr.ui.Oobe.showScreen", screen_params);
}

OobeUI* BaseWebUIHandler::GetOobeUI() {
  return static_cast<OobeUI*>(web_ui()->GetController());
}

OobeScreenId BaseWebUIHandler::GetCurrentScreen() {
  OobeUI* oobe_ui = GetOobeUI();
  if (!oobe_ui)
    return OobeScreen::SCREEN_UNKNOWN;
  return oobe_ui->current_screen();
}

void BaseWebUIHandler::OnJavascriptDisallowed() {
  javascript_disallowed_ = true;
}

void BaseWebUIHandler::InsertIntoList(std::vector<base::Value>*) {}

void BaseWebUIHandler::MaybeRecordIncomingEvent(
    const std::string& function_name,
    const base::ListValue* args) {
  if (js_calls_container_->record_all_events_for_test()) {
    // Do a clone so `args` is still available for the actual handler.
    std::vector<base::Value> arguments = args->Clone().TakeList();
    js_calls_container_->events()->emplace_back(
        JSCallsContainer::Event(JSCallsContainer::Event::Type::kIncoming,
                                function_name, std::move(arguments)));
  }
}

void BaseWebUIHandler::OnRawCallback(
    const std::string& function_name,
    const content::WebUI::DeprecatedMessageCallback& callback,
    const base::ListValue* args) {
  MaybeRecordIncomingEvent(function_name, args);
  callback.Run(args);
}

}  // namespace chromeos
