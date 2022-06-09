// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_network_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_network_handler.h"
#include "chrome/browser/ui/webui/chromeos/internet_config_dialog.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/strings/network_element_localized_strings_provider.h"

namespace chromeos {

// static
void LockScreenNetworkUI::GetLocalizedStrings(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString(
      "titleText", l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_NETWORK_TITLE));
  localized_strings->SetString(
      "lockScreenNetworkTitle",
      l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_NETWORK_TITLE));
  localized_strings->SetString(
      "lockScreenNetworkSubtitle",
      l10n_util::GetStringFUTF16(IDS_LOCK_SCREEN_NETWORK_SUBTITLE,
                                 ui::GetChromeOSDeviceName()));
  localized_strings->SetString(
      "lockScreenCancelButton",
      l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_CANCEL_BUTTON));
}

LockScreenNetworkUI::LockScreenNetworkUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  auto main_handler = std::make_unique<NetworkConfigMessageHandler>();
  main_handler_ = main_handler.get();
  web_ui->AddMessageHandler(std::move(main_handler));

  base::DictionaryValue localized_strings;
  GetLocalizedStrings(&localized_strings);

  content::WebUIDataSource* html =
      content::WebUIDataSource::Create(chrome::kChromeUILockScreenNetworkHost);

  // TODO(crbug.com/1098690): Trusted Type Polymer.
  html->DisableTrustedTypesCSP();

  html->AddLocalizedStrings(localized_strings);

  ui::network_element::AddLocalizedStrings(html);
  ui::network_element::AddOncLocalizedStrings(html);
  html->UseStringsJs();

  html->AddResourcePath("lock_screen_network.js", IDR_LOCK_SCREEN_NETWORK_JS);
  html->SetDefaultResource(IDR_LOCK_SCREEN_NETWORK_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html);
}

LockScreenNetworkUI::~LockScreenNetworkUI() {}

void LockScreenNetworkUI::BindInterface(
    mojo::PendingReceiver<network_config::mojom::CrosNetworkConfig> receiver) {
  ash::GetNetworkConfigService(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(LockScreenNetworkUI)

}  // namespace chromeos
