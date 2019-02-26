// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/identity_utils.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

namespace identity {

bool IsUsernameAllowedByPattern(base::StringPiece username,
                                base::StringPiece pattern) {
  if (pattern.empty())
    return true;

  // Patterns like "*@foo.com" are not accepted by our regex engine (since they
  // are not valid regular expressions - they should instead be ".*@foo.com").
  // For convenience, detect these patterns and insert a "." character at the
  // front.
  base::string16 utf16_pattern = base::UTF8ToUTF16(pattern);
  if (utf16_pattern[0] == L'*')
    utf16_pattern.insert(utf16_pattern.begin(), L'.');

  // See if the username matches the policy-provided pattern.
  UErrorCode status = U_ZERO_ERROR;
  const icu::UnicodeString icu_pattern(FALSE, utf16_pattern.data(),
                                       utf16_pattern.length());
  icu::RegexMatcher matcher(icu_pattern, UREGEX_CASE_INSENSITIVE, status);
  if (!U_SUCCESS(status)) {
    LOG(ERROR) << "Invalid login regex: " << utf16_pattern
               << ", status: " << status;
    // If an invalid pattern is provided, then prohibit *all* logins (better to
    // break signin than to quietly allow users to sign in).
    return false;
  }
  // The default encoding is UTF-8 in Chromium's ICU.
  icu::UnicodeString icu_input(username.data());
  matcher.reset(icu_input);
  status = U_ZERO_ERROR;
  UBool match = matcher.matches(status);
  DCHECK(U_SUCCESS(status));
  return !!match;  // !! == convert from UBool to bool.
}

bool LegacyIsUsernameAllowedByPatternFromPrefs(
    PrefService* prefs,
    const std::string& username,
    const std::string& pattern_pref_name) {
  // TODO(crbug.com/908121): We need to deal for now with the fact that most
  // unit tests don't register a local state with the browser process, in which
  // case all usernames are considered 'allowed'.
  if (!prefs)
    return true;

  return IsUsernameAllowedByPattern(username,
                                    prefs->GetString(pattern_pref_name));
}

}  // namespace identity
