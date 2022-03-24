// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_name_handler.h"

#include <string>

#include "base/check.h"
#include "base/values.h"
#include "chrome/browser/ash/device_name/device_name_store.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {
namespace settings {

namespace {

// Key names for first and second key-value pairs in dictionary that is passed
// as argument to FireWebUIListener.
const char kMetadataFirstKey[] = "deviceName";
const char kMetadataSecondKey[] = "deviceNameState";

}  // namespace

DeviceNameHandler::DeviceNameHandler()
    : DeviceNameHandler(DeviceNameStore::GetInstance()) {}

DeviceNameHandler::DeviceNameHandler(DeviceNameStore* device_name_store)
    : device_name_store_(device_name_store) {}

DeviceNameHandler::~DeviceNameHandler() = default;

void DeviceNameHandler::OnJavascriptAllowed() {
  observation_.Observe(device_name_store_);
}

void DeviceNameHandler::OnJavascriptDisallowed() {
  observation_.Reset();
}

void DeviceNameHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "notifyReadyForDeviceName",
      base::BindRepeating(&DeviceNameHandler::HandleNotifyReadyForDeviceName,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "attemptSetDeviceName",
      base::BindRepeating(&DeviceNameHandler::HandleAttemptSetDeviceName,
                          base::Unretained(this)));
}

base::Value DeviceNameHandler::GetDeviceNameMetadata() const {
  base::Value metadata(base::Value::Type::DICTIONARY);
  DeviceNameStore::DeviceNameMetadata device_name_metadata =
      device_name_store_->GetDeviceNameMetadata();
  metadata.SetStringKey(kMetadataFirstKey, device_name_metadata.device_name);
  metadata.SetIntKey(kMetadataSecondKey,
                     static_cast<int>(device_name_metadata.device_name_state));
  return metadata;
}

void DeviceNameHandler::HandleAttemptSetDeviceName(
    const base::ListValue* args) {
  AllowJavascript();

  DCHECK_EQ(2U, args->GetList().size());
  const base::Value::ConstListView args_list = args->GetList();
  const std::string callback_id = args_list[0].GetString();
  const std::string name_from_user = args_list[1].GetString();
  DeviceNameStore::SetDeviceNameResult result =
      device_name_store_->SetDeviceName(name_from_user);

  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(static_cast<int>(result)));
}

void DeviceNameHandler::HandleNotifyReadyForDeviceName(
    const base::ListValue* args) {
  AllowJavascript();
  FireWebUIListener("settings.updateDeviceNameMetadata",
                    base::Value(GetDeviceNameMetadata()));
}

void DeviceNameHandler::OnDeviceNameMetadataChanged() {
  FireWebUIListener("settings.updateDeviceNameMetadata",
                    base::Value(GetDeviceNameMetadata()));
}

}  // namespace settings
}  // namespace chromeos
