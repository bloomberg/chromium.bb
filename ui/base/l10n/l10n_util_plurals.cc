// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util_plurals.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/l10n/l10n_util.h"

namespace l10n_util {

scoped_ptr<icu::PluralRules> BuildPluralRules() {
  UErrorCode err = U_ZERO_ERROR;
  scoped_ptr<icu::PluralRules> rules(
      icu::PluralRules::forLocale(icu::Locale::getDefault(), err));
  if (U_FAILURE(err)) {
    err = U_ZERO_ERROR;
    icu::UnicodeString fallback_rules("one: n is 1", -1, US_INV);
    rules.reset(icu::PluralRules::createRules(fallback_rules, err));
    DCHECK(U_SUCCESS(err));
  }
  return rules.Pass();
}

void FormatNumberInPlural(const icu::MessageFormat& format, int number,
                          icu::UnicodeString* result, UErrorCode* err) {
  if (U_FAILURE(*err)) return;
  icu::Formattable formattable(number);
  icu::FieldPosition ignore(icu::FieldPosition::DONT_CARE);
  format.format(&formattable, 1, *result, ignore, *err);
  DCHECK(U_SUCCESS(*err));
  return;
}


}  // namespace l10n_util
