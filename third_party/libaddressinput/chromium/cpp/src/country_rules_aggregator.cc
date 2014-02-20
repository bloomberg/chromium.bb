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

  scoped_ptr<Ruleset> ruleset = Build(key_, COUNTRY);
  const bool parse_success = ruleset != NULL;
  (*rules_ready_)(parse_success, country_code_, ruleset.Pass());
  Reset();
}

scoped_ptr<Ruleset> CountryRulesAggregator::Build(const std::string& key,
                                                  AddressField field) {
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

  // Parse the language-specific rules. For example, parse the rules for "fr"
  // and "it" languages in Switzerland.
  for (std::vector<std::string>::const_iterator
           lang_it = non_default_languages_.begin();
       lang_it != non_default_languages_.end(); ++lang_it) {
    scoped_ptr<Rule> lang_rule = ParseRule(key + "--" + *lang_it, field);
    if (lang_rule == NULL) {
      return scoped_ptr<Ruleset>();
    }
    ruleset->AddLanguageCodeRule(*lang_it, lang_rule.Pass());
  }

  // Parse the sub-keys. For example, parse the rules for all of the states in
  // US: "CA", "TX", "NY", etc.
  for (std::vector<std::string>::const_iterator
           subkey_it = ruleset->rule().GetSubKeys().begin();
       subkey_it != ruleset->rule().GetSubKeys().end(); ++subkey_it) {
    scoped_ptr<Ruleset> sub_ruleset =
        Build(key + "/" + *subkey_it, static_cast<AddressField>(field + 1));
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
