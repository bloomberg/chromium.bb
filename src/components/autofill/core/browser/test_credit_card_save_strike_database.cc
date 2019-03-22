// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_credit_card_save_strike_database.h"

namespace autofill {

TestCreditCardSaveStrikeDatabase::TestCreditCardSaveStrikeDatabase(
    const base::FilePath& database_dir)
    : CreditCardSaveStrikeDatabase(database_dir) {
  database_initialized_ = true;
}

}  // namespace autofill
