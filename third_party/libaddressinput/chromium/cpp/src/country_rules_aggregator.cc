// Copyright (C) 2014 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "country_rules_aggregator.h"

#include <libaddressinput/address_field.h>
#include <libaddressinput/callback.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "retriever.h"
#include "rule.h"
#include "ruleset.h"
#include "util/json.h"

namespace i18n {
namespace addressinput {

CountryRulesAggregator::CountryRulesAggregator(scoped_ptr<Retriever> retriever)
    : retriever_(retriever.Pass()),
      country_code_(),
      key_(),
      rules_ready_(),
      json_(),
      non_default_languages_() {
  assert(retriever_ != NULL);
}

CountryRulesAggregator::~CountryRulesAggregator() {}

void CountryRulesAggregator::AggregateRules(const std::string& country_code,
                                            scoped_ptr<Callback> rules_ready) {
  Reset();
  country_code_ = country_code;
  rules_ready_ = rules_ready.Pass();

  // Key construction:
  // https://code.google.com/p/libaddressinput/wiki/AddressValidationMetadata
  // Example of a country-level key: "data/CA".
  key_ = "data/" + country_code_;
  retriever_->Retrieve(
      key_, BuildCallback(this, &CountryRulesAggregator::OnDataReady));
}

void CountryRulesAggregator::OnDataReady(bool success,
                                         const std::string& key,
                                         const std::string& data) {
  if (key != key_) {
    return;  // An abandoned request.
  }

  json_ = Json::Build().Pass();
  if (!success || !json_->ParseObject(data)) {
    (*rules_ready_)(false, country_code_, scoped_ptr<Ruleset>());
    Reset();
    return;
  }

  std::map<std::string, std::string> language_keys;
  scoped_ptr<Ruleset> ruleset = Build(key_, COUNTRY, language_keys);
  const bool parse_success = ruleset != NULL;
  (*rules_ready_)(parse_success, country_code_, ruleset.Pass());
  Reset();
}

scoped_ptr<Ruleset> CountryRulesAggregator::Build(
    const std::string& key,
    AddressField field,
    const std::map<std::string, std::string>& language_specific_keys) {
  scoped_ptr<Rule> rule = ParseRule(key, field);
  if (rule == NULL) {
    return scoped_ptr<Ruleset>();
  }

  // Determine the languages that have language-specific rules. For example,
  // the default language in Switzerland is "de", but "fr" and "it" have
  // language specific rules.
  if (field == COUNTRY) {
    non_default_languages_ = rule->GetLanguages();
    std::vector<std::string>::iterator default_language_it =
        std::find(non_default_languages_.begin(),
                  non_default_languages_.end(),
                  rule->GetLanguage());
    if (default_language_it != non_default_languages_.end()) {
      non_default_languages_.erase(default_language_it);
    }
  }

  scoped_ptr<Ruleset> ruleset(new Ruleset(field, rule.Pass()));
  std::map<std::string, std::map<std::string, std::string> >
      language_specific_subkeys;

  // Parse the language-specific rules. For example, parse the rules for "fr"
  // and "it" languages in Switzerland.
  for (std::vector<std::string>::const_iterator
           non_default_language_it = non_default_languages_.begin();
       non_default_language_it != non_default_languages_.end();
       ++non_default_language_it) {
    std::map<std::string, std::string>::const_iterator
        language_specific_key_it =
            language_specific_keys.find(*non_default_language_it);
    std::string language_specific_key =
        language_specific_key_it != language_specific_keys.end()
            ? language_specific_key_it->second
            : key;
    scoped_ptr<Rule> language_specific_rule =
        ParseRule(language_specific_key + "--" + *non_default_language_it,
                  field);

    if (language_specific_rule == NULL ||
        language_specific_rule->GetSubKeys().size() !=
            ruleset->rule().GetSubKeys().size()) {
      return scoped_ptr<Ruleset>();
    }

    // Build the language specific subkeys for the next level of
    // rules. Examples:
    //    ["data/CA/AB"]["fr"] <- "data/CA/AB"
    //    ["data/HK/香港島"]["en"] <- "data/HK/Kowloon"
    for (size_t i = 0; i < ruleset->rule().GetSubKeys().size(); ++i) {
      const std::string& subkey = key + "/" + ruleset->rule().GetSubKeys()[i];
      const std::string& language_specific_subkey =
          key + "/" + language_specific_rule->GetSubKeys()[i];
      language_specific_subkeys[subkey][*non_default_language_it] =
          language_specific_subkey;
    }

    ruleset->AddLanguageCodeRule(
        *non_default_language_it, language_specific_rule.Pass());
  }

  // Parse the sub-keys recursively. For example, parse the rules for all of the
  // states in US: "CA", "TX", "NY", etc.
  for (std::vector<std::string>::const_iterator
           subkey_it = ruleset->rule().GetSubKeys().begin();
       subkey_it != ruleset->rule().GetSubKeys().end(); ++subkey_it) {
    std::string subkey = key + "/" + *subkey_it;
    scoped_ptr<Ruleset> sub_ruleset =
        Build(subkey,
              static_cast<AddressField>(field + 1),
              language_specific_subkeys[subkey]);

    if (sub_ruleset == NULL) {
      return scoped_ptr<Ruleset>();
    }

    ruleset->AddSubRegionRuleset(*subkey_it, sub_ruleset.Pass());
  }

  return ruleset.Pass();
}

scoped_ptr<Rule> CountryRulesAggregator::ParseRule(const std::string& key,
                                                   AddressField field) const {
  scoped_ptr<Json> value;
  if (!json_->GetJsonValueForKey(key, &value) || value == NULL) {
    return scoped_ptr<Rule>();
  }
  scoped_ptr<Rule> rule(new Rule);
  if (field == COUNTRY) {
    rule->CopyFrom(Rule::GetDefault());
  }
  rule->ParseJsonRule(*value);
  return rule.Pass();
}

void CountryRulesAggregator::Reset() {
  country_code_.clear();
  key_.clear();
  rules_ready_.reset();
  json_.reset();
  non_default_languages_.clear();
}

}  // namespace addressinput
}  // namespace i18n
