// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_handler.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/user_image_source.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "components/user_manager/user.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {

namespace multidevice_setup {

MultideviceSetupHandler::MultideviceSetupHandler() = default;

MultideviceSetupHandler::~MultideviceSetupHandler() = default;

void MultideviceSetupHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "getProfileInfo",
      base::BindRepeating(&MultideviceSetupHandler::HandleGetProfileInfo,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "openMultiDeviceSettings",
      base::BindRepeating(
          &MultideviceSetupHandler::HandleOpenMultiDeviceSettings,
          base::Unretained(this)));
}

void MultideviceSetupHandler::HandleGetProfileInfo(
    const base::ListValue* args) {
  AllowJavascript();

  DCHECK(!args->GetList().empty());
  std::string callback_id = args->GetList()[0].GetString();

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          Profile::FromWebUI(web_ui()));

  base::DictionaryValue response;
  response.SetString("email", user->GetDisplayEmail());

  scoped_refptr<base::RefCountedMemory> image =
      chromeos::UserImageSource::GetUserImage(user->GetAccountId());
  response.SetString("profilePhotoUrl",
                     webui::GetPngDataUrl(image->front(), image->size()));

  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void MultideviceSetupHandler::HandleOpenMultiDeviceSettings(
    const base::ListValue* args) {
  DCHECK(args->GetList().empty());
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      Profile::FromWebUI(web_ui()),
      chromeos::settings::mojom::kMultiDeviceFeaturesSubpagePath);
}

}  // namespace multidevice_setup

}  // namespace chromeos
