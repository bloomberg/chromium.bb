// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oobe_configuration.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/oobe_configuration_client.h"

namespace chromeos {

// static
OobeConfiguration* OobeConfiguration::instance = nullptr;

OobeConfiguration::OobeConfiguration()
    : configuration_(
          std::make_unique<base::Value>(base::Value::Type::DICTIONARY)),
      weak_factory_(this) {
  DCHECK(!OobeConfiguration::Get());
  OobeConfiguration::instance = this;
}

OobeConfiguration::~OobeConfiguration() {
  DCHECK_EQ(instance, this);
  OobeConfiguration::instance = nullptr;
}

// static
OobeConfiguration* OobeConfiguration::Get() {
  return OobeConfiguration::instance;
}

void OobeConfiguration::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void OobeConfiguration::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

const base::Value& OobeConfiguration::GetConfiguration() const {
  return *configuration_.get();
}

void OobeConfiguration::ResetConfiguration() {
  configuration_ = std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  for (auto& observer : observer_list_)
    observer.OnOobeConfigurationChanged();
}

void OobeConfiguration::CheckConfiguration() {
  DBusThreadManager::Get()
      ->GetOobeConfigurationClient()
      ->CheckForOobeConfiguration(
          base::BindOnce(&OobeConfiguration::OnConfigurationCheck,
                         weak_factory_.GetWeakPtr()));
}

void OobeConfiguration::OnConfigurationCheck(bool has_configuration,
                                             const std::string& configuration) {
  if (!has_configuration)
    return;

  int error_code, row, col;
  std::string error_message;
  auto value = base::JSONReader::ReadAndReturnError(
      configuration, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS,
      &error_code, &error_message, &row, &col);
  if (!value || !value->is_dict()) {
    LOG(ERROR) << "Error parsing OOBE configuration: " << error_message;
    return;
  }

  configuration_ = std::move(value);
  for (auto& observer : observer_list_)
    observer.OnOobeConfigurationChanged();
}

}  // namespace chromeos
