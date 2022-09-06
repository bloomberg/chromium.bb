// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/eula_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"
#include "rlz/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace chromeos {

constexpr StaticOobeScreenId EulaView::kScreenId;

EulaScreenHandler::EulaScreenHandler() : BaseScreenHandler(kScreenId) {
  set_user_acted_method_path_deprecated("login.EulaScreen.userActed");
}

EulaScreenHandler::~EulaScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void EulaScreenHandler::Show() {
  if (!IsJavascriptAllowed()) {
    show_on_init_ = true;
    return;
  }
  // TODO(https://crbug.com/1309022): pass variables below directly to
  //                                  EulaScreenHandler::Show once show_on_init_
  //                                  is gone.
  base::Value::Dict data;
  data.Set("backButtonHidden", back_button_hidden_);
  data.Set("securitySettingsShown", security_settings_hidden_);
  ShowInWebUI(std::move(data));
}

void EulaScreenHandler::Hide() {
}

void EulaScreenHandler::Bind(EulaScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreenDeprecated(screen_);
  if (IsJavascriptAllowed())
    InitializeDeprecated();
}

void EulaScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreenDeprecated(nullptr);
}

std::string EulaScreenHandler::GetEulaOnlineUrl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeEulaUrlForTests)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kOobeEulaUrlForTests);
  }

  return base::StringPrintf(chrome::kGoogleEulaOnlineURLPath,
                            g_browser_process->GetApplicationLocale().c_str());
}

std::string EulaScreenHandler::GetAdditionalToSUrl() {
  return base::StringPrintf(chrome::kCrosEulaOnlineURLPath,
                            g_browser_process->GetApplicationLocale().c_str());
}

void EulaScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("eulaScreenAccessibleTitle", IDS_EULA_SCREEN_ACCESSIBLE_TITLE);
  builder->Add("checkboxLogging", IDS_EULA_CHECKBOX_ENABLE_LOGGING);
  builder->Add("acceptAgreement", IDS_EULA_ACCEPT_AND_CONTINUE_BUTTON);
  builder->Add("eulaSystemSecuritySettings", IDS_EULA_SYSTEM_SECURITY_SETTING);

  ::login::GetSecureModuleUsed(base::BindOnce(&EulaScreenHandler::UpdateTpmDesc,
                                              weak_factory_.GetWeakPtr()));

  builder->Add("eulaSystemSecuritySettingsOkButton", IDS_OK);
  builder->Add("termsOfServiceLoading", IDS_TERMS_OF_SERVICE_SCREEN_LOADING);
#if BUILDFLAG(ENABLE_RLZ)
  builder->AddF("eulaRlzDesc",
                IDS_EULA_RLZ_DESCRIPTION,
                IDS_SHORT_PRODUCT_NAME,
                IDS_PRODUCT_NAME);
  builder->AddF("eulaRlzEnable",
                IDS_EULA_RLZ_ENABLE,
                IDS_SHORT_PRODUCT_OS_NAME);
#endif

  // Online URL to use. May be overridden by tests.
  builder->Add("eulaOnlineUrl", GetEulaOnlineUrl());
  builder->Add("eulaAdditionalToSOnlineUrl", GetAdditionalToSUrl());

  /* MD-OOBE */
  builder->Add("oobeEulaSectionTitle", IDS_OOBE_EULA_SECTION_TITLE);
  builder->Add("oobeEulaAditionalTerms", IDS_OOBE_EULA_ADDITIONAL_TERMS);
  builder->Add("oobeEulaIframeLabel", IDS_OOBE_EULA_IFRAME_LABEL);
  builder->Add("oobeEulaAcceptAndContinueButtonText",
               IDS_OOBE_EULA_ACCEPT_AND_CONTINUE_BUTTON_TEXT);
}

void EulaScreenHandler::GetAdditionalParameters(base::Value::Dict* dict) {
#if BUILDFLAG(ENABLE_RLZ)
  dict->Set("rlzEnabled", "enabled");
#else
  dict->Set("rlzEnabled", "disabled");
#endif
}

void EulaScreenHandler::InitializeDeprecated() {
  if (!IsJavascriptAllowed() || !screen_)
    return;

  CallJS("login.EulaScreen.setUsageStats", screen_->IsUsageStatsEnabled());

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void EulaScreenHandler::ShowStatsUsageLearnMore() {
  if (!help_app_.get())
    help_app_ = new HelpAppLauncher(
        LoginDisplayHost::default_host()->GetNativeWindow());
  help_app_->ShowHelpTopic(HelpAppLauncher::HELP_STATS_USAGE);
}

void EulaScreenHandler::ShowAdditionalTosDialog() {
  CallJS("login.EulaScreen.showAdditionalTosDialog");
}

void EulaScreenHandler::ShowSecuritySettingsDialog() {
  CallJS("login.EulaScreen.showSecuritySettingsDialog");
}

void EulaScreenHandler::HideSecuritySettingsInfo() {
  security_settings_hidden_ = true;
}

void EulaScreenHandler::HideBackButton() {
  back_button_hidden_ = true;
}

void EulaScreenHandler::UpdateTpmDesc(
    ::login::SecureModuleUsed secure_module_used) {
  const std::u16string tpm_desc =
      secure_module_used == ::login::SecureModuleUsed::TPM
          ? l10n_util::GetStringUTF16(IDS_EULA_TPM_DESCRIPTION)
          : l10n_util::GetStringUTF16(IDS_EULA_SECURE_MODULE_DESCRIPTION);
  CallJS("login.EulaScreen.setTpmDesc", tpm_desc);
}

}  // namespace chromeos
