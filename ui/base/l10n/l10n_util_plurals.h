// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with pluralization of
// localized content.

#ifndef UI_BASE_L10N_L10N_UTIL_PLURALS_H_
#define UI_BASE_L10N_L10N_UTIL_PLURALS_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "third_party/icu/source/i18n/unicode/msgfmt.h"
#include "third_party/icu/source/i18n/unicode/plurrule.h"

namespace l10n_util {

// Returns a PluralRules for the current locale.
scoped_ptr<icu::PluralRules> BuildPluralRules();

// Formats |number| using MessageFormat |format| and appends the result
// to |append_to|.
// |format| has to be generated with ICU's plural message format.
// See http://userguide.icu-project.org/formatparse/messages
void FormatNumberInPlural(const icu::MessageFormat& format, int number,
                          icu::UnicodeString* append_to, UErrorCode* err);

}  // namespace l10n_util

#endif  // UI_BASE_L10N_L10N_UTIL_PLURALS_H_
