// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ACCESSORY_SHEET_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ACCESSORY_SHEET_DATA_H_

#include <vector>

#include "base/strings/string16.h"

namespace autofill {

// Represents user data to be shown on the manual fallback UI (e.g. a Profile,
// or a Credit Card, or the credentials for a website).
class UserInfo {
 public:
  // Represents a selectable item, such as the username or a credit card
  // number.
  class Field {
   public:
    Field(const base::string16& display_text,
          const base::string16& a11y_description,
          bool is_obfuscated,
          bool selectable);
    Field(const Field& field);
    Field(Field&& field);

    ~Field();

    Field& operator=(const Field& field);
    Field& operator=(Field&& field);

    const base::string16& display_text() const { return display_text_; }

    const base::string16& a11y_description() const { return a11y_description_; }

    bool is_obfuscated() const { return is_obfuscated_; }

    bool selectable() const { return selectable_; }

    bool operator==(const UserInfo::Field& field) const;

   private:
    base::string16 display_text_;
    base::string16 a11y_description_;
    bool is_obfuscated_;
    bool selectable_;
  };

  UserInfo();
  UserInfo(const UserInfo& user_info);
  UserInfo(UserInfo&& field);

  ~UserInfo();

  UserInfo& operator=(const UserInfo& user_info);
  UserInfo& operator=(UserInfo&& user_info);

  void add_field(Field field) { fields_.emplace_back(std::move(field)); }

  const std::vector<Field>& fields() const { return fields_; }

  bool operator==(const UserInfo& user_info) const;

 private:
  std::vector<Field> fields_;
};

// Represents a command below the suggestions, such as "Manage password...".
class FooterCommand {
 public:
  explicit FooterCommand(const base::string16& display_text);
  FooterCommand(const FooterCommand& footer_command);
  FooterCommand(FooterCommand&& footer_command);

  ~FooterCommand();

  FooterCommand& operator=(const FooterCommand& footer_command);
  FooterCommand& operator=(FooterCommand&& footer_command);

  const base::string16& display_text() const { return display_text_; }

  bool operator==(const FooterCommand& fc) const;

 private:
  base::string16 display_text_;
};

// Represents the contents of a bottom sheet tab below the keyboard accessory,
// which can correspond to passwords, credit cards, or profiles data.
//
// TODO(crbug.com/902425): Add a field to indicate if this corresponds to
//                         password, profile, or credit card data.
class AccessorySheetData {
 public:
  explicit AccessorySheetData(const base::string16& title);
  AccessorySheetData(const AccessorySheetData& data);
  AccessorySheetData(AccessorySheetData&& data);

  ~AccessorySheetData();

  AccessorySheetData& operator=(const AccessorySheetData& data);
  AccessorySheetData& operator=(AccessorySheetData&& data);

  const base::string16& title() const { return title_; }

  void add_user_info(UserInfo user_info) {
    user_info_list_.emplace_back(std::move(user_info));
  }

  const std::vector<UserInfo>& user_info_list() const {
    return user_info_list_;
  }

  std::vector<UserInfo>& mutable_user_info_list() { return user_info_list_; }

  void add_footer_command(FooterCommand footer_command) {
    footer_commands_.emplace_back(std::move(footer_command));
  }

  const std::vector<FooterCommand>& footer_commands() const {
    return footer_commands_;
  }

  bool operator==(const AccessorySheetData& data) const;

 private:
  base::string16 title_;
  std::vector<UserInfo> user_info_list_;
  std::vector<FooterCommand> footer_commands_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ACCESSORY_SHEET_DATA_H_
