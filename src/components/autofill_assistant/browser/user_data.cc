// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_data.h"

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/password_form.h"

namespace autofill_assistant {

LoginChoice::LoginChoice(
    const std::string& _identifier,
    const std::string& _label,
    const std::string& _sublabel,
    const base::Optional<std::string>& _sublabel_accessibility_hint,
    int _preselect_priority,
    const base::Optional<InfoPopupProto>& _info_popup)
    : identifier(_identifier),
      label(_label),
      sublabel(_sublabel),
      sublabel_accessibility_hint(_sublabel_accessibility_hint),
      preselect_priority(_preselect_priority),
      info_popup(_info_popup) {}
LoginChoice::LoginChoice(const LoginChoice& another) = default;
LoginChoice::~LoginChoice() = default;

PaymentInstrument::PaymentInstrument() = default;
PaymentInstrument::PaymentInstrument(
    std::unique_ptr<autofill::CreditCard> _card,
    std::unique_ptr<autofill::AutofillProfile> _billing_address)
    : card(std::move(_card)), billing_address(std::move(_billing_address)) {}
PaymentInstrument::~PaymentInstrument() = default;

UserData::UserData() = default;
UserData::~UserData() = default;

CollectUserDataOptions::CollectUserDataOptions() = default;
CollectUserDataOptions::~CollectUserDataOptions() = default;

bool UserData::has_selected_address(const std::string& name) const {
  return selected_address(name) != nullptr;
}

bool UserData::has_additional_value(const std::string& key) const {
  return additional_values_.find(key) != additional_values_.end();
}

const autofill::AutofillProfile* UserData::selected_address(
    const std::string& name) const {
  auto it = selected_addresses_.find(name);
  if (it == selected_addresses_.end()) {
    return nullptr;
  }

  return it->second.get();
}

const ValueProto* UserData::additional_value(const std::string& key) const {
  auto it = additional_values_.find(key);
  if (it == additional_values_.end()) {
    return nullptr;
  }
  return &it->second;
}

std::string UserData::GetAllAddressKeyNames() const {
  std::vector<std::string> entries;
  for (const auto& entry : selected_addresses_) {
    entries.emplace_back(entry.first);
  }
  std::sort(entries.begin(), entries.end());
  return base::JoinString(entries, ",");
}
}  // namespace autofill_assistant
