// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/privacy_sandbox_handler.h"

#include "chrome/browser/federated_learning/floc_id_provider_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace settings {

namespace {

// Keys of the dictionary returned by GetFlocIdInformation.
constexpr char kTrialStatus[] = "trialStatus";
constexpr char kCohort[] = "cohort";
constexpr char kNextUpdate[] = "nextUpdate";
constexpr char kCanReset[] = "canReset";

base::Value GetFlocIdInformation(Profile* profile) {
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(profile);
  DCHECK(privacy_sandbox_service);

  base::DictionaryValue floc_id_information;
  floc_id_information.SetKey(
      kTrialStatus,
      base::Value(privacy_sandbox_service->GetFlocStatusForDisplay()));
  floc_id_information.SetKey(
      kCohort, base::Value(privacy_sandbox_service->GetFlocIdForDisplay()));
  floc_id_information.SetKey(
      kNextUpdate,
      base::Value(privacy_sandbox_service->GetFlocIdNextUpdateForDisplay(
          base::Time::Now())));
  floc_id_information.SetKey(
      kCanReset, base::Value(privacy_sandbox_service->IsFlocIdResettable()));

  return std::move(floc_id_information);
}

}  // namespace

void PrivacySandboxHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "getFlocId", base::BindRepeating(&PrivacySandboxHandler::HandleGetFlocId,
                                       base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "resetFlocId",
      base::BindRepeating(&PrivacySandboxHandler::HandleResetFlocId,
                          base::Unretained(this)));
}

void PrivacySandboxHandler::HandleGetFlocId(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetList().size());
  const base::Value& callback_id = args->GetList()[0];

  ResolveJavascriptCallback(callback_id,
                            GetFlocIdInformation(Profile::FromWebUI(web_ui())));
}

void PrivacySandboxHandler::HandleResetFlocId(const base::ListValue* args) {
  CHECK_EQ(0U, args->GetList().size());
  AllowJavascript();

  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  DCHECK(privacy_sandbox_service);

  privacy_sandbox_service->ResetFlocId(/*user_initiated=*/true);

  // The identifier will have been immediately invalidated in response to
  // the clearing action, so synchronously retrieving the FLoC ID will retrieve
  // the appropriate invalid ID string.
  // TODO(crbug.com/1207891): Have this handler listen to an event directly
  // from the FLoC provider, rather than inferring behavior.
  FireWebUIListener("floc-id-changed",
                    GetFlocIdInformation(Profile::FromWebUI(web_ui())));
}

}  // namespace settings
