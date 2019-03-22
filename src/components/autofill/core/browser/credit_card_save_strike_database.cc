// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/credit_card_save_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"

namespace autofill {

CreditCardSaveStrikeDatabase::CreditCardSaveStrikeDatabase(
    const base::FilePath& database_dir)
    : StrikeDatabase(database_dir) {}

CreditCardSaveStrikeDatabase::~CreditCardSaveStrikeDatabase() {}

std::string CreditCardSaveStrikeDatabase::GetProjectPrefix() {
  return "CreditCardSave";
}

int CreditCardSaveStrikeDatabase::GetMaxStrikesLimit() {
  return 3;
}

}  // namespace autofill
