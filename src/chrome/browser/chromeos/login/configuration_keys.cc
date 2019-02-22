// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/configuration_keys.h"

namespace chromeos {
namespace configuration {

// Configuration keys that are used to automate OOBE screens go here.
// Please keep keys grouped by screens and ordered according to OOBE flow.
// All keys should be listed here, even if they are used in JS code only.
// These keys are used in chrome/browser/resources/chromeos/login/oobe_types.js

// == Welcome screen:

// Boolean value indicating if "Next" button on welcome screen is pressed
// automatically.
const char kWelcomeNext[] = "welcomeNext";

// == Network screen:

// String value specifying GUID of the network that would be automatically
// selected.
const char kNetworkSelectGUID[] = "networkSelectGuid";

// == EULA screen:

// Boolean value indicating if device should send usage statistics.
const char kEULASendUsageStatistics[] = "eulaSendStatistics";

// Boolean value indicating if the EULA is automatically accepted.
const char kEULAAutoAccept[] = "eulaAutoAccept";

// == Update screen:

// Boolean value, indicating that all non-critical updates should be skipped.
const char kUpdateSkipUpdate[] = "updateSkipNonCritical";

// == Wizard controller:

// Boolean value, controls if WizardController should automatically start
// enrollment at appropriate moment.
const char kWizardAutoEnroll[] = "wizardAutoEnroll";

// String value, containing device requisition parameter.
const char kDeviceRequisition[] = "deviceRequisition";

// == Enrollment screen

// String value indicating which license type should automatically be used if
// license selection is done on a client side.
const char kEnrollmentLicenseType[] = "enrollmentLicenseType";

// String value indicating what value would be propagated to Asset ID field
// on Device Attributes step.
const char kEnrollmentAssetId[] = "enrollmentAssetId";

// String value indicating what value would be propagated to Location field
// on Device Attributes step.
const char kEnrollmentLocation[] = "enrollmentLocation";

// Boolean value, controls if device attributes step should proceed with preset
// values.
const char kEnrollmentAutoAttributes[] = "enrollmentAutoAttributes";

using ValueType = base::Value::Type;

constexpr struct {
  const char* key;
  ValueType type;
  ConfigurationHandlerSide side;
} kAllConfigurationKeys[] = {
    {kWelcomeNext, ValueType::BOOLEAN, ConfigurationHandlerSide::HANDLER_JS},
    {kNetworkSelectGUID, ValueType::STRING,
     ConfigurationHandlerSide::HANDLER_JS},
    {kEULASendUsageStatistics, ValueType::BOOLEAN,
     ConfigurationHandlerSide::HANDLER_JS},
    {kEULAAutoAccept, ValueType::BOOLEAN, ConfigurationHandlerSide::HANDLER_JS},
    {kUpdateSkipUpdate, ValueType::BOOLEAN,
     ConfigurationHandlerSide::HANDLER_JS},
    {kWizardAutoEnroll, ValueType::BOOLEAN,
     ConfigurationHandlerSide::HANDLER_CPP},
    {kDeviceRequisition, ValueType::STRING,
     ConfigurationHandlerSide::HANDLER_CPP},
    {kEnrollmentLicenseType, ValueType::STRING,
     ConfigurationHandlerSide::HANDLER_CPP},
    {kEnrollmentLocation, ValueType::STRING,
     ConfigurationHandlerSide::HANDLER_CPP},
    {kEnrollmentLocation, ValueType::BOOLEAN,
     ConfigurationHandlerSide::HANDLER_CPP},
    {"desc", ValueType::STRING, ConfigurationHandlerSide::HANDLER_DOC},
    {"testValue", ValueType::STRING, ConfigurationHandlerSide::HANDLER_BOTH},
};

bool ValidateConfiguration(const base::Value& configuration) {
  if (configuration.type() != ValueType::DICTIONARY) {
    LOG(ERROR) << "Configuration should be a dictionary";
    return false;
  }
  base::Value clone = configuration.Clone();
  bool valid = true;
  for (const auto& key : kAllConfigurationKeys) {
    auto* value = clone.FindKey(key.key);
    if (value) {
      if (value->type() != key.type) {
        valid = false;
        LOG(ERROR) << "Invalid configuration: key " << key.key
                   << " type is invalid";
      }
      clone.RemoveKey(key.key);
    }
  }
  valid = valid && clone.DictEmpty();
  for (const auto& item : clone.DictItems()) {
    LOG(ERROR) << "Unknown configuration key " << item.first;
  }
  return valid;
}

void FilterConfiguration(const base::Value& configuration,
                         ConfigurationHandlerSide side,
                         base::Value& filtered_result) {
  DCHECK(side == ConfigurationHandlerSide::HANDLER_CPP ||
         side == ConfigurationHandlerSide::HANDLER_JS);
  for (const auto& key : kAllConfigurationKeys) {
    if (key.side == side ||
        key.side == ConfigurationHandlerSide::HANDLER_BOTH) {
      auto* value = configuration.FindKey(key.key);
      if (value) {
        filtered_result.SetKey(key.key, value->Clone());
      }
    }
  }
}

}  // namespace configuration
}  // namespace chromeos
