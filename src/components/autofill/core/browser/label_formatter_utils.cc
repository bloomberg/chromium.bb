// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/label_formatter_utils.h"

#include <memory>
#include <set>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_form_label_formatter.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/contact_form_label_formatter.h"
#include "components/autofill/core/browser/validation.h"

namespace autofill {
namespace {

bool ContainsName(uint32_t groups) {
  return groups & label_formatter_groups::kName;
}

bool ContainsAddress(uint32_t groups) {
  return groups & label_formatter_groups::kAddress;
}

bool ContainsEmail(uint32_t groups) {
  return groups & label_formatter_groups::kEmail;
}

bool ContainsPhone(uint32_t groups) {
  return groups & label_formatter_groups::kPhone;
}

std::set<FieldTypeGroup> GetFieldTypeGroups(uint32_t groups) {
  std::set<FieldTypeGroup> field_type_groups;
  if (ContainsName(groups)) {
    field_type_groups.insert(NAME);
  }
  if (ContainsAddress(groups)) {
    field_type_groups.insert(ADDRESS_HOME);
  }
  if (ContainsEmail(groups)) {
    field_type_groups.insert(EMAIL);
  }
  if (ContainsPhone(groups)) {
    field_type_groups.insert(PHONE_HOME);
  }
  return field_type_groups;
}

}  // namespace

std::vector<ServerFieldType> FilterFieldTypes(
    const std::vector<ServerFieldType>& field_types) {
  std::vector<ServerFieldType> filtered_field_types;
  for (const ServerFieldType& field_type : field_types) {
    const FieldTypeGroup group =
        AutofillType(AutofillType(field_type).GetStorableType()).group();
    if (group == NAME || group == ADDRESS_HOME || group == EMAIL ||
        group == PHONE_HOME) {
      filtered_field_types.push_back(field_type);
    }
  }
  return filtered_field_types;
}

uint32_t DetermineGroups(const std::vector<ServerFieldType>& field_types) {
  uint32_t group_bitmask = 0;
  for (const ServerFieldType& type : field_types) {
    const FieldTypeGroup group =
        AutofillType(AutofillType(type).GetStorableType()).group();
    switch (group) {
      case autofill::NAME:
        group_bitmask |= label_formatter_groups::kName;
        break;
      case autofill::ADDRESS_HOME:
        group_bitmask |= label_formatter_groups::kAddress;
        break;
      case autofill::EMAIL:
        group_bitmask |= label_formatter_groups::kEmail;
        break;
      case autofill::PHONE_HOME:
        group_bitmask |= label_formatter_groups::kPhone;
        break;
      default:
        break;
    }
  }
  return group_bitmask;
}

std::unique_ptr<LabelFormatter> Create(
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    const std::vector<ServerFieldType>& field_types) {
  const uint32_t groups = DetermineGroups(field_types);

  if (!ContainsName(groups)) {
    return nullptr;
  }
  if (ContainsAddress(groups) && !ContainsEmail(groups) &&
      !ContainsPhone(groups)) {
    return std::make_unique<AddressFormLabelFormatter>(
        app_locale, focused_field_type, FilterFieldTypes(field_types));
  }
  if (ContainsEmail(groups) || ContainsPhone(groups)) {
    return std::make_unique<ContactFormLabelFormatter>(
        app_locale, focused_field_type, FilterFieldTypes(field_types),
        GetFieldTypeGroups(groups));
  }
  return nullptr;
}

}  // namespace autofill
