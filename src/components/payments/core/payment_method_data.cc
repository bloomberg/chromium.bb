// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_method_data.h"

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/payment-method-basic-card/
static const char kMethodDataData[] = "data";
static const char kSupportedMethods[] = "supportedMethods";
static const char kSupportedNetworks[] = "supportedNetworks";

}  // namespace

PaymentMethodData::PaymentMethodData() {}
PaymentMethodData::PaymentMethodData(const PaymentMethodData& other) = default;
PaymentMethodData::~PaymentMethodData() = default;

bool PaymentMethodData::operator==(const PaymentMethodData& other) const {
  return supported_method == other.supported_method && data == other.data &&
         supported_networks == other.supported_networks;
}

bool PaymentMethodData::operator!=(const PaymentMethodData& other) const {
  return !(*this == other);
}

bool PaymentMethodData::FromDictionaryValue(
    const base::DictionaryValue& value) {
  supported_networks.clear();

  // The value of supportedMethods should be a string.
  if (!value.GetString(kSupportedMethods, &supported_method) ||
      !base::IsStringASCII(supported_method) || supported_method.empty()) {
    return false;
  }

  // Data is optional, but if a dictionary is present, save a stringified
  // version and attempt to parse supportedNetworks.
  const base::DictionaryValue* data_dict = nullptr;
  if (value.GetDictionary(kMethodDataData, &data_dict)) {
    std::string json_data;
    base::JSONWriter::Write(*data_dict, &json_data);
    data = json_data;
    const base::ListValue* supported_networks_list = nullptr;
    if (data_dict->GetList(kSupportedNetworks, &supported_networks_list)) {
      for (size_t i = 0; i < supported_networks_list->GetSize(); ++i) {
        std::string supported_network;
        if (!supported_networks_list->GetString(i, &supported_network) ||
            !base::IsStringASCII(supported_network)) {
          return false;
        }
        supported_networks.push_back(supported_network);
      }
    }
  }
  return true;
}

}  // namespace payments
