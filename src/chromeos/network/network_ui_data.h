// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_UI_DATA_H_
#define CHROMEOS_NETWORK_NETWORK_UI_DATA_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "components/onc/onc_constants.h"

namespace base {
class DictionaryValue;
class Value;
}  // namespace base

namespace chromeos {

// Helper for accessing and setting values in the network's UI data dictionary.
// Accessing values is done via static members that take the network as an
// argument.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkUIData {
 public:
  NetworkUIData();
  NetworkUIData(const NetworkUIData& other);
  NetworkUIData& operator=(const NetworkUIData& other);
  explicit NetworkUIData(const base::Value& dict);
  ~NetworkUIData();

  // Creates a NetworkUIData object from |onc_source|. This function is used to
  // create the "UIData" property of the Shill configuration.
  static std::unique_ptr<NetworkUIData> CreateFromONC(
      ::onc::ONCSource onc_source);

  // Returns a |user_settings_| as a DictionaryValue*.
  const base::DictionaryValue* GetUserSettingsDictionary() const;

  // Setus |user_settings_| to the provided value which must be a dictionary.
  void SetUserSettingsDictionary(std::unique_ptr<base::Value> dict);

  // Returns a JSON string representing currently configured values for storing
  // in Shill.
  std::string GetAsJson() const;

  ::onc::ONCSource onc_source() const { return onc_source_; }

 private:
  std::string GetONCSourceAsString() const;

  ::onc::ONCSource onc_source_;
  std::unique_ptr<base::Value> user_settings_;
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_UI_DATA_H_
