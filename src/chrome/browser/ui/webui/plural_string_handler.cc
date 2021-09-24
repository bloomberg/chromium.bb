// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/plural_string_handler.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

PluralStringHandler::PluralStringHandler() {}

PluralStringHandler::~PluralStringHandler() {}

void PluralStringHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "getPluralString",
      base::BindRepeating(&PluralStringHandler::HandleGetPluralString,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "getPluralStringTupleWithComma",
      base::BindRepeating(
          &PluralStringHandler::HandleGetPluralStringTupleWithComma,
          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "getPluralStringTupleWithPeriods",
      base::BindRepeating(
          &PluralStringHandler::HandleGetPluralStringTupleWithPeriods,
          base::Unretained(this)));
}

void PluralStringHandler::AddLocalizedString(const std::string& name, int id) {
  name_to_id_[name] = id;
}

void PluralStringHandler::HandleGetPluralString(const base::ListValue* args) {
  AllowJavascript();
  const auto& list = args->GetList();
  CHECK_EQ(3U, list.size());

  const base::Value& callback_id = list[0];
  std::string message_name = list[1].GetString();
  int count = list[2].GetInt();

  auto string = GetPluralizedStringForMessageName(message_name, count);

  ResolveJavascriptCallback(callback_id, base::Value(string));
}

void PluralStringHandler::HandleGetPluralStringTupleWithComma(
    const base::ListValue* args) {
  GetPluralStringTuple(args, IDS_CONCAT_TWO_STRINGS_WITH_COMMA);
}

void PluralStringHandler::HandleGetPluralStringTupleWithPeriods(
    const base::ListValue* args) {
  GetPluralStringTuple(args, IDS_CONCAT_TWO_STRINGS_WITH_PERIODS);
}

void PluralStringHandler::GetPluralStringTuple(const base::ListValue* args,
                                               int string_tuple_id) {
  AllowJavascript();
  const auto& list = args->GetList();
  CHECK_EQ(5U, list.size());

  const base::Value& callback_id = list[0];
  std::string message_name1 = list[1].GetString();
  int count1 = list[2].GetInt();
  std::string message_name2 = list[3].GetString();
  int count2 = list[4].GetInt();

  auto string1 = GetPluralizedStringForMessageName(message_name1, count1);
  auto string2 = GetPluralizedStringForMessageName(message_name2, count2);

  ResolveJavascriptCallback(
      callback_id, base::Value(l10n_util::GetStringFUTF8(string_tuple_id,
                                                         string1, string2)));
}

std::u16string PluralStringHandler::GetPluralizedStringForMessageName(
    std::string message_name,
    int count) {
  auto message_id_it = name_to_id_.find(message_name);
  CHECK(name_to_id_.end() != message_id_it);
  return l10n_util::GetPluralStringFUTF16(message_id_it->second, count);
}
