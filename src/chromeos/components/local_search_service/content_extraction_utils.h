// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_CONTENT_EXTRACTION_UTILS_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_CONTENT_EXTRACTION_UTILS_H_

#include "base/strings/string16.h"
#include "chromeos/components/local_search_service/shared_structs.h"

namespace chromeos {
namespace local_search_service {

// Given a list of tokens, returns a list of tokens where each token has a
// unique content.
std::vector<Token> ConsolidateToken(const std::vector<Token>& tokens);

// Given a text document, returns a list of Tokens.
// Locale should be obtained using this function:
// base::i18n::GetConfiguredLocale(). The format of locale will be
// language-country@variant. Country and variant are optional.
std::vector<Token> ExtractContent(const std::string& content_id,
                                  const base::string16& text,
                                  double weight,
                                  const std::string& locale);

// Checks if the locale is non Latin locales.
// Locale should be obtained using this function:
// base::i18n::GetConfiguredLocale(). The format of locale will be
// language-country@variant. Country and variant are optional.
bool IsNonLatinLocale(const std::string& locale);

// Checks if a word is a stopword given a locale. Locale will be in the
// following format: language-country@variant (country and variant are
// optional).
bool IsStopword(const base::string16& word, const std::string& locale);

// Returns a normalized version of a string16: removes diacritics, convert to
// lower-case and possibly remove hyphen from the text (set to true by default).
base::string16 Normalizer(const base::string16& word,
                          bool remove_hyphen = true);

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_CONTENT_EXTRACTION_UTILS_H_
