// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/cookie_controls_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"

namespace {
static const char* kPolicyIcon = "cr20:domain";
static const char* kExtensionIcon = "cr:extension";
static const char* kSettingsIcon = "cr:settings_icon";
}  // namespace

CookieControlsHandler::CookieControlsHandler(Profile* profile)
    : service_(CookieControlsServiceFactory::GetForProfile(profile)) {}

CookieControlsHandler::~CookieControlsHandler() {
  service_->RemoveObserver(this);
}

void CookieControlsHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "cookieControlsToggleChanged",
      base::BindRepeating(
          &CookieControlsHandler::HandleCookieControlsToggleChanged,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "observeCookieControlsSettingsChanges",
      base::BindRepeating(
          &CookieControlsHandler::HandleObserveCookieControlsSettingsChanges,
          base::Unretained(this)));
}

void CookieControlsHandler::OnJavascriptAllowed() {
  service_->AddObserver(this);
}

void CookieControlsHandler::OnJavascriptDisallowed() {
  service_->RemoveObserver(this);
}

void CookieControlsHandler::HandleCookieControlsToggleChanged(
    const base::ListValue* args) {
  const auto& list = args->GetList();
  CHECK(!list.empty());
  const bool checked = list[0].GetBool();
  service_->HandleCookieControlsToggleChanged(checked);
}

void CookieControlsHandler::HandleObserveCookieControlsSettingsChanges(
    const base::ListValue* args) {
  AllowJavascript();
  SendCookieControlsUIChanges();
}

const char* CookieControlsHandler::GetEnforcementIcon(
    CookieControlsEnforcement enforcement) {
  switch (enforcement) {
    case CookieControlsEnforcement::kEnforcedByPolicy:
      return kPolicyIcon;
    case CookieControlsEnforcement::kEnforcedByExtension:
      return kExtensionIcon;
    case CookieControlsEnforcement::kEnforcedByCookieSetting:
      return kSettingsIcon;
    case CookieControlsEnforcement::kNoEnforcement:
      return "";
  }
}

void CookieControlsHandler::OnThirdPartyCookieBlockingPrefChanged() {
  SendCookieControlsUIChanges();
}

void CookieControlsHandler::OnThirdPartyCookieBlockingPolicyChanged() {
  SendCookieControlsUIChanges();
}

void CookieControlsHandler::SendCookieControlsUIChanges() {
  base::DictionaryValue dict;
  dict.SetBoolKey("enforced", service_->ShouldEnforceCookieControls());
  dict.SetBoolKey("checked", service_->GetToggleCheckedValue());
  dict.SetStringKey(
      "icon", GetEnforcementIcon(service_->GetCookieControlsEnforcement()));
  dict.SetString("cookieSettingsUrl", chrome::kChromeUICookieSettingsURL);
  FireWebUIListener("cookie-controls-changed", dict);
}
