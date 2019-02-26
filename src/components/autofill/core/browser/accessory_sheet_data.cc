// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/accessory_sheet_data.h"

namespace autofill {

UserInfo::Field::Field(const base::string16& display_text,
                       const base::string16& a11y_description,
                       bool is_obfuscated,
                       bool selectable)
    : display_text_(display_text),
      a11y_description_(a11y_description),
      is_obfuscated_(is_obfuscated),
      selectable_(selectable) {}

UserInfo::Field::Field(const Field& field) = default;

UserInfo::Field::Field(Field&& field) = default;

UserInfo::Field::~Field() = default;

UserInfo::Field& UserInfo::Field::operator=(const Field& field) = default;

UserInfo::Field& UserInfo::Field::operator=(Field&& field) = default;

bool UserInfo::Field::operator==(const UserInfo::Field& field) const {
  return display_text_ == field.display_text_ &&
         a11y_description_ == field.a11y_description_ &&
         is_obfuscated_ == field.is_obfuscated_ &&
         selectable_ == field.selectable_;
}

UserInfo::UserInfo() = default;

UserInfo::UserInfo(const UserInfo& user_info) = default;

UserInfo::UserInfo(UserInfo&& field) = default;

UserInfo::~UserInfo() = default;

UserInfo& UserInfo::operator=(const UserInfo& user_info) = default;

UserInfo& UserInfo::operator=(UserInfo&& user_info) = default;

bool UserInfo::operator==(const UserInfo& user_info) const {
  return fields_ == user_info.fields_;
}

FooterCommand::FooterCommand(const base::string16& display_text)
    : display_text_(display_text) {}

FooterCommand::FooterCommand(const FooterCommand& footer_command) = default;

FooterCommand::FooterCommand(FooterCommand&& footer_command) = default;

FooterCommand::~FooterCommand() = default;

FooterCommand& FooterCommand::operator=(const FooterCommand& footer_command) =
    default;

FooterCommand& FooterCommand::operator=(FooterCommand&& footer_command) =
    default;

bool FooterCommand::operator==(const FooterCommand& fc) const {
  return display_text_ == fc.display_text_;
}

AccessorySheetData::AccessorySheetData(const base::string16& title)
    : title_(title) {}

AccessorySheetData::AccessorySheetData(const AccessorySheetData& data) =
    default;

AccessorySheetData::AccessorySheetData(AccessorySheetData&& data) = default;

AccessorySheetData::~AccessorySheetData() = default;

AccessorySheetData& AccessorySheetData::operator=(
    const AccessorySheetData& data) = default;

AccessorySheetData& AccessorySheetData::operator=(AccessorySheetData&& data) =
    default;

bool AccessorySheetData::operator==(const AccessorySheetData& data) const {
  return title_ == data.title_ && user_info_list_ == data.user_info_list_ &&
         footer_commands_ == data.footer_commands_;
}

}  // namespace autofill
