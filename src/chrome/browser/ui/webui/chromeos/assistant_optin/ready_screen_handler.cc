// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/assistant_optin/ready_screen_handler.h"

#include "chrome/browser/browser_process.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace {

constexpr char kJsScreenPath[] = "assistant.ReadyScreen";

constexpr char kUserActionNextPressed[] = "next-pressed";

}  // namespace

namespace chromeos {

ReadyScreenHandler::ReadyScreenHandler(
    OnAssistantOptInScreenExitCallback callback)
    : BaseWebUIHandler(), exit_callback_(std::move(callback)) {
  set_call_js_prefix(kJsScreenPath);
}

ReadyScreenHandler::~ReadyScreenHandler() = default;

void ReadyScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("assistantReadyTitle", IDS_ASSISTANT_READY_SCREEN_TITLE);
  builder->Add("assistantReadyMessage", IDS_ASSISTANT_READY_SCREEN_MESSAGE);
  builder->Add("assistantReadyButton", IDS_ASSISTANT_DONE_BUTTON);
}

void ReadyScreenHandler::RegisterMessages() {
  AddPrefixedCallback("userActed", &ReadyScreenHandler::HandleUserAction);
  AddPrefixedCallback("screenShown", &ReadyScreenHandler::HandleScreenShown);
}

void ReadyScreenHandler::Initialize() {}

void ReadyScreenHandler::HandleUserAction(const std::string& action) {
  DCHECK(exit_callback_);
  if (action == kUserActionNextPressed) {
    RecordAssistantOptInStatus(READY_SCREEN_CONTINUED);
    std::move(exit_callback_)
        .Run(AssistantOptInScreenExitCode::READY_SCREEN_CONTINUED);
  }
}

void ReadyScreenHandler::HandleScreenShown() {
  RecordAssistantOptInStatus(READY_SCREEN_SHOWN);
}

}  // namespace chromeos
