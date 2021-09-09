// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_language_blocklist_policy_handler.h"

#include <unordered_set>
#include <utility>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/strings/grit/components_strings.h"

SpellcheckLanguageBlocklistPolicyHandler::
    SpellcheckLanguageBlocklistPolicyHandler(const char* policy_name)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST) {}

SpellcheckLanguageBlocklistPolicyHandler::
    ~SpellcheckLanguageBlocklistPolicyHandler() = default;

bool SpellcheckLanguageBlocklistPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value = nullptr;
  bool ok = CheckAndGetValue(policies, errors, &value);

  std::vector<base::Value> blocklisted;
  std::vector<std::string> unknown;
  std::vector<std::string> duplicates;
  SortBlocklistedLanguages(policies, &blocklisted, &unknown, &duplicates);

#if !defined(OS_MAC)
  for (const std::string& language : duplicates) {
    errors->AddError(policy_name(), IDS_POLICY_SPELLCHECK_BLOCKLIST_IGNORE,
                     language);
  }

  for (const std::string& language : unknown) {
    errors->AddError(policy_name(), IDS_POLICY_SPELLCHECK_UNKNOWN_LANGUAGE,
                     language);
  }
#endif

  return ok;
}

void SpellcheckLanguageBlocklistPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  // Ignore this policy if the SpellcheckEnabled policy disables spellcheck.
  const base::Value* spellcheck_enabled_value =
      policies.GetValue(policy::key::kSpellcheckEnabled);
  if (spellcheck_enabled_value && spellcheck_enabled_value->GetBool() == false)
    return;

  // If this policy isn't set, don't modify spellcheck languages.
  const base::Value* value = policies.GetValue(policy_name());
  if (!value)
    return;

  // Set the blocklisted dictionaries preference based on this policy's values,
  // and emit warnings for unknown or duplicate languages.
  std::vector<base::Value> blocklisted;
  std::vector<std::string> unknown;
  std::vector<std::string> duplicates;
  SortBlocklistedLanguages(policies, &blocklisted, &unknown, &duplicates);

  for (const std::string& language : duplicates) {
    SYSLOG(WARNING)
        << "SpellcheckLanguageBlocklist policy: an entry was also found in"
           " the SpellcheckLanguage policy: \""
        << language << "\". Blocklist entry will be ignored.";
  }

  for (const std::string& language : unknown) {
    SYSLOG(WARNING) << "SpellcheckLanguageBlocklist policy: Unknown or "
                       "unsupported language \""
                    << language << "\"";
  }

  prefs->SetValue(spellcheck::prefs::kSpellCheckBlocklistedDictionaries,
                  base::Value(std::move(blocklisted)));
}

void SpellcheckLanguageBlocklistPolicyHandler::SortBlocklistedLanguages(
    const policy::PolicyMap& policies,
    std::vector<base::Value>* const blocklisted,
    std::vector<std::string>* const unknown,
    std::vector<std::string>* const duplicates) {
  const base::Value* value = policies.GetValue(policy_name());
  if (!value)
    return;

  // Build a lookup of force-enabled spellcheck languages to find duplicates.
  const base::Value* forced_enabled_value =
      policies.GetValue(policy::key::kSpellcheckLanguage);
  std::unordered_set<std::string> forced_languages_lookup;
  if (forced_enabled_value) {
    for (const auto& forced_language : forced_enabled_value->GetList())
      forced_languages_lookup.insert(forced_language.GetString());
  }

  // Separate the valid languages from the unknown / unsupported languages and
  // the languages that also appear in the SpellcheckLanguage policy.
  for (const base::Value& language : value->GetList()) {
    std::string candidate_language(
        base::TrimWhitespaceASCII(language.GetString(), base::TRIM_ALL));
    std::string current_language =
        SpellcheckService::GetSupportedAcceptLanguageCode(candidate_language);

    if (current_language.empty()) {
      unknown->emplace_back(language.GetString());
    } else {
      if (forced_languages_lookup.find(language.GetString()) !=
          forced_languages_lookup.end()) {
        // If a language is both force-enabled and force-disabled, force-enable
        // wins. Put the language in the list of duplicates.
        duplicates->emplace_back(std::move(current_language));
      } else {
        blocklisted->emplace_back(std::move(current_language));
      }
    }
  }
}
