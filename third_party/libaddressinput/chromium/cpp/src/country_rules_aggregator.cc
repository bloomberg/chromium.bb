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
#include <libaddressinput/util/basictypes.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <cassert>
#include <cstddef>
#include <map>
#include <string>
#include <utility>

#include "retriever.h"
#include "rule.h"
#include "ruleset.h"
#include "util/stl_util.h"

namespace i18n {
namespace addressinput {

// Information about data requests sent to Retriever. This data is not returned
// as part of the ruleset, but is useful in constructing the ruleset
// asynchronously.
struct CountryRulesAggregator::RequestData {
  // Does not take ownership of |parent|.
  RequestData(Ruleset* parent,
              AddressField level,
              bool is_language_code,
              const std::string& id)
      : parent(parent),
        level(level),
        is_language_code(is_language_code),
        id(id) {
    assert(parent != NULL || level == COUNTRY);
  }

  ~RequestData() {}

  // The parent ruleset of the data being downloaded. If NULL, then this is the
  // root ruleset at the COUNTRY level. The language-specific and sub-region
  // rules are added to this ruleset. Owned by |CountryRulesRetriever|.
  Ruleset* parent;

  // The level of the data being requested. Ranges from COUNTRY to
  // DEPENDENT_LOCALITY. If COUNTRY, then the rule should use default rules from
  // Rule::GetDefault().
  AddressField level;

  // If true, then |id| is a language. The data received for this request should
  // be placed into a language-specific rule.
  bool is_language_code;

  // Can be a region name (e.g. "CA") or a language (e.g. "fr"). Used to add a
  // sub-region or a language-specific rule to |parent|.
  std::string id;
};

CountryRulesAggregator::CountryRulesAggregator(scoped_ptr<Retriever> retriever)
    : retriever_(retriever.Pass()),
      requests_(),
      country_code_(),
      rules_ready_(),
      root_(),
      default_language_(),
      languages_() {
  assert(retriever_ != NULL);
}

CountryRulesAggregator::~CountryRulesAggregator() {}

void CountryRulesAggregator::AggregateRules(const std::string& country_code,
                                            scoped_ptr<Callback> rules_ready) {
  Reset();
  country_code_ = country_code;
  rules_ready_.reset(rules_ready.release());

  // Key construction:
  // https://code.google.com/p/libaddressinput/wiki/AddressValidationMetadata
  // Example of a country-level key: "data/CA".
  std::string key = "data/" + country_code_;
  requests_.insert(std::make_pair(
      key, RequestData(NULL, COUNTRY, false, std::string())));

  retriever_->Retrieve(
      key, BuildCallback(this, &CountryRulesAggregator::OnDataReady));
}

void CountryRulesAggregator::OnDataReady(bool success,
                                         const std::string& key,
                                         const std::string& data) {
  std::map<std::string, RequestData>::iterator request_it =
      requests_.find(key);
  if (request_it == requests_.end()) {
    return;  // An abandoned request.
  }

  if (!success) {
    (*rules_ready_)(false, country_code_, scoped_ptr<Ruleset>());
    Reset();
    return;
  }

  RequestData request = request_it->second;
  requests_.erase(request_it);

  // All country-level rules are based on the default rule.
  scoped_ptr<Rule> rule(new Rule);
  if (request.level == COUNTRY) {
    rule->CopyFrom(Rule::GetDefault());
  }

  if (!rule->ParseSerializedRule(data)) {
    (*rules_ready_)(false, country_code_, scoped_ptr<Ruleset>());
    Reset();
    return;
  }

  // Place the rule in the correct location in the ruleset.
  Ruleset* ruleset = NULL;
  if (request.is_language_code) {
    assert(request.parent != NULL);
    request.parent->AddLanguageCodeRule(request.id, rule.Pass());
    ruleset = request.parent;
  } else if (request.level == COUNTRY) {
    // The default language and all supported languages for the country code are
    // in the country-level rule without a language code identifier. For
    // example: "data/CA".
    default_language_ = rule->GetLanguage();
    languages_ = rule->GetLanguages();

    root_.reset(new Ruleset(request.level, rule.Pass()));
    ruleset = root_.get();
  } else {
    assert(request.parent != NULL);
    ruleset = new Ruleset(request.level, rule.Pass());
    request.parent->AddSubRegionRuleset(
        request.id, scoped_ptr<Ruleset>(ruleset));
  }

  if (!request.is_language_code) {
    // Retrieve the language-specific rules for this region.
    for (std::vector<std::string>::const_iterator
             lang_it = languages_.begin();
         lang_it != languages_.end();
         ++lang_it) {
      if (*lang_it == default_language_) {
        continue;
      }
      // Example of a language-specific key: "data/CA--fr".
      std::string language_code_key = key + "--" + *lang_it;
      requests_.insert(std::make_pair(
          key, RequestData(ruleset, request.level, true, *lang_it)));
      retriever_->Retrieve(
          language_code_key,
          BuildCallback(this, &CountryRulesAggregator::OnDataReady));
    }

    if (request.level < DEPENDENT_LOCALITY) {
      // Retrieve the sub-region rules for this region.
      for (std::vector<std::string>::const_iterator
               subkey_it = ruleset->rule().GetSubKeys().begin();
           subkey_it != ruleset->rule().GetSubKeys().end();
           ++subkey_it) {
        // Example of a sub-region key: "data/CA/AB".
        std::string sub_region_key = key + "/" + *subkey_it;
        requests_.insert(std::make_pair(
            key,
            RequestData(ruleset,
                        static_cast<AddressField>(request.level + 1),
                        false,
                        *subkey_it)));
        retriever_->Retrieve(
            sub_region_key,
            BuildCallback(this, &CountryRulesAggregator::OnDataReady));
      }
    }
  }

  if (requests_.empty()) {
    (*rules_ready_)(true, country_code_, root_.Pass());
    Reset();
  }
}

void CountryRulesAggregator::Reset() {
  requests_.clear();
  country_code_.clear();
  rules_ready_.reset();
  root_.reset();
  default_language_.clear();
  languages_.clear();
}

}  // namespace addressinput
}  // namespace i18n
